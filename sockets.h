////////////////////////////////////////////////////////////////////////////////
// MIT License                                                                //
//                                                                            //
// Copyright (c) 2023 Erich Erstu                                             //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
////////////////////////////////////////////////////////////////////////////////

#ifndef SOCKETS_H_05_01_2023
#define SOCKETS_H_05_01_2023

#include <algorithm>
#include <limits>

#include <csignal>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <cstdio>

#include <sys/epoll.h>
#include <netdb.h>

class SOCKETS final {
    public:
    static constexpr const char *VERSION = "1.0";
    static constexpr const int NO_DESCRIPTOR = -1; // TODO: get rid of this

    enum class ERROR : uint8_t {
        NONE = 0,
        OUT_OF_MEMORY,
        UNDEFINED_BEHAVIOR,
        FORBIDDEN_CONDITION,
        UNHANDLED_EVENTS
    };

    static constexpr const ERROR
        ERR_NONE                = ERROR::NONE,
        ERR_OUT_OF_MEMORY       = ERROR::OUT_OF_MEMORY,
        ERR_UNDEFINED_BEHAVIOR  = ERROR::UNDEFINED_BEHAVIOR,
        ERR_FORBIDDEN_CONDITION = ERROR::FORBIDDEN_CONDITION,
        ERR_UNHANDLED_EVENTS    = ERROR::UNHANDLED_EVENTS;

    inline static const char *get_code(ERROR) noexcept;

    enum class EVENT : uint8_t {
        NONE = 0,
        // Do not change the order of the events above this line.
        READ,
        WRITE,
        ACCEPT,
        CONNECTION,
        DISCONNECTION,
        CLOSE,
        INCOMING,
        // Do not change the order of the events below this line.
        EPOLL,
        MAX_EVENTS
    };

    struct ALERT {
        int descriptor;
        EVENT event;
        bool valid:1;
    };

    static constexpr const EVENT
        EV_NONE          = EVENT::NONE,
        EV_CONNECTION    = EVENT::CONNECTION,
        EV_DISCONNECTION = EVENT::DISCONNECTION,
        EV_INCOMING      = EVENT::INCOMING;

    SOCKETS() noexcept;
    ~SOCKETS();

    bool init() noexcept;
    bool deinit() noexcept;

    void set_logger(void (*callback)(const char *) noexcept) noexcept;

    int listen(
        const char *port, int family =AF_UNSPEC, int flags =AI_PASSIVE
    ) noexcept;
    bool connect(const char *host, const char *port, int group =0) noexcept;
    bool idle() const noexcept;

    ERROR next_error(int timeout =-1) noexcept;
    ERROR last_error() noexcept;
    ALERT next_alert() noexcept;

    size_t incoming(int descriptor) const noexcept;
    size_t outgoing(int descriptor) const noexcept;
    size_t read(int descriptor, void *buf, size_t count) noexcept;
    const char *read(int descriptor) noexcept;
    ERROR write(int descriptor, const void *buf, size_t count) noexcept;
    ERROR write(int descriptor, const char *text) noexcept;
    ERROR writef(
        int descriptor, const char *fmt, ...
    ) noexcept __attribute__((format(printf, 3, 4)));

    [[nodiscard]] bool is_listener(int descriptor) const noexcept;
    [[nodiscard]] bool is_frozen(int descriptor) const noexcept;
    [[nodiscard]] int get_group(int descriptor) const noexcept;
    [[nodiscard]] size_t get_group_size(int group) const noexcept;
    [[nodiscard]] int get_listener(int descriptor) const noexcept;
    [[nodiscard]] const char *get_host(int descriptor) const noexcept;
    [[nodiscard]] const char *get_port(int descriptor) const noexcept;

    void freeze(int descriptor) noexcept;
    void unfreeze(int descriptor) noexcept;
    void disconnect(int descriptor) noexcept;

    private:
    enum class BUFFER : uint8_t {
        GENERIC_INT,
        GENERIC_BYTE,
        NEXT_ERROR,
        READ,
        WRITEF,
        HANDLE_READ,
        // Do not change the order of items below this line.
        MAX_BUFFERS
    };

    struct MEMORY {
        size_t   size;
        uint8_t *data;
        MEMORY  *next;
        MEMORY  *prev;
        bool indexed:1;
    };

    struct KEY {
        uintptr_t value;
    };

    struct PIPE {
        enum class TYPE : uint8_t {
            NONE = 0,
            UINT8,
            UINT64,
            INT,
            PTR,
            JACK_PTR,
            MEMORY_PTR,
            KEY,
            EPOLL_EVENT
        };

        struct ENTRY {
            union {
                uint8_t     as_uint8;
                uint64_t    as_uint64;
                int         as_int;
                void       *as_ptr;
                KEY         as_key;
                epoll_event as_epoll_event;
            };
            TYPE type;
        };

        size_t capacity;
        size_t size;
        void *data;
        TYPE type;
        MEMORY *memory;
    };

    struct INDEX {
        enum class TYPE : uint8_t {
            NONE = 0,
            // Do not change the order of the types above this line.
            GROUP_SIZE,
            EVENT_DESCRIPTOR,
            DESCRIPTOR_JACK,
            RESOURCE_MEMORY,
            // Do not change the order of the types below this line.
            MAX_TYPES
        };

        struct ENTRY {
            PIPE *key_pipe;
            PIPE *val_pipe;
            size_t index;
            ERROR error;
            bool valid:1;
        };

        size_t buckets;
        struct TABLE {
            PIPE key;
            PIPE value;
        } *table;
        TYPE type;
        bool multimap:1;
    };

    struct JACK {
        uint32_t event_lookup[
            static_cast<size_t>(EVENT::MAX_EVENTS) // TODO: improve type?
        ];
        PIPE epoll_ev;
        PIPE incoming;
        PIPE outgoing;
        char host[NI_MAXHOST]; // TODO: store in a PIPE instead
        char port[NI_MAXSERV]; // TODO: store in a PIPE instead
        int descriptor; // TODO: typedef descriptor_type and use it instead
        int parent;
        int group;
        int ai_family;
        int ai_flags;
        struct addrinfo *blacklist;
        struct bitset_type {
            bool frozen:1;
            bool connecting:1;
            bool may_shutdown:1;
            bool reconnect:1;
            bool listener:1;
        } bitset;
    };

    inline static constexpr KEY make_key(uintptr_t) noexcept;
    inline static constexpr KEY make_key(EVENT) noexcept;

    inline static constexpr struct JACK make_jack(
        int descriptor, int parent, int group
    ) noexcept;

    inline static constexpr struct ALERT make_alert(
        int descriptor, EVENT type, bool valid =true
    ) noexcept;

    inline static constexpr struct INDEX::ENTRY make_index_entry(
        PIPE &keys, PIPE &values, size_t index, ERROR, bool valid
    ) noexcept;

    inline static constexpr struct INDEX::ENTRY make_index_entry(
        PIPE &keys, PIPE &values, size_t index, ERROR
    ) noexcept;

    inline static constexpr PIPE make_pipe(
        const uint8_t *data, size_t size
    ) noexcept;

    inline static constexpr PIPE make_pipe(PIPE::TYPE) noexcept;

    inline static constexpr PIPE::ENTRY make_pipe_entry(PIPE::TYPE) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(uint64_t  ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(int       ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(KEY       ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(JACK *    ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(MEMORY *  ) noexcept;

    inline static constexpr epoll_data_t make_epoll_data(int fd) noexcept;
    inline static constexpr epoll_event make_epoll_event(
        int descriptor, uint32_t events
    ) noexcept;

    inline static bool is_listed(
        const addrinfo &info, const addrinfo *list
    ) noexcept;

    inline static EVENT next(EVENT) noexcept;

    ERROR handle_close (JACK &) noexcept;
    ERROR handle_epoll (JACK &, int timeout) noexcept;
    ERROR handle_read  (JACK &) noexcept;
    ERROR handle_write (JACK &) noexcept;
    ERROR handle_accept(JACK &) noexcept;

    int connect(
        const char *host, const char *port, int group, int family, int flags,
        const struct addrinfo *blacklist =nullptr,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    int listen(
        const char *host, const char *port, int family, int flags,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    void terminate(
        int descriptor,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    int next_connection() noexcept;
    int next_disconnection() noexcept;
    int next_incoming() noexcept;

    int create_epoll() noexcept;
    bool bind_to_epoll(int descriptor, int epoll_descriptor) noexcept;
    bool modify_epoll(int descriptor, uint32_t events) noexcept;

    int open_and_init(
        const char *host, const char *port, int family, int flags,
        const struct addrinfo *blacklist =nullptr,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    size_t close_and_deinit(int descriptor) noexcept;

    [[nodiscard]] ERROR capture(const JACK &copy) noexcept;
    void release(JACK *) noexcept;

    const JACK *find_jack(int descriptor) const noexcept;
    JACK *find_jack(int descriptor) noexcept;
    const JACK *find_jack(EVENT) const noexcept;
    JACK *find_jack(EVENT) noexcept;
    const JACK *find_epoll_jack() const noexcept;
    JACK *find_epoll_jack() noexcept;
    const JACK &get_jack(int descriptor) const noexcept;
    JACK &get_jack(int descriptor) noexcept;
    const JACK &get_epoll_jack() const noexcept;
    JACK &get_epoll_jack() noexcept;
    const PIPE *find_descriptors(EVENT) const noexcept;

    [[nodiscard]] ERROR set_group(int descriptor, int group) noexcept;
    void rem_group(int descriptor) noexcept;
    /*TODO: [[nodiscard]]*/ ERROR set_event(
        JACK &, EVENT, bool val =true
    ) noexcept;
    void rem_event(JACK &, EVENT) noexcept;
    bool has_event(const JACK &, EVENT) const noexcept;
    [[nodiscard]] bool is_listener(const JACK &) const noexcept;

    size_t count(INDEX::TYPE, KEY key) const noexcept;
    INDEX::ENTRY find(
        INDEX::TYPE, KEY key, PIPE::ENTRY value ={},
        size_t start_i =std::numeric_limits<size_t>::max(),
        size_t iterations =std::numeric_limits<size_t>::max()
    ) const noexcept;
    size_t erase(
        INDEX::TYPE, KEY key, PIPE::ENTRY value ={},
        size_t start_i =std::numeric_limits<size_t>::max(),
        size_t iterations =std::numeric_limits<size_t>::max()
    ) noexcept;
    [[nodiscard]] INDEX::ENTRY insert(
        INDEX::TYPE, KEY key, PIPE::ENTRY value
    ) noexcept;
    void erase(PIPE &pipe, size_t index) noexcept;
    void destroy(PIPE &pipe) noexcept;
    void set_value(INDEX::ENTRY, PIPE::ENTRY) noexcept;
    PIPE::ENTRY get_value(INDEX::ENTRY) const noexcept;
    PIPE::ENTRY get_entry(const PIPE &pipe, size_t index) const noexcept;
    PIPE::ENTRY get_last(const PIPE &pipe) const noexcept;
    PIPE::ENTRY pop_back(PIPE &pipe) noexcept;
    [[nodiscard]] ERROR reserve(PIPE&, size_t capacity) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, size_t index, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR copy(const PIPE &src, PIPE &dst) noexcept;
    [[nodiscard]] ERROR append(const PIPE &src, PIPE &dst) noexcept;
    void replace(PIPE&, size_t index, PIPE::ENTRY) noexcept;
    ERROR swap(PIPE &, PIPE &) noexcept;
    PIPE &get_buffer(BUFFER) noexcept;
    INDEX &get_index(INDEX::TYPE) noexcept;

    JACK     *to_jack  (PIPE::ENTRY) const noexcept;
    MEMORY   *to_memory(PIPE::ENTRY) const noexcept;
    int       to_int   (PIPE::ENTRY) const noexcept;
    uint64_t  to_uint64(PIPE::ENTRY) const noexcept;

    int         *to_int        (const PIPE &) const noexcept;
    uint8_t     *to_uint8      (const PIPE &) const noexcept;
    uint64_t    *to_uint64     (const PIPE &) const noexcept;
    KEY         *to_key        (const PIPE &) const noexcept;
    void       **to_ptr        (const PIPE &) const noexcept;
    epoll_event *to_epoll_event(const PIPE &) const noexcept;

    const MEMORY *find_memory(const void *) const noexcept;
    MEMORY *find_memory(const void *) noexcept;
    const MEMORY &get_memory(const void *) const noexcept;
    MEMORY &get_memory(const void *) noexcept;
    MEMORY *allocate_and_index(
        size_t byte_count, const void *copy =nullptr
    ) noexcept;
    MEMORY *allocate(size_t byte_count) noexcept;
    void deallocate(MEMORY &) noexcept;
    JACK *new_jack(const JACK *copy =nullptr) noexcept;

    void log(
        const char *fmt, ...
    ) const noexcept __attribute__((format(printf, 2, 3)));
    void bug(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;
    ERROR die(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;

    void clear() noexcept;
    void dump(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;

    ERROR err(ERROR) noexcept;

    void (*log_callback)(const char *text) noexcept;
    INDEX indices[static_cast<size_t>(INDEX::TYPE::MAX_TYPES)];
    PIPE  buffers[static_cast<size_t>(BUFFER::MAX_BUFFERS)];
    MEMORY *allocated;
    EVENT handled;
    ERROR errored;

    struct bitset_type {
        bool out_of_memory:1;
        bool unhandled_events:1;
        bool timeout:1;
    } bitset;

    sigset_t sigset_all;
    sigset_t sigset_none;
    sigset_t sigset_orig;
};

bool operator!(SOCKETS::ERROR error) noexcept {
    return error == static_cast<SOCKETS::ERROR>(0);
}

SOCKETS::SOCKETS() noexcept :
    log_callback(nullptr), indices{}, buffers{}, allocated(nullptr), handled{},
    errored{}, bitset{} {
}

SOCKETS::~SOCKETS() {
    // TODO: check for memory leaks by comparing total number of bytes allocated
    // to the total number of bytes deallocated

    for (INDEX &index : indices) {
        if (index.type == INDEX::TYPE::NONE) {
            continue;
        }

        log(
            "%s\n", "destroying instance without having it deinitialized first"
        );

        break;
    }
}

void SOCKETS::clear() noexcept {
    errored = ERROR::NONE;
    handled = EVENT::NONE;

    for (PIPE &buffer : buffers) {
        destroy(buffer);
    }

    for (INDEX &index : indices) {
        if (index.table) {
            for (size_t i=0; i<index.buckets; ++i) {
                destroy(index.table[i].key);
                destroy(index.table[i].value);
            }

            delete [] index.table;
            index.table = nullptr;
        }

        index.buckets = 0;
        index.type = INDEX::TYPE::NONE;
    }

    if (allocated) {
        bug(); // We should have already explicitly deallocated all memory.

        while (allocated) {
            deallocate(*allocated);
        }
    }

    bitset = {};
}

bool SOCKETS::init() noexcept {
    static constexpr const size_t max_key_hash = 1024;

    for (INDEX &index : indices) {
        if (index.type != INDEX::TYPE::NONE) {
            log("%s: already initialized", __FUNCTION__);

            return false;
        }
    }

    int retval = sigfillset(&sigset_all);
    if (retval == -1) {
        int code = errno;

        log(
            "sigfillset: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );

        return false;
    }
    else if (retval) {
        log(
            "sigfillset: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );

        return false;
    }

    retval = sigemptyset(&sigset_none);
    if (retval == -1) {
        int code = errno;

        log(
            "sigemptyset: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );

        return false;
    }
    else if (retval) {
        log(
            "sigemptyset: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );

        return false;
    }

    for (INDEX &index : indices) {
        index.type = static_cast<INDEX::TYPE>(&index - &indices[0]);

        switch (index.type) {
            default: {
                index.buckets = max_key_hash;
                index.multimap = false;
                break;
            }
            case INDEX::TYPE::EVENT_DESCRIPTOR: {
                index.buckets = static_cast<size_t>(EVENT::MAX_EVENTS);
                index.multimap = true;
                break;
            }
        }

        switch (index.type) {
            case INDEX::TYPE::NONE: continue;
            case INDEX::TYPE::EVENT_DESCRIPTOR:
            case INDEX::TYPE::DESCRIPTOR_JACK:
            case INDEX::TYPE::RESOURCE_MEMORY:
            case INDEX::TYPE::GROUP_SIZE: {
                index.table = new (std::nothrow) INDEX::TABLE [index.buckets]();
                break;
            }
            default: die();
        }

        if (index.table == nullptr) {
            clear();
            return false;
        }

        for (size_t j=0; j<index.buckets; ++j) {
            INDEX::TABLE &table = index.table[j];
            PIPE &key_pipe = table.key;
            PIPE &val_pipe = table.value;

            key_pipe.type = PIPE::TYPE::KEY;

            switch (index.type) {
                case INDEX::TYPE::DESCRIPTOR_JACK: {
                    val_pipe.type = PIPE::TYPE::JACK_PTR;
                    break;
                }
                case INDEX::TYPE::RESOURCE_MEMORY: {
                    val_pipe.type = PIPE::TYPE::MEMORY_PTR;
                    break;
                }
                case INDEX::TYPE::GROUP_SIZE: {
                    val_pipe.type = PIPE::TYPE::UINT64;
                    break;
                }
                case INDEX::TYPE::EVENT_DESCRIPTOR: {
                    val_pipe.type = PIPE::TYPE::INT;
                    break;
                }
                default: die();
            }

            if (val_pipe.type == PIPE::TYPE::NONE) {
                clear();
                return false;
            }
        }
    }

    for (PIPE &pipe : buffers) {
        switch (static_cast<BUFFER>(&pipe - &buffers[0])) {
            case BUFFER::NEXT_ERROR:
            case BUFFER::GENERIC_INT: {
                pipe.type = PIPE::TYPE::INT;
                break;
            }
            case BUFFER::GENERIC_BYTE:
            case BUFFER::WRITEF: {
                pipe.type = PIPE::TYPE::UINT8;
                break;
            }
            case BUFFER::READ:
            case BUFFER::HANDLE_READ: {
                static constexpr const size_t buffer_length{
                    1024 // TODO: make it possible to configure this
                };

                pipe.type = PIPE::TYPE::UINT8;

                ERROR error{ reserve(pipe, buffer_length) };

                if (error != ERROR::NONE) {
                    log(
                        "%s: %s (%s:%d)",
                        __FUNCTION__, get_code(error), __FILE__, __LINE__
                    );

                    clear();

                    return false;
                }

                break;
            }
            case BUFFER::MAX_BUFFERS: die();
        }
    }

    int epoll_descriptor = create_epoll();

    if (epoll_descriptor == NO_DESCRIPTOR) {
        log(
            "%s: %s (%s:%d)", __FUNCTION__,
            "epoll jack could not be created", __FILE__, __LINE__
        );

        return false;
    }

    return true;
}

bool SOCKETS::deinit() noexcept {
    if (!find_epoll_jack()) {
        log("%s: already deinitialized", __FUNCTION__);

        return false;
    }

    bool success = true;

    INDEX &descriptor_jack = get_index(INDEX::TYPE::DESCRIPTOR_JACK);

    for (size_t bucket=0; bucket<descriptor_jack.buckets; ++bucket) {
        while (descriptor_jack.table[bucket].key.size) {
            JACK *jack{
                to_jack(
                    get_last(descriptor_jack.table[bucket].value)
                )
            };

            if (!close_and_deinit(jack->descriptor)) {
                // If for some reason we couldn't close the descriptor,
                // we still need to deallocate the related memmory.

                release(jack);
                success = false;
            }
        }
    }

    clear();

    return success;
}

void SOCKETS::set_logger(void (*callback)(const char *) noexcept) noexcept {
    log_callback = callback;
}

int SOCKETS::listen(const char *port, int family, int flags) noexcept {
    return listen(nullptr, port, family, flags);
}

struct SOCKETS::ALERT SOCKETS::next_alert() noexcept {
    int d;

    bitset.unhandled_events = false;

    while (( d = next_connection() ) != NO_DESCRIPTOR) {
        return make_alert(d, EV_CONNECTION);
    }

    while (( d = next_incoming() ) != NO_DESCRIPTOR) {
        return make_alert(d, EV_INCOMING);
    }

    while (( d = next_disconnection() ) != NO_DESCRIPTOR) {
        return make_alert(d, EV_DISCONNECTION);
    }

    return make_alert(NO_DESCRIPTOR, EV_NONE, false);
}

int SOCKETS::next_connection() noexcept {
    JACK *jack = find_jack(EVENT::CONNECTION);

    if (jack) {
        rem_event(*jack, EVENT::CONNECTION);

        return jack->descriptor;
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_disconnection() noexcept {
    if (find_jack(EVENT::CONNECTION)) {
        // We postpone reporting any disconnections until the application
        // has acknowledged all the new incoming connections. This prevents
        // us from reporting a disconnection event before its respective
        // connection event is reported.

        return NO_DESCRIPTOR;
    }

    JACK *jack = find_jack(EVENT::DISCONNECTION);

    if (jack) {
        if (set_event(*jack, EVENT::CLOSE) == ERROR::NONE) {
            rem_event(*jack, EVENT::DISCONNECTION);

            return jack->descriptor;
        }
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_incoming() noexcept {
    JACK *jack = find_jack(EVENT::INCOMING);

    if (jack) {
        rem_event(*jack, EVENT::INCOMING);

        return jack->descriptor;
    }

    return NO_DESCRIPTOR;
}

bool SOCKETS::is_listener(const JACK &jack) const noexcept {
    return jack.bitset.listener;
}

bool SOCKETS::is_listener(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);
    return jack ? is_listener(*jack) : false;
}

int SOCKETS::get_group(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);
    return jack ? jack->group : 0;
}

size_t SOCKETS::get_group_size(int group) const noexcept {
    INDEX::ENTRY found{find(INDEX::TYPE::GROUP_SIZE, make_key(group))};

    if (found.valid) {
        return to_uint64(get_value(found));
    }

    return 0;
}

int SOCKETS::get_listener(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);
    return jack ? jack->parent : NO_DESCRIPTOR;
}

const char *SOCKETS::get_host(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);
    return jack ? jack->host : "";
}

const char *SOCKETS::get_port(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);
    return jack ? jack->port : "";
}

void SOCKETS::freeze(int descriptor) noexcept {
    JACK *jack = find_jack(descriptor);

    if (jack) {
        jack->bitset.frozen = true;
    }
}

void SOCKETS::unfreeze(int descriptor) noexcept {
    JACK *jack = find_jack(descriptor);

    if (jack) {
        if (!has_event(*jack, EVENT::DISCONNECTION)
        &&  !has_event(*jack, EVENT::CLOSE)) {
            jack->bitset.frozen = false;
        }
    }
}

bool SOCKETS::is_frozen(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);

    return jack ? jack->bitset.frozen : false;
}

bool SOCKETS::idle() const noexcept {
    return bitset.timeout;
}

bool SOCKETS::connect(const char *host, const char *port, int group) noexcept {
    int descriptor = connect(host, port, group, AF_UNSPEC, 0);

    return descriptor != NO_DESCRIPTOR;
}

void SOCKETS::disconnect(int descriptor) noexcept {
    terminate(descriptor);
}

SOCKETS::ERROR SOCKETS::err(ERROR e) noexcept {
    return (errored = e);
}

SOCKETS::ERROR SOCKETS::last_error() noexcept {
    return errored;
}

SOCKETS::ERROR SOCKETS::next_error(int timeout) noexcept {
    if (bitset.out_of_memory) {
        bitset.out_of_memory = false;
        return err(ERROR::OUT_OF_MEMORY);
    }

    if (find_jack(EVENT::CONNECTION)) {
        // We postpone handling any descriptor events until the application has
        // acknowledged all the new incoming connections.

        if (!bitset.unhandled_events) {
            bitset.unhandled_events = true;
            return err(ERROR::NONE);
        }

        return err(ERROR::UNHANDLED_EVENTS);
    }

    PIPE &descriptor_buffer = get_buffer(BUFFER::NEXT_ERROR);

    if (handled == EVENT::NONE) {
        bitset.timeout = false;
        handled = next(handled);
    }

    for (; handled != EVENT::NONE; handled = next(handled)) {
        EVENT event = handled;

        switch (event) {
            case EVENT::INCOMING:
            case EVENT::CONNECTION:
            case EVENT::DISCONNECTION: {
                // This event has to be handled by the user. We ignore it here.

                continue;
            }
            default: break;
        }

        const PIPE *event_subscribers = find_descriptors(event);

        if (!event_subscribers) continue;

        ERROR error = copy(*event_subscribers, descriptor_buffer);

        if (error != ERROR::NONE) {
            return err(error);
        }

        for (size_t j=0, sz=descriptor_buffer.size; j<sz; ++j) {
            int d = to_int(get_entry(descriptor_buffer, j));
            JACK *jack = find_jack(d);

            if (jack == nullptr) continue;

            rem_event(*jack, event);

            ERROR error = ERROR::NONE;

            switch (event) {
                case EVENT::EPOLL: {
                    error = handle_epoll(*jack, timeout);
                    break;
                }
                case EVENT::CLOSE: {
                    if (has_event(*jack, EVENT::READ)
                    && !jack->bitset.frozen) {
                        // Unless this descriptor is frozen, we postpone
                        // normal closing until there is nothing left to
                        // read from this descriptor.

                        set_event(*jack, event);
                        continue;
                    }

                    error = handle_close(*jack);
                    break;
                }
                case EVENT::ACCEPT: {
                    if (find_jack(EVENT::DISCONNECTION)
                    || jack->bitset.frozen) {
                        // We postpone the acceptance of new connections
                        // until all the recent disconnections have been
                        // acknowledged and the descriptor is not frozen.

                        set_event(*jack, event);
                        continue;
                    }

                    error = handle_accept(*jack);
                    break;
                }
                case EVENT::WRITE: {
                    if (jack->bitset.frozen) {
                        set_event(*jack, event);
                        continue;
                    }

                    error = handle_write(*jack);
                    break;
                }
                case EVENT::READ: {
                    if (jack->bitset.frozen) {
                        set_event(*jack, event);
                        continue;
                    }

                    error = handle_read(*jack);
                    break;
                }
                default: {
                    log(
                        "Event %lu of descriptor %d was not handled.",
                        static_cast<size_t>(event), d
                    );

                    error = ERROR::FORBIDDEN_CONDITION;
                    break;
                }
            }

            if (error == ERROR::NONE) {
                continue;
            }

            return err(error);
        }
    }

    return err(ERROR::NONE);
}

size_t SOCKETS::incoming(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);

    if (jack) {
        return jack->incoming.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::outgoing(int descriptor) const noexcept {
    const JACK *jack = find_jack(descriptor);

    if (jack) {
        return jack->outgoing.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::read(int descriptor, void *buf, size_t count) noexcept {
    if (!count) return 0;

    JACK *jack = find_jack(descriptor);

    if (!jack) {
        bug();
        return 0;
    }

    if (jack->incoming.size == 0) {
        return 0;
    }

    count = std::min(count, jack->incoming.size);

    if (buf) {
        std::memcpy(buf, to_uint8(jack->incoming), count);
    }

    if (jack->incoming.size > count) {
        std::memmove(
            jack->incoming.data,
            to_uint8(jack->incoming) + count, jack->incoming.size - count
        );
    }

    jack->incoming.size -= count;

    return count;
}

const char *SOCKETS::read(int descriptor) noexcept {
    JACK *jack = find_jack(descriptor);

    if (!jack || jack->incoming.size == 0) {
        return "";
    }

    PIPE &buffer = get_buffer(BUFFER::READ);

    if (buffer.capacity <= 1) {
        return "";
    }

    size_t count{
        read(
            descriptor, to_uint8(buffer),
            std::min(buffer.capacity - 1, jack->incoming.size)
        )
    };

    to_uint8(buffer)[count] = '\0';
    buffer.size = count + 1;

    return reinterpret_cast<const char *>(to_uint8(buffer));
}

SOCKETS::ERROR SOCKETS::write(
    int descriptor, const void *buf, size_t count
) noexcept {
    JACK *jack = find_jack(descriptor);

    if (!jack) {
        bug();
        return ERROR::FORBIDDEN_CONDITION;
    }

    if (!buf) die();

    if (count) {
        const PIPE wrapper{
            make_pipe(reinterpret_cast<const uint8_t *>(buf), count)
        };

        ERROR error{ append(wrapper, jack->outgoing) };

        if (error != ERROR::NONE) {
            return error;
        }

        set_event(*jack, EVENT::WRITE, jack->outgoing.size);
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::write(int descriptor, const char *text) noexcept {
    return write(descriptor, text, std::strlen(text));
}

SOCKETS::ERROR SOCKETS::writef(int descriptor, const char *fmt, ...) noexcept {
    char stackbuf[1024];

    std::va_list args;
    va_start(args, fmt);
    int retval = vsnprintf(stackbuf, sizeof(stackbuf), fmt, args);
    va_end(args);

    if (retval < 0) {
        log(
            "%s: encoding error when formatting '%s' (%s:%d).",
            __FUNCTION__, fmt, __FILE__, __LINE__
        );

        return die();
    }

    JACK &jack = get_jack(descriptor);

    if (size_t(retval) < sizeof(stackbuf)) {
        const PIPE wrapper{
            make_pipe(reinterpret_cast<const uint8_t *>(stackbuf), retval)
        };

        ERROR error{ append(wrapper, jack.outgoing) };

        if (error != ERROR::NONE) {
            return error;
        }

        set_event(jack, EVENT::WRITE, jack.outgoing.size);

        return ERROR::NONE;
    }

    PIPE &buffer = get_buffer(BUFFER::WRITEF);

    size_t heapbuf_sz = size_t(retval) + 1;
    char *heapbuf = nullptr;

    ERROR error{ reserve(buffer, heapbuf_sz) };

    if (error != ERROR::NONE) {
        return error;
    }

    heapbuf = (char *) buffer.data;

    va_start(args, fmt);
    retval = vsnprintf(heapbuf, heapbuf_sz, fmt, args);
    va_end(args);

    if (retval < 0) {
        log(
            "%s: encoding error when formatting '%s' (%s:%d).",
            __FUNCTION__, fmt, __FILE__, __LINE__
        );
    }
    else if (size_t(retval) < heapbuf_sz) {
        const PIPE wrapper{
            make_pipe(reinterpret_cast<const uint8_t *>(heapbuf), retval)
        };

        ERROR error{ append(wrapper, jack.outgoing) };

        if (error != ERROR::NONE) {
            return error;
        }

        set_event(jack, EVENT::WRITE, jack.outgoing.size);

        return ERROR::NONE;
    }
    else {
        log(
            "%s: unexpected program flow (%s:%d).",
            __FUNCTION__, __FILE__, __LINE__
        );
    }

    return die();
}

void SOCKETS::log(const char *fmt, ...) const noexcept {
    char stackbuf[256];
    char *bufptr = stackbuf;
    size_t bufsz = sizeof(stackbuf);

    for (size_t i=0; i<2 && bufptr; ++i) {
        std::va_list args;
        va_start(args, fmt);
        int cx = vsnprintf(bufptr, bufsz, fmt, args);
        va_end(args);

        if ((cx >= 0 && (size_t)cx < bufsz) || cx < 0) {
            if (log_callback) {
                log_callback(bufptr);
            }
            else {
                ::write(STDERR_FILENO, bufptr, strlen(bufptr));
                ::write(STDERR_FILENO, "\n", 1);
            }

            break;
        }

        if (bufptr == stackbuf) {
            bufsz = cx + 1;
            bufptr = new (std::nothrow) char[bufsz];

            if (!bufptr) {
                static constexpr const char *OOM = "Out Of Memory!";

                if (log_callback) {
                    log_callback(OOM);
                }
                else {
                    ::write(STDERR_FILENO, OOM, strlen(OOM));
                    ::write(STDERR_FILENO, "\n", 1);
                }
            }
        }
        else {
            if (log_callback) {
                log_callback(bufptr);
            }
            else {
                ::write(STDERR_FILENO, bufptr, strlen(bufptr));
                ::write(STDERR_FILENO, "\n", 1);
            }

            break;
        }
    }

    if (bufptr && bufptr != stackbuf) delete [] bufptr;
}

void SOCKETS::bug(const char *file, int line) const noexcept {
    log("Forbidden condition met in %s on line %d.", file, line);
}

SOCKETS::ERROR SOCKETS::die(const char *file, int line) const noexcept {
    bug(file, line);
    fflush(nullptr);
    std::raise(SIGSEGV);
    return ERROR::UNDEFINED_BEHAVIOR;
}

SOCKETS::ERROR SOCKETS::handle_close(JACK &jack) noexcept {
    int descriptor = jack.descriptor;

    if (jack.bitset.reconnect) {
        int new_descriptor = connect(
            get_host(descriptor), get_port(descriptor),
            get_group(descriptor), jack.ai_family, jack.ai_flags, jack.blacklist
        );

        if (new_descriptor == NO_DESCRIPTOR) {
            jack.bitset.reconnect = false;
        }
        else {
            JACK &new_jack = get_jack(new_descriptor);

            struct addrinfo *blacklist = new_jack.blacklist;

            if (!blacklist) {
                blacklist = jack.blacklist;
                jack.blacklist = nullptr;
            }
            else {
                for (;; blacklist = blacklist->ai_next) {
                    if (blacklist->ai_next == nullptr) {
                        blacklist->ai_next = jack.blacklist;
                        jack.blacklist = nullptr;
                        break;
                    }
                }
            }
        }
    }

    if (jack.bitset.connecting
    && !jack.bitset.reconnect
    && !has_event(jack, EVENT::DISCONNECTION)) {
        // Here we postpone closing this descriptor because we first want to
        // notify the user of this library of the connection that could not be
        // established.

        jack.bitset.connecting = false;
        set_event(jack, EVENT::DISCONNECTION);

        return ERROR::NONE;
    }

    if (!close_and_deinit(descriptor)) {
        release(&jack);
        return ERROR::FORBIDDEN_CONDITION;
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_epoll(
    JACK &epoll_jack, int timeout
) noexcept {
    static constexpr const EVENT blockers[]{
        EVENT::CONNECTION,
        EVENT::DISCONNECTION,
        EVENT::INCOMING
    };

    set_event(epoll_jack, EVENT::EPOLL);

    for (EVENT event_type : blockers) {
        if (!find_jack(event_type)) {
            continue;
        }

        if (!bitset.unhandled_events) {
            bitset.unhandled_events = true;

            return ERROR::NONE;
        }

        log("Cannot serve sockets if there are unhandled events.\n");

        return ERROR::UNHANDLED_EVENTS;
    }

    bitset.unhandled_events = false;

    epoll_event *events = to_epoll_event(epoll_jack.epoll_ev);

    int maxevents{
        epoll_jack.epoll_ev.size > std::numeric_limits<int>::max() ? (
            std::numeric_limits<int>::max()
        ) : static_cast<int>(epoll_jack.epoll_ev.size)
    };

    int pending = epoll_pwait(
        epoll_jack.descriptor, events, maxevents, timeout, &sigset_none
    );

    if (pending == -1) {
        int code = errno;

        if (code == EINTR) return ERROR::NONE;

        log(
            "epoll_pwait: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );

        return ERROR::FORBIDDEN_CONDITION;
    }
    else if (pending < 0) {
        log(
            "epoll_pwait: unexpected return value %d (%s:%d)", pending,
            __FILE__, __LINE__
        );

        return ERROR::FORBIDDEN_CONDITION;
    }
    else if (pending == 0) {
        bitset.timeout = true;
    }
    else if (pending == maxevents) {
        // Increase the epoll event buffer size.
        size_t old_size = epoll_jack.epoll_ev.size;

        ERROR error{
            insert(
                epoll_jack.epoll_ev, make_pipe_entry(PIPE::TYPE::EPOLL_EVENT)
            )
        };

        epoll_jack.epoll_ev.size = epoll_jack.epoll_ev.capacity;

        size_t new_size = epoll_jack.epoll_ev.size;

        if (error != ERROR::NONE) {
            log(
                "%s: %s (%s:%d)", __FUNCTION__, get_code(error),
                __FILE__, __LINE__
            );
        }

        if (old_size != new_size) {
            log("Epoll event buffer size has been changed to %lu.", new_size);
        }

        events = to_epoll_event(
            // Reallocation may have happened.
            epoll_jack.epoll_ev
        );
    }

    for (int i=0; i<pending; ++i) {
        const int d = events[i].data.fd;
        JACK &jack = get_jack(d);

        if ((  events[i].events & EPOLLERR )
        ||  (  events[i].events & EPOLLHUP )
        ||  (  events[i].events & EPOLLRDHUP )
        ||  (!(events[i].events & (EPOLLIN|EPOLLOUT) ))) {
            int socket_error = 0;
            socklen_t socket_errlen = sizeof(socket_error);

            if (events[i].events & EPOLLERR) {
                int retval = getsockopt(
                    d, SOL_SOCKET, SO_ERROR, (void *) &socket_error,
                    &socket_errlen
                );

                if (retval) {
                    if (retval == -1) {
                        int code = errno;

                        log(
                            "getsockopt: %s (%s:%d)",
                            strerror(code), __FILE__, __LINE__
                        );
                    }
                    else {
                        log(
                            "getsockopt: unexpected return value %d "
                            "(%s:%d)", retval, __FILE__, __LINE__
                        );
                    }
                }
                else {
                    switch (socket_error) {
                        case EPIPE:
                        case ECONNRESET: {
                            // Silent errors.
                            break;
                        }
                        case ECONNREFUSED: {
                            if (jack.bitset.connecting) {
                                jack.bitset.reconnect = true;
                                break;
                            }
                        } // fall through
                        default: {
                            log(
                                "epoll error on descriptor %d: %s (%s:%d)",
                                d, strerror(socket_error), __FILE__,
                                __LINE__
                            );

                            break;
                        }
                    }
                }
            }
            else if ((events[i].events & EPOLLHUP) == false
            && (events[i].events & EPOLLRDHUP) == false) {
                log(
                    "unexpected events %d on descriptor %d (%s:%d)",
                    events[i].events, d, __FILE__, __LINE__
                );
            }

            jack.bitset.may_shutdown = false;
            terminate(d);

            continue;
        }

        if (is_listener(jack)) {
            set_event(jack, EVENT::ACCEPT);
        }
        else {
            if (events[i].events & EPOLLIN) {
                set_event(jack, EVENT::READ);
            }

            if (events[i].events & EPOLLOUT) {
                set_event(jack, EVENT::WRITE);

                if (jack.bitset.connecting) {
                    jack.bitset.connecting = false;
                    set_event(jack, EVENT::CONNECTION);
                    jack.bitset.may_shutdown = true;
                    modify_epoll(d, EPOLLIN|EPOLLET|EPOLLRDHUP);
                }
            }
        }
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_read(JACK &jack) noexcept {
    // TODO: read directly to the pipework of the jack's incoming buffer in
    // respect to its individual hard limit of incoming bytes.

    int descriptor = jack.descriptor;
    PIPE &buffer = get_buffer(BUFFER::HANDLE_READ);

    for (size_t total_count = 0;;) {
        ssize_t count;
        char *buf = (char *) buffer.data;
        const size_t buf_sz = buffer.capacity;

        if (!buf_sz) {
            set_event(jack, EVENT::READ);
            return ERROR::NONE;
        }
        else if ((count = ::read(descriptor, buf, buf_sz)) < 0) {
            if (count == -1) {
                int code = errno;

                if (code == EAGAIN || code == EWOULDBLOCK) {
                    if (total_count) {
                        rem_event(jack, EVENT::READ);
                    }

                    return ERROR::NONE;
                }

                log(
                    "read(%d, ?, %lu): %s (%s:%d)", descriptor, buf_sz,
                    strerror(code), __FILE__, __LINE__
                );

                break;
            }

            log(
                "read(%d, ?, %lu): unexpected return value %lld (%s:%d)",
                descriptor, buf_sz, (long long)(count), __FILE__, __LINE__
            );

            break;
        }
        else if (count == 0) {
            // End of file. The remote has closed the connection.
            break;
        }

        const PIPE wrapper{
            make_pipe(reinterpret_cast<const uint8_t *>(buf), count)
        };

        ERROR error{
            // TODO: if appending fails, then incoming bytes go lost. in case
            // of out-of-memory error, we should somehow overcome this.
            append(wrapper, jack.incoming)
        };

        if (!total_count) {
            set_event(jack, EVENT::READ);
            set_event(jack, EVENT::INCOMING);
        }

        if (error != ERROR::NONE) {
            // TODO: figure out if there is a better way to overcome errors here
            return error;
        }

        total_count += count;

        //if (size_t(count) == buf_sz && buffer.capacity < MAX_CACHE_SIZE) {
        //    TODO: request for the expansion of the buffer
        //    reserve(buffer, std::min(2 * buffer.capacity, MAX_CACHE_SIZE));
        //}
    }

    jack.bitset.may_shutdown = false;
    terminate(descriptor);

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_write(JACK &jack) noexcept {
    int descriptor = jack.descriptor;
    PIPE &outgoing = jack.outgoing;

    if (outgoing.size == 0) {
        return ERROR::NONE;
    }

    const unsigned char *bytes = to_uint8(outgoing);
    size_t length = outgoing.size;

    bool try_again_later = true;
    size_t istart;
    ssize_t nwrite;

    for (istart = 0; istart<length; istart+=nwrite) {
        static constexpr const size_t max_chunk = 1024 * 1024;
        size_t buf = length - istart;
        size_t nblock = (buf < max_chunk ? buf : max_chunk);

        nwrite = ::write(descriptor, bytes+istart, nblock);

        if (nwrite < 0) {
            int code = errno;

            if (code != EPIPE) {
                if (code == EAGAIN || code == EWOULDBLOCK) {
                    // Let's start expecting EPOLLOUT.
                    try_again_later = false;
                }
                else {
                    log(
                        "write: %s (%s:%d)", strerror(code), __FILE__, __LINE__
                    );
                }
            }

            break;
        }
        else if (nwrite == 0) {
            break;
        }
    }

    if (istart == length) {
        outgoing.size = 0;
    }
    else if (istart > outgoing.size) die();
    else if (istart > 0) {
        size_t new_size = outgoing.size - istart;

        std::memmove(outgoing.data, to_uint8(outgoing) + istart, new_size);

        outgoing.size = new_size;

        if (try_again_later) {
            set_event(jack, EVENT::WRITE);
        }
    }

    if (try_again_later) {
        return (
            modify_epoll(descriptor, EPOLLIN|EPOLLET|EPOLLRDHUP) ? (
                ERROR::NONE
            ) : ERROR::FORBIDDEN_CONDITION
        );
    }

    return (
        modify_epoll(descriptor, EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP) ? (
            ERROR::NONE
        ) : ERROR::FORBIDDEN_CONDITION
    );
}

SOCKETS::ERROR SOCKETS::handle_accept(JACK &jack) noexcept {
    int descriptor = jack.descriptor;

    // New incoming connection detected.
    JACK &epoll_jack = get_epoll_jack();
    int epoll_descriptor = epoll_jack.descriptor;

    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);

    int client_descriptor{
        accept4(
            descriptor, &in_addr, &in_len, SOCK_CLOEXEC|SOCK_NONBLOCK
        )
    };

    if (client_descriptor < 0) {
        if (client_descriptor == -1) {
            int code = errno;

            switch (code) {
#if EAGAIN != EWOULDBLOCK
                case EAGAIN:
#endif
                case EWOULDBLOCK: {
                    // Everything is normal.

                    return ERROR::NONE;
                }
                case ENETDOWN:
                case EPROTO:
                case ENOPROTOOPT:
                case EHOSTDOWN:
                case ENONET:
                case EHOSTUNREACH:
                case EOPNOTSUPP:
                case ENETUNREACH: {
                    // These errors are supposed to be temporary.

                    log(
                        "accept4: %s (%s:%d)", strerror(code),
                        __FILE__, __LINE__
                    );

                    set_event(jack, EVENT::ACCEPT);

                    return ERROR::NONE;
                }
                case EINTR: {
                    set_event(jack, EVENT::ACCEPT);

                    return ERROR::NONE;
                }
                default: {
                    // These errors are fatal.

                    log(
                        "accept4: %s (%s:%d)", strerror(code),
                        __FILE__, __LINE__
                    );

                    break;
                }
            }
        }
        else {
            log(
                "accept4: unexpected return value %d (%s:%d)",
                client_descriptor, __FILE__, __LINE__
            );
        }

        // Something has gone terribly wrong.

        if (!close_and_deinit(descriptor)) {
            release(find_jack(descriptor));
        }

        return ERROR::FORBIDDEN_CONDITION;
    }

    ERROR error{capture(make_jack(client_descriptor, descriptor, 0))};

    if (error != ERROR::NONE) {
        if (!close_and_deinit(client_descriptor)) {
            release(find_jack(client_descriptor));
        }

        return ERROR::FORBIDDEN_CONDITION;
    }

    JACK &client_jack = get_jack(client_descriptor);

    int retval = getnameinfo(
        &in_addr, in_len,
        client_jack.host,
        socklen_t(std::extent<decltype(client_jack.host)>::value),
        client_jack.port,
        socklen_t(std::extent<decltype(client_jack.port)>::value),
        NI_NUMERICHOST|NI_NUMERICSERV
    );

    if (retval != 0) {
        log(
            "getnameinfo: %s (%s:%d)", gai_strerror(retval),
            __FILE__, __LINE__
        );

        client_jack.host[0] = '\0';
        client_jack.port[0] = '\0';
    }

    epoll_event event{
        make_epoll_event(client_descriptor, EPOLLIN|EPOLLET|EPOLLRDHUP)
    };

    retval = epoll_ctl(
        epoll_descriptor, EPOLL_CTL_ADD, client_descriptor, &event
    );

    if (retval != 0) {
        if (retval == -1) {
            int code = errno;

            log(
                "epoll_ctl: %s (%s:%d)", strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "epoll_ctl: unexpected return value %d (%s:%d)",
                retval, __FILE__, __LINE__
            );
        }

        if (!close_and_deinit(client_descriptor)) {
            release(&client_jack);
        }
    }
    else {
        set_event(client_jack, EVENT::CONNECTION);
        client_jack.bitset.may_shutdown = true;
    }

    // We successfully accepted one client, but since there may be more of
    // them waiting we should recursively retry until we fail to accept any
    // new connections.

    return handle_accept(jack);
}

int SOCKETS::connect(
    const char *host, const char *port, int group, int ai_family, int ai_flags,
    const struct addrinfo *blacklist, const char *file, int line
) noexcept {
    int epoll_descriptor = get_epoll_jack().descriptor;

    int descriptor{
        open_and_init(
            host, port, ai_family, ai_flags, blacklist, file, line
        )
    };

    if (descriptor == NO_DESCRIPTOR) {
        return NO_DESCRIPTOR;
    }

    if (set_group(descriptor, group) != ERROR::NONE) {
        if (!close_and_deinit(descriptor)) {
            release(find_jack(descriptor));
        }

        return NO_DESCRIPTOR;
    }

    JACK &jack = get_jack(descriptor);

    if (jack.host[0] == '\0') {
        size_t buflen = std::extent<decltype(jack.host)>::value;
        strncpy(jack.host, host, buflen - 1);
        jack.host[buflen - 1] = '\0';
    }

    if (jack.port[0] == '\0') {
        size_t buflen = std::extent<decltype(jack.port)>::value;
        strncpy(jack.port, port, buflen - 1);
        jack.port[buflen - 1] = '\0';
    }

    if (!bind_to_epoll(descriptor, epoll_descriptor)) {
        if (!close_and_deinit(descriptor)) {
            release(&jack);
        }

        return NO_DESCRIPTOR;
    }

    if (jack.bitset.connecting) {
        modify_epoll(descriptor, EPOLLOUT|EPOLLET);
    }
    else {
        jack.bitset.may_shutdown = true;
        set_event(jack, EVENT::CONNECTION);
    }

    return descriptor;
}

void SOCKETS::terminate(int descriptor, const char *file, int line) noexcept {
    if (descriptor == NO_DESCRIPTOR) return;

    JACK &jack = get_jack(descriptor);

    if (has_event(jack, EVENT::CLOSE)
    ||  has_event(jack, EVENT::DISCONNECTION)) {
        return;
    }

    if (jack.bitset.reconnect || jack.bitset.connecting) {
        set_event(jack, EVENT::CLOSE);
    }
    else {
        set_event(jack, EVENT::DISCONNECTION);
    }

    if (jack.bitset.may_shutdown) {
        jack.bitset.may_shutdown = false;

        if (has_event(jack, EVENT::WRITE) && !jack.bitset.connecting) {
            // Let's handle writing here so that the descriptor would have a
            // chance to receive any pending bytes before being shut down.
            handle_write(jack);
        }

        int retval = shutdown(descriptor, SHUT_WR);
        if (retval == -1) {
            int code = errno;

            log(
                "shutdown(%d, SHUT_WR): %s (%s:%d)", descriptor,
                strerror(code), file, line
            );
        }
        else if (retval != 0) {
            log(
                "shutdown(%d, SHUT_WR): unexpected return value of %d "
                "(%s:%d)", descriptor, retval, file, line
            );
        }
    }

    if (is_listener(jack)) {
        INDEX &index = get_index(INDEX::TYPE::DESCRIPTOR_JACK);

        for (size_t bucket=0; bucket < index.buckets; ++bucket) {
            // TODO: optimize this (use a special index for child-parent rels?)
            for (size_t i=0; i<index.table[bucket].value.size; ++i) {
                JACK *rec{
                    to_jack(get_entry(index.table[bucket].value, i))
                };

                if (rec->parent != descriptor) {
                    continue;
                }

                terminate(rec->descriptor, file, line);
            }
        }
    }
}

int SOCKETS::listen(
    const char *host, const char *port, int ai_family, int ai_flags,
    const char *file, int line
) noexcept {
    int epoll_descriptor = get_epoll_jack().descriptor;
    int descriptor = open_and_init(host, port, ai_family, ai_flags);

    if (descriptor == NO_DESCRIPTOR) return NO_DESCRIPTOR;

    int retval = ::listen(descriptor, SOMAXCONN);

    if (retval != 0) {
        if (retval == -1) {
            int code = errno;

            log(
                "listen: %s (%s:%d)", strerror(code), file, line
            );
        }
        else {
            log(
                "listen: unexpected return value %d (%s:%d)", retval, file, line
            );
        }

        if (!close_and_deinit(descriptor)) {
            release(find_jack(descriptor));
        }

        return NO_DESCRIPTOR;
    }

    if (!bind_to_epoll(descriptor, epoll_descriptor)) {
        if (!close_and_deinit(descriptor)) {
            release(find_jack(descriptor));
        }

        return NO_DESCRIPTOR;
    }

    JACK &jack = get_jack(descriptor);

    set_event(jack, EVENT::ACCEPT);

    jack.bitset.listener = true;

    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);

    retval = getsockname(descriptor, (struct sockaddr *)&in_addr, &in_len);

    if (retval != 0) {
        if (retval == -1) {
            int code = errno;

            log(
                "getsockname: %s (%s:%d)", strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "getsockname: unexpected return value %d (%s:%d)",
                retval, __FILE__, __LINE__
            );
        }
    }
    else {
        retval = getnameinfo(
            &in_addr, in_len,
            jack.host, socklen_t(std::extent<decltype(jack.host)>::value),
            jack.port, socklen_t(std::extent<decltype(jack.port)>::value),
            NI_NUMERICHOST|NI_NUMERICSERV
        );

        if (retval != 0) {
            log(
                "getnameinfo: %s (%s:%d)", gai_strerror(retval),
                __FILE__, __LINE__
            );

            jack.host[0] = '\0';
            jack.port[0] = '\0';
        }
    }

    return descriptor;
}

int SOCKETS::create_epoll() noexcept {
    int epoll_descriptor = epoll_create1(0);

    if (epoll_descriptor < 0) {
        if (epoll_descriptor == -1) {
            int code = errno;

            log(
                "epoll_create1: %s (%s:%d)",
                strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "epoll_create1: unexpected return value %d (%s:%d)",
                epoll_descriptor, __FILE__, __LINE__
            );
        }

        return NO_DESCRIPTOR;
    }

    {
        ERROR error{capture(make_jack(epoll_descriptor, NO_DESCRIPTOR, 0))};

        if (error != ERROR::NONE) {
            if (!close_and_deinit(epoll_descriptor)) {
                release(find_jack(epoll_descriptor));
            }

            return NO_DESCRIPTOR;
        }
    }

    JACK &jack = get_jack(epoll_descriptor);

    {
        ERROR error{ reserve(jack.epoll_ev, 1) };

        if (error != ERROR::NONE) {
            log(
                "%s: %s (%s:%d)", __FUNCTION__, get_code(error),
                __FILE__, __LINE__
            );

            if (!close_and_deinit(epoll_descriptor)) {
                release(&jack);
            }

            return NO_DESCRIPTOR;
        }

        jack.epoll_ev.size = jack.epoll_ev.capacity;
    }

    set_event(jack, EVENT::EPOLL);

    return epoll_descriptor;
}

bool SOCKETS::bind_to_epoll(int descriptor, int epoll_descriptor) noexcept {
    if (descriptor == NO_DESCRIPTOR) return NO_DESCRIPTOR;

    epoll_event event{
        make_epoll_event(descriptor, EPOLLIN|EPOLLET|EPOLLRDHUP)
    };

    int retval{
        epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, descriptor, &event)
    };

    if (retval != 0) {
        if (retval == -1) {
            int code = errno;

            log(
                "epoll_ctl: %s (%s:%d)", strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "epoll_ctl: unexpected return value %d (%s:%d)", retval,
                __FILE__, __LINE__
            );
        }

        return false;
    }

    return true;
}

int SOCKETS::open_and_init(
    const char *host, const char *port, int ai_family, int ai_flags,
    const struct addrinfo *blacklist, const char *file, int line
) noexcept {
    static constexpr const bool establish_nonblocking_connections = true;
    const bool accept_incoming_connections = host == nullptr;

    struct addrinfo hint =
#if __cplusplus <= 201703L
    __extension__
#endif
    addrinfo{
        .ai_flags     = ai_flags,
        .ai_family    = ai_family,
        .ai_socktype  = SOCK_STREAM,
        .ai_protocol  = 0,
        .ai_addrlen   = 0,
        .ai_addr      = nullptr,
        .ai_canonname = nullptr,
        .ai_next      = nullptr
    };
    struct addrinfo *info = nullptr;
    struct addrinfo *next = nullptr;
    struct addrinfo *prev = nullptr;

    int descriptor = NO_DESCRIPTOR;
    int retval = getaddrinfo(host, port, &hint, &info);

    if (retval != 0) {
        log("getaddrinfo: %s (%s:%d)", gai_strerror(retval), file, line);

        goto CleanUp;
    }

    for (next = info; next; prev = next, next = next->ai_next) {
        if (is_listed(*next, blacklist)) {
            continue;
        }

        if (accept_incoming_connections) {
            descriptor = socket(
                next->ai_family,
                next->ai_socktype|SOCK_NONBLOCK|SOCK_CLOEXEC,
                next->ai_protocol
            );
        }
        else {
            descriptor = socket(
                next->ai_family,
                next->ai_socktype|SOCK_CLOEXEC|(
                    establish_nonblocking_connections ? SOCK_NONBLOCK : 0
                ),
                next->ai_protocol
            );
        }

        if (descriptor == -1) {
            int code = errno;

            log("socket: %s (%s:%d)", strerror(code), __FILE__, __LINE__);

            continue;
        }

        JACK *jack = nullptr;

        ERROR error{capture(make_jack(descriptor, NO_DESCRIPTOR, 0))};

        if (error == ERROR::NONE) {
            jack = &get_jack(descriptor);
            jack->ai_family = ai_family;
            jack->ai_flags  = ai_flags;
        }

        if (jack && accept_incoming_connections) {
            int optval = 1;

            retval = setsockopt(
                descriptor, SOL_SOCKET, SO_REUSEADDR,
                (const void *) &optval, sizeof(optval)
            );

            if (retval != 0) {
                if (retval == -1) {
                    int code = errno;

                    log(
                        "setsockopt: %s (%s:%d)", strerror(code),
                        __FILE__, __LINE__
                    );
                }
                else {
                    log(
                        "setsockopt: unexpected return value %d (%s:%d)",
                        retval, __FILE__, __LINE__
                    );
                }
            }
            else {
                retval = bind(descriptor, next->ai_addr, next->ai_addrlen);

                if (retval) {
                    if (retval == -1) {
                        int code = errno;

                        log(
                            "bind: %s (%s:%d)", strerror(code),
                            __FILE__, __LINE__
                        );
                    }
                    else {
                        log(
                            "bind(%d, ?, %d) returned %d (%s:%d)",
                            descriptor, next->ai_addrlen, retval,
                            __FILE__, __LINE__
                        );
                    }
                }
                else break;
            }
        }
        else if (jack) {
            // Let's block all signals before calling connect because we
            // don't want it to fail due to getting interrupted by a singal.

            retval = sigprocmask(SIG_SETMASK, &sigset_all, &sigset_orig);
            if (retval == -1) {
                int code = errno;
                log(
                    "sigprocmask: %s (%s:%d)", strerror(code),
                    __FILE__, __LINE__
                );
            }
            else if (retval) {
                log(
                    "sigprocmask: unexpected return value %d (%s:%d)",
                    retval, __FILE__, __LINE__
                );
            }
            else {
                bool success = false;

                retval = ::connect(descriptor, next->ai_addr, next->ai_addrlen);

                if (retval) {
                    if (retval == -1) {
                        int code = errno;

                        if (code == EINPROGRESS) {
                            success = true;
                            jack->bitset.connecting = true;

                            if (info == next) {
                                info = next->ai_next;
                            }
                            else {
                                prev->ai_next = next->ai_next;
                            }

                            next->ai_next = jack->blacklist;
                            jack->blacklist = next;
                        }
                        else if (code != ECONNREFUSED) {
                            log(
                                "connect: %s (%s:%d)", strerror(code),
                                __FILE__, __LINE__
                            );
                        }
                    }
                    else {
                        log(
                            "connect(%d, ?, ?) returned %d (%s:%d)",
                            descriptor, retval, __FILE__, __LINE__
                        );
                    }
                }
                else success = true;

                retval = sigprocmask(SIG_SETMASK, &sigset_orig, nullptr);

                if (retval == -1) {
                    int code = errno;
                    log(
                        "sigprocmask: %s (%s:%d)", strerror(code),
                        __FILE__, __LINE__
                    );
                }
                else if (retval) {
                    log(
                        "sigprocmask: unexpected return value %d (%s:%d)",
                        retval, __FILE__, __LINE__
                    );
                }

                if (success) break;
            }
        }

        if (!close_and_deinit(descriptor)) {
            log(
                "failed to close descriptor %d (%s:%d)", descriptor,
                __FILE__, __LINE__
            );

            release(find_jack(descriptor));
        }

        descriptor = NO_DESCRIPTOR;
    }

    CleanUp:
    if (info) freeaddrinfo(info);

    return descriptor;
}

size_t SOCKETS::close_and_deinit(int descriptor) noexcept {
    // Returns the number of descriptors successfully closed as a result.

    if (descriptor == NO_DESCRIPTOR) {
        log(
            "unexpected descriptor %d (%s:%d)", descriptor,
            __FILE__, __LINE__
        );

        return 0;
    }

    // Let's block all signals before calling close because we don't
    // want it to fail due to getting interrupted by a singal.
    int retval = sigprocmask(SIG_SETMASK, &sigset_all, &sigset_orig);
    if (retval == -1) {
        int code = errno;
        log(
            "sigprocmask: %s (%s:%d)", strerror(code),
            __FILE__, __LINE__
        );
        return 0;
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );

        return 0;
    }

    size_t closed = 0;
    retval = close(descriptor);

    if (retval) {
        if (retval == -1) {
            int code = errno;

            log(
                "close(%d): %s (%s:%d)",
                descriptor, strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "close(%d): unexpected return value %d (%s:%d)",
                descriptor, retval, __FILE__, __LINE__
            );
        }
    }
    else {
        ++closed;

        JACK *found = find_jack(descriptor);
        int close_children_of = NO_DESCRIPTOR;

        if (found->parent == NO_DESCRIPTOR) {
            close_children_of = descriptor;
        }

        if (!found) {
            log(
                "descriptor %d closed but jack not found (%s:%d)",
                descriptor, __FILE__, __LINE__
            );
        }
        else release(found);

        if (close_children_of != NO_DESCRIPTOR) {
            static constexpr const size_t descriptor_buffer_length = 1024;
            int descriptor_buffer[descriptor_buffer_length];
            size_t to_be_closed = 0;

            INDEX &index = get_index(INDEX::TYPE::DESCRIPTOR_JACK);

            Again:

            for (size_t bucket=0; bucket < index.buckets; ++bucket) {
                // TODO: optimize this (use a special index?)
                for (size_t i=0; i<index.table[bucket].value.size; ++i) {
                    JACK *rec{
                        to_jack(get_entry(index.table[bucket].value, i))
                    };

                    if (rec->parent != close_children_of) {
                        continue;
                    }

                    if (to_be_closed < descriptor_buffer_length) {
                        descriptor_buffer[to_be_closed++] = rec->descriptor;
                    }
                    else {
                        goto CloseDescriptors;
                    }
                }
            }

            CloseDescriptors:

            if (to_be_closed) {
                for (size_t i=0; i<to_be_closed; ++i) {
                    int d = descriptor_buffer[i];

                    retval = close(d);

                    if (retval == -1) {
                        int code = errno;
                        log(
                            "close(%d): %s (%s:%d)", d,
                            strerror(code), __FILE__, __LINE__
                        );
                    }
                    else if (retval != 0) {
                        log(
                            "close(%d): unexpected return value %d (%s:%d)",
                            d, retval, __FILE__, __LINE__
                        );
                    }
                    else {
                        found = find_jack(d);

                        if (!found) {
                            log(
                                "descriptor %d closed but jack not found "
                                "(%s:%d)", d, __FILE__, __LINE__
                            );
                        }
                        else release(found);

                        ++closed;
                    }
                }

                to_be_closed = 0;

                goto Again;
            }
        }
    }

    retval = sigprocmask(SIG_SETMASK, &sigset_orig, nullptr);
    if (retval == -1) {
        int code = errno;
        log(
            "sigprocmask: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );
    }

    return closed;
}

SOCKETS::ERROR SOCKETS::capture(const JACK &copy) noexcept {
    if (copy.descriptor == NO_DESCRIPTOR) {
        die();
    }

    int descriptor = copy.descriptor;
    int group = copy.group;

    JACK *jack = new_jack(&copy);

    if (!jack) {
        return ERROR::OUT_OF_MEMORY;
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::DESCRIPTOR_JACK,
            make_key(descriptor), make_pipe_entry(jack)
        )
    };

    if (!entry.valid) {
        deallocate(get_memory(jack));

        return entry.error;
    }

    {
        // If the newly pushed jack has not its group set to zero at first, then
        // set_group would falsely reduce the group size.

        jack->group = 0;
    }

    return set_group(descriptor, group);
}

void SOCKETS::release(JACK *jack) noexcept {
    if (!jack) {
        return bug();
    }

    // First, let's free the events.
    for (auto &ev : jack->event_lookup) {
        rem_event(*jack, static_cast<EVENT>(&ev - &(jack->event_lookup[0])));
    }

    // Then, we free the dynamically allocated memory.
    destroy(jack->epoll_ev);
    destroy(jack->incoming);
    destroy(jack->outgoing);

    if (jack->blacklist) {
        freeaddrinfo(jack->blacklist);
    }

    rem_group(jack->descriptor);

    // Finally, we remove the jack.
    size_t erased{
        erase(INDEX::TYPE::DESCRIPTOR_JACK, make_key(jack->descriptor))
    };

    if (!erased) die();

    deallocate(get_memory(jack)); // TODO: recycle instead
}

const SOCKETS::JACK *SOCKETS::find_jack(
    int descriptor
) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::DESCRIPTOR_JACK, make_key(descriptor))
    };

    if (!entry.valid) {
        return nullptr;
    }

    return to_jack(get_entry(*entry.val_pipe, entry.index));
}

SOCKETS::JACK *SOCKETS::find_jack(int descriptor) noexcept {
    return const_cast<JACK *>(
        static_cast<const SOCKETS &>(*this).find_jack(descriptor)
    );
}

const SOCKETS::JACK *SOCKETS::find_jack(EVENT ev) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::EVENT_DESCRIPTOR, make_key(ev))
    };

    if (entry.valid) {
        const int descriptor = to_int(get_value(entry));
        return &get_jack(descriptor); // If it's indexed, then it must exist.
    }

    return nullptr;
}

SOCKETS::JACK *SOCKETS::find_jack(EVENT ev) noexcept {
    return const_cast<JACK *>(
        static_cast<const SOCKETS &>(*this).find_jack(ev)
    );
}

const SOCKETS::JACK *SOCKETS::find_epoll_jack() const noexcept {
    return find_jack(EVENT::EPOLL);
}

SOCKETS::JACK *SOCKETS::find_epoll_jack() noexcept {
    return const_cast<JACK *>(
        static_cast<const SOCKETS &>(*this).find_epoll_jack()
    );
}

const SOCKETS::JACK &SOCKETS::get_jack(int descriptor) const noexcept {
    const JACK *rec = find_jack(descriptor);

    if (!rec) die();

    return *rec;
}

SOCKETS::JACK &SOCKETS::get_jack(int descriptor) noexcept {
    JACK *rec = find_jack(descriptor);

    if (!rec) die();

    return *rec;
}

const SOCKETS::JACK &SOCKETS::get_epoll_jack() const noexcept {
    const JACK *rec = find_epoll_jack();

    if (!rec) die();

    return *rec;
}

SOCKETS::JACK &SOCKETS::get_epoll_jack() noexcept {
    JACK *rec = find_epoll_jack();

    if (!rec) die();

    return *rec;
}

const SOCKETS::PIPE *SOCKETS::find_descriptors(EVENT ev) const noexcept {
    INDEX::ENTRY entry{find(INDEX::TYPE::EVENT_DESCRIPTOR, make_key(ev))};

    if (entry.valid) {
        return entry.val_pipe;
    }

    return nullptr;
}

SOCKETS::PIPE &SOCKETS::get_buffer(BUFFER buffer) noexcept {
    size_t index = static_cast<size_t>(buffer);

    if (index >= std::extent<decltype(buffers)>::value) die();

    return buffers[index];
}

SOCKETS::INDEX &SOCKETS::get_index(INDEX::TYPE index_type) noexcept {
    size_t i = static_cast<size_t>(index_type);

    if (i >= std::extent<decltype(indices)>::value) die();

    return indices[i];
}

SOCKETS::JACK *SOCKETS::to_jack(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::JACK_PTR) die();

    return static_cast<JACK *>(entry.as_ptr);
}

SOCKETS::MEMORY *SOCKETS::to_memory(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::MEMORY_PTR) die();

    return static_cast<MEMORY *>(entry.as_ptr);
}

int SOCKETS::to_int(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::INT) die();

    return entry.as_int;
}

uint64_t SOCKETS::to_uint64(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::UINT64) die();

    return entry.as_uint64;
}

int *SOCKETS::to_int(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::INT) die();

    return static_cast<int *>(pipe.data);
}

uint8_t *SOCKETS::to_uint8(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::UINT8) die();

    return static_cast<uint8_t *>(pipe.data);
}

uint64_t *SOCKETS::to_uint64(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::UINT64) die();

    return static_cast<uint64_t *>(pipe.data);
}

SOCKETS::KEY *SOCKETS::to_key(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::KEY) die();

    return static_cast<KEY *>(pipe.data);
}

epoll_event *SOCKETS::to_epoll_event(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::EPOLL_EVENT) die();

    return static_cast<epoll_event *>(pipe.data);
}

void **SOCKETS::to_ptr(const PIPE &pipe) const noexcept {
    switch (pipe.type) {
        case PIPE::TYPE::PTR:
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR: {
            break;
        }
        case PIPE::TYPE::UINT8:
        case PIPE::TYPE::UINT64:
        case PIPE::TYPE::INT:
        case PIPE::TYPE::KEY:
        case PIPE::TYPE::EPOLL_EVENT:
        case PIPE::TYPE::NONE: {
            die();
            return nullptr;
        }
    }

    return static_cast<void **>(pipe.data);
}

bool SOCKETS::modify_epoll(int descriptor, uint32_t events) noexcept {
    JACK &epoll_jack = get_epoll_jack();
    int epoll_descriptor = epoll_jack.descriptor;
    epoll_event event{ make_epoll_event(descriptor, events) };

    int retval = epoll_ctl(
        epoll_descriptor, EPOLL_CTL_MOD, descriptor, &event
    );

    if (retval != 0) {
        if (retval == -1) {
            int code = errno;

            log(
                "epoll_ctl: %s (%s:%d)", strerror(code), __FILE__, __LINE__
            );
        }
        else {
            log(
                "epoll_ctl: unexpected return value %d (%s:%d)", retval,
                __FILE__, __LINE__
            );
        }

        return false;
    }

    return true;
}

SOCKETS::ERROR SOCKETS::set_group(int descriptor, int group) noexcept {
    JACK &jack = get_jack(descriptor);

    INDEX::ENTRY found{find(INDEX::TYPE::GROUP_SIZE, make_key(jack.group))};

    if (found.valid) {
        if (found.val_pipe->type == PIPE::TYPE::UINT64) {
            uint64_t &value = to_uint64(*found.val_pipe)[found.index];

            if (--value == 0) {
                erase(INDEX::TYPE::GROUP_SIZE, make_key(jack.group));
            }
        }
        else die();
    }

    jack.group = group;

    if (group == 0) {
        // 0 stands for no group. We don't keep track of its size.
        return ERROR::NONE;
    }

    found = find(INDEX::TYPE::GROUP_SIZE, make_key(group));

    if (found.valid) {
        if (found.val_pipe->type == PIPE::TYPE::UINT64) {
            uint64_t &value = to_uint64(*found.val_pipe)[found.index];
            ++value;
        }
        else die();
    }
    else {
        return insert(
            INDEX::TYPE::GROUP_SIZE, make_key(group), make_pipe_entry(1)
        ).error;
    }

    return ERROR::NONE;
}

void SOCKETS::rem_group(int descriptor) noexcept {
    if (set_group(descriptor, 0) != ERROR::NONE) {
        die(); // Removing descriptor from its group should never fail.
    }
}

SOCKETS::ERROR SOCKETS::set_event(
    JACK &jack, EVENT event, bool value
) noexcept {
    if (value == false) {
        rem_event(jack, event);
        return ERROR::NONE;
    }

    size_t index = static_cast<size_t>(event);

    if (index >= std::extent<decltype(jack.event_lookup)>::value) {
        return die();
    }

    uint32_t pos = jack.event_lookup[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        return ERROR::NONE; // Already set.
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::EVENT_DESCRIPTOR,
            make_key(event), make_pipe_entry(jack.descriptor)
        )
    };

    if (entry.valid) {
        if (entry.index >= std::numeric_limits<uint32_t>::max()) {
            die(); // the number of descriptors is limited by the max of int.
        }

        jack.event_lookup[index] = uint32_t(entry.index);
        return ERROR::NONE;
    }

    return entry.error;
}

void SOCKETS::rem_event(JACK &jack, EVENT event) noexcept {
    size_t index = static_cast<size_t>(event);

    if (index >= std::extent<decltype(jack.event_lookup)>::value) {
        die();
    }

    uint32_t pos = jack.event_lookup[index];

    if (pos == std::numeric_limits<uint32_t>::max()) {
        return;
    }

    size_t erased = erase(
        INDEX::TYPE::EVENT_DESCRIPTOR,
        make_key(event), make_pipe_entry(jack.descriptor), pos, 1
    );

    if (!erased) {
        die();
        return;
    }

    INDEX::ENTRY entry{
        find(INDEX::TYPE::EVENT_DESCRIPTOR, make_key(event), {}, pos, 1)
    };

    if (entry.valid && entry.index == pos) {
        int other_descriptor = ((int *) entry.val_pipe->data)[entry.index];
        get_jack(other_descriptor).event_lookup[index] = pos;
    }

    jack.event_lookup[index] = std::numeric_limits<uint32_t>::max();
}

bool SOCKETS::has_event(const JACK &jack, EVENT event) const noexcept {
    size_t index = static_cast<size_t>(event);

    if (index >= std::extent<decltype(jack.event_lookup)>::value) {
        return false;
    }

    uint32_t pos = jack.event_lookup[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        return true;
    }

    return false;
}

SOCKETS::INDEX::ENTRY SOCKETS::find(
    INDEX::TYPE index_type, KEY key, PIPE::ENTRY value,
    size_t start_i, size_t iterations
) const noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key.value % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::KEY) {
        die();
    }

    const KEY *data = to_key(key_pipe);

    if (!data) {
        return {};
    }

    PIPE &value_pipe = table.value;

    if (value.type != PIPE::TYPE::NONE && value.type != value_pipe.type) {
        die();
    }

    size_t sz = key_pipe.size;
    size_t i = std::min(sz-1, start_i);

    for (; i<sz && iterations; --i, --iterations) {
        if (data[i].value != key.value) {
            continue;
        }

        if (value.type != PIPE::TYPE::NONE) {
            switch (value_pipe.type) {
                case PIPE::TYPE::UINT8: {
                    const uint8_t *vd = (const uint8_t *) value_pipe.data;

                    if (vd[i] != value.as_uint8) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::UINT64: {
                    const uint64_t *vd = (const uint64_t *) value_pipe.data;

                    if (vd[i] != value.as_uint64) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::INT: {
                    const int *vd = (const int *) value_pipe.data;

                    if (vd[i] != value.as_int) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::MEMORY_PTR:
                case PIPE::TYPE::JACK_PTR:
                case PIPE::TYPE::PTR: {
                    const void **vd = (const void **) value_pipe.data;

                    if (vd[i] != value.as_ptr) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::KEY: {
                    const KEY *vd = (const KEY *) value_pipe.data;

                    if (std::memcmp(vd+i, &value.as_key, sizeof(KEY))) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::EPOLL_EVENT: {
                    static constexpr const size_t epoll_ev_sz{
                        sizeof(epoll_event)
                    };

                    const epoll_event *vd = (
                        (const epoll_event *) value_pipe.data
                    );

                    if (std::memcmp(vd+i, &value.as_epoll_event, epoll_ev_sz)) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::NONE: die(); continue;
            }
        }

        INDEX::ENTRY entry{};

        entry.index = i;
        entry.valid = true;
        entry.key_pipe = &key_pipe;
        entry.val_pipe = &value_pipe;

        return entry;
    }

    return {};
}

SOCKETS::INDEX::ENTRY SOCKETS::insert(
    INDEX::TYPE index_type, KEY key, PIPE::ENTRY value
) noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (!index.multimap) {
        INDEX::ENTRY found{ find(index_type, key) };

        if (found.valid) {
            return make_index_entry(
                *found.key_pipe, *found.val_pipe, found.index,
                insert(*found.val_pipe, found.index, value)
            );
        }
    }

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key.value % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::KEY) die();

    PIPE &val_pipe = table.value;

    size_t old_size = key_pipe.size;

    ERROR error{insert(key_pipe, make_pipe_entry(key))};

    if (error == ERROR::NONE) {
        error = insert(val_pipe, value);

        if (error != ERROR::NONE) {
            if (key_pipe.size > old_size) {
                --key_pipe.size;
            }
            else die();
        }
    }

    return make_index_entry(key_pipe, val_pipe, old_size, error);
}

size_t SOCKETS::erase(
    INDEX::TYPE index_type, KEY key, PIPE::ENTRY value,
    size_t start_i, size_t iterations
) noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key.value % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::KEY) die();

    KEY *key_data = to_key(key_pipe);

    size_t erased = 0;

    if (!key_data) {
        return erased;
    }

    PIPE &val_pipe = table.value;

    if (value.type != PIPE::TYPE::NONE && value.type != val_pipe.type) {
        die();
    }

    size_t i{
        // We start from the end because erasing the last element is fast.

        std::min(
            key_pipe.size - 1, start_i
        )
    };

    for (; i < key_pipe.size && iterations; --iterations) {
        if (key_data[i].value != key.value) {
            --i;
            continue;
        }

        if (value.type != PIPE::TYPE::NONE) {
            switch (val_pipe.type) {
                case PIPE::TYPE::UINT8: {
                    const uint8_t *vd = (const uint8_t *) val_pipe.data;

                    if (vd[i] != value.as_uint8) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::UINT64: {
                    const uint64_t *vd = (const uint64_t *) val_pipe.data;

                    if (vd[i] != value.as_uint64) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::INT: {
                    const int *vd = (const int *) val_pipe.data;

                    if (vd[i] != value.as_int) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::MEMORY_PTR:
                case PIPE::TYPE::JACK_PTR:
                case PIPE::TYPE::PTR: {
                    const void **vd = (const void **) val_pipe.data;

                    if (vd[i] != value.as_ptr) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::KEY: {
                    const KEY *vd = (const KEY *) val_pipe.data;

                    if (std::memcmp(vd+i, &value.as_key, sizeof(KEY))) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::EPOLL_EVENT: {
                    static constexpr const size_t epoll_ev_sz{
                        sizeof(epoll_event)
                    };
                    const epoll_event *vd = (const epoll_event *) val_pipe.data;

                    if (std::memcmp(vd+i, &value.as_epoll_event, epoll_ev_sz)) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::NONE: die(); continue;
            }
        }

        erase(key_pipe, i);
        erase(val_pipe, i);

        ++erased;

        if (index.multimap) {
            if (i == key_pipe.size) --i;

            continue;
        }

        break;
    }

    return erased;
}

size_t SOCKETS::count(INDEX::TYPE index_type, KEY key) const noexcept {
    size_t count = 0;
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets > 0) {
        const INDEX::TABLE &table = index.table[key.value % index.buckets];
        const PIPE &pipe = table.key;
        const KEY *data = to_key(pipe);

        if (data) {
            for (size_t i=0, sz=pipe.size; i<sz; ++i) {
                if (data[i].value == key.value) {
                    ++count;

                    if (!index.multimap) return count;
                }
            }
        }
    }
    else die();

    return count;
}

void SOCKETS::replace(
    PIPE &pipe, size_t index, PIPE::ENTRY value
) noexcept {
    if (index >= pipe.size) {
        die();
    }
    else if (pipe.type != value.type) {
        die();
    }

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            to_uint8(pipe)[index] = value.as_uint8;
            break;
        }
        case PIPE::TYPE::UINT64: {
            to_uint64(pipe)[index] = value.as_uint64;
            break;
        }
        case PIPE::TYPE::INT: {
            to_int(pipe)[index] = value.as_int;
            break;
        }
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            to_ptr(pipe)[index] = value.as_ptr;
            break;
        }
        case PIPE::TYPE::KEY: {
            to_key(pipe)[index] = value.as_key;
            break;
        }
        case PIPE::TYPE::EPOLL_EVENT: {
            to_epoll_event(pipe)[index] = value.as_epoll_event;
            break;
        }
        case PIPE::TYPE::NONE: die();
    }
}

SOCKETS::ERROR SOCKETS::insert(
    PIPE &pipe, size_t index, PIPE::ENTRY value
) noexcept {
    if (index > pipe.size) {
        die();
    }
    else if (pipe.type != value.type) {
        die();
    }
    else if (index == pipe.size) {
        if (pipe.size == pipe.capacity) {
            ERROR error = reserve(pipe, std::max(pipe.size * 2, size_t{1}));

            if (error != ERROR::NONE) {
                return error;
            }
        }

        ++pipe.size;
    }

    replace(pipe, index, value);

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::insert(PIPE &pipe, PIPE::ENTRY value) noexcept {
    return insert(pipe, pipe.size, value);
}

SOCKETS::ERROR SOCKETS::reserve(PIPE &pipe, size_t capacity) noexcept {
    if (pipe.capacity >= capacity) {
        return ERROR::NONE;
    }

    size_t element_size = 0;

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            element_size = sizeof(uint8_t); // TODO: pipe.type -> size function
            break;
        }
        case PIPE::TYPE::UINT64: {
            element_size = sizeof(uint64_t);
            break;
        }
        case PIPE::TYPE::INT: {
            element_size = sizeof(int);
            break;
        }
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            element_size = sizeof(void*);
            break;
        }
        case PIPE::TYPE::KEY: {
            element_size = sizeof(KEY);
            break;
        }
        case PIPE::TYPE::EPOLL_EVENT: {
            element_size = sizeof(epoll_event);
            break;
        }
        case PIPE::TYPE::NONE: return die();
    }

    size_t byte_count = element_size * capacity;

    if (!byte_count) {
        die();
        return ERROR::FORBIDDEN_CONDITION;
    }

    MEMORY *new_memory = allocate(byte_count);

    if (!new_memory) {
        return ERROR::OUT_OF_MEMORY;
    }

    MEMORY *old_memory = pipe.memory;
    void *old_data = pipe.data;
    void *new_data = new_memory->data;

    if (old_data) {
        std::memcpy(new_data, old_data, pipe.size * element_size);
    }

    if (old_memory) {
        deallocate(*old_memory);
    }

    pipe.memory = new_memory;
    pipe.data = new_data;
    pipe.capacity = capacity;

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::swap(PIPE &first, PIPE &second) noexcept {
    if (first.type != second.type) {
        return die();
    }

    std::swap(first.capacity, second.capacity);
    std::swap(first.size,     second.size);
    std::swap(first.data,     second.data);
    std::swap(first.memory,   second.memory);

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::copy(const PIPE &src, PIPE &dst) noexcept {
    if (&src == &dst) {
        return ERROR::NONE;
    }

    if (src.type != dst.type || dst.type == PIPE::TYPE::NONE) {
        return die();
    }

    dst.size = 0;

    return append(src, dst);
}

SOCKETS::ERROR SOCKETS::append(const PIPE &src, PIPE &dst) noexcept {
    if (src.type != dst.type || dst.type == PIPE::TYPE::NONE) {
        return die();
    }

    size_t old_size = dst.size;
    size_t new_size = old_size + src.size;

    if (new_size > dst.capacity) {
        ERROR error = reserve(dst, new_size);

        if (error != ERROR::NONE) {
            return error;
        }
    }

    size_t count = src.size;

    dst.size = new_size;

    if (src.data == nullptr) {
        return ERROR::NONE;
    }

    switch (dst.type) {
        case PIPE::TYPE::UINT8: {
            std::memcpy(
                static_cast<uint8_t *>(dst.data) + old_size, src.data,
                count * sizeof(uint8_t)
            );
            break;
        }
        case PIPE::TYPE::UINT64: {
            std::memcpy(
                static_cast<uint64_t *>(dst.data) + old_size, src.data,
                count * sizeof(uint64_t)
            );
            break;
        }
        case PIPE::TYPE::INT: {
            std::memcpy(
                static_cast<int *>(dst.data) + old_size, src.data,
                count * sizeof(int)
            );
            break;
        }
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            std::memcpy(
                static_cast<void **>(dst.data) + old_size, src.data,
                count * sizeof(void*)
            );
            break;
        }
        case PIPE::TYPE::KEY: {
            std::memcpy(
                static_cast<KEY *>(dst.data) + old_size, src.data,
                count * sizeof(KEY)
            );
            break;
        }
        case PIPE::TYPE::EPOLL_EVENT: {
            std::memcpy(
                static_cast<epoll_event *>(dst.data) + old_size, src.data,
                count * sizeof(epoll_event)
            );
            break;
        }
        case PIPE::TYPE::NONE: return die();
    }

    return ERROR::NONE;
}

void SOCKETS::erase(PIPE &pipe, size_t index) noexcept {
    if (index >= pipe.size) {
        die();
    }

    if (index + 1 >= pipe.size) {
        --pipe.size;
        return;
    }

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            uint8_t *data = (uint8_t *) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::UINT64: {
            uint64_t *data = (uint64_t *) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::INT: {
            int *data = (int *) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            void **data = (void **) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::KEY: {
            KEY *data = (KEY *) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::EPOLL_EVENT: {
            epoll_event *data = (epoll_event *) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::NONE: die(); return;
    }

    --pipe.size;
}

SOCKETS::PIPE::ENTRY SOCKETS::pop_back(PIPE &pipe) noexcept {
    size_t size = pipe.size;

    if (!size) {
        die();
        return {};
    }

    PIPE::ENTRY entry{get_last(pipe)};

    erase(pipe, size - 1);

    return entry;
}

SOCKETS::PIPE::ENTRY SOCKETS::get_last(const PIPE &pipe) const noexcept {
    size_t size = pipe.size;

    if (!size) {
        die();
        return {};
    }

    return get_entry(pipe, size - 1);
}

SOCKETS::PIPE::ENTRY SOCKETS::get_entry(
    const PIPE &pipe, size_t index
) const noexcept {
    if (index >= pipe.size) {
        die();
        return {};
    }

    void *data = pipe.data;
    PIPE::ENTRY entry{};

    entry.type = pipe.type;

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            entry.as_uint8 = static_cast<uint8_t *>(data)[index];
            break;
        }
        case PIPE::TYPE::UINT64: {
            entry.as_uint64 = static_cast<uint64_t *>(data)[index];
            break;
        }
        case PIPE::TYPE::INT: {
            entry.as_int = static_cast<int *>(data)[index];
            break;
        }
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            entry.as_ptr = static_cast<void **>(data)[index];
            break;
        }
        case PIPE::TYPE::KEY: {
            entry.as_key = static_cast<KEY *>(data)[index];
            break;
        }
        case PIPE::TYPE::EPOLL_EVENT: {
            entry.as_epoll_event = static_cast<epoll_event *>(data)[index];
            break;
        }
        case PIPE::TYPE::NONE: die(); break;
    }

    return entry;
}

void SOCKETS::set_value(
    INDEX::ENTRY index_entry, PIPE::ENTRY pipe_entry
) noexcept {
    replace(*index_entry.val_pipe, index_entry.index, pipe_entry);
}

SOCKETS::PIPE::ENTRY SOCKETS::get_value(INDEX::ENTRY entry) const noexcept {
    return get_entry(*entry.val_pipe, entry.index);
}

void SOCKETS::destroy(PIPE &pipe) noexcept {
    if (pipe.memory) {
        deallocate(*pipe.memory);
        pipe.memory = nullptr;
    }

    pipe.data = nullptr;
    pipe.capacity = 0;
    pipe.size = 0;
}

const SOCKETS::MEMORY *SOCKETS::find_memory(
    const void *resource
) const noexcept {
    INDEX::ENTRY entry{
        find(
            INDEX::TYPE::RESOURCE_MEMORY,
            make_key(reinterpret_cast<uintptr_t>(resource))
        )
    };

    if (entry.valid) {
        return to_memory(get_value(entry));
    }

    die();

    return nullptr;
}

SOCKETS::MEMORY *SOCKETS::find_memory(const void *resource) noexcept {
    return const_cast<MEMORY *>(
        static_cast<const SOCKETS &>(*this).find_memory(resource)
    );
}

const SOCKETS::MEMORY &SOCKETS::get_memory(
    const void *resource
) const noexcept {
    const MEMORY *memory = find_memory(resource);

    if (!memory) die();

    return *memory;
}

SOCKETS::MEMORY &SOCKETS::get_memory(const void *resource) noexcept {
    MEMORY *memory = find_memory(resource);

    if (!memory) die();

    return *memory;
}

SOCKETS::MEMORY *SOCKETS::allocate(size_t byte_count) noexcept {
    const size_t total_size = sizeof(MEMORY) + byte_count;
    uint8_t *array = new (std::nothrow) uint8_t[total_size];

    if (!array) {
        return nullptr;
    }

    MEMORY *memory = reinterpret_cast<MEMORY *>(array);

    memory->size = byte_count;
    memory->data = byte_count ? (array + sizeof(MEMORY)) : nullptr;
    memory->next = allocated;
    memory->prev = nullptr;
    memory->indexed = false;

    if (allocated) {
        allocated->prev = memory;
    }

    allocated = memory;

    log("Allocated %lu byte%s.", total_size, total_size == 1 ? "" : "s");

    return memory;
}

SOCKETS::MEMORY *SOCKETS::allocate_and_index(
    size_t byte_count, const void *copy
) noexcept {
    MEMORY *memory = allocate(byte_count);

    if (!memory) {
        return nullptr;
    }

    if (copy) {
        std::memcpy(memory->data, copy, memory->size);
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::RESOURCE_MEMORY,
            make_key(reinterpret_cast<uintptr_t>(memory->data)),
            make_pipe_entry(memory)
        )
    };

    if (entry.valid) {
        memory->indexed = true;
    }
    else {
        deallocate(*memory);
        return nullptr;
    }

    return memory;
}

void SOCKETS::deallocate(MEMORY &memory) noexcept {
    if (memory.indexed) {
        const KEY key{make_key(reinterpret_cast<uintptr_t>(memory.data))};
        INDEX::ENTRY entry{ find(INDEX::TYPE::RESOURCE_MEMORY, key) };

        size_t erased{
            entry.valid ? (
                erase(INDEX::TYPE::RESOURCE_MEMORY, key, {}, entry.index, 1)
            ) : 0
        };

        if (!erased) {
            die();
            return;
        }
    }

    if (allocated == &memory) {
        allocated = memory.next;

        if (allocated) {
            allocated->prev = nullptr;
        }
    }
    else {
        memory.prev->next = memory.next;

        if (memory.next) {
            memory.next->prev = memory.prev;
        }
    }

    const size_t total_size = sizeof(MEMORY) + memory.size;

    delete [] reinterpret_cast<uint8_t *>(&memory);

    log("Deallocated %lu byte%s.", total_size, total_size == 1 ? "" : "s");
}

SOCKETS::JACK *SOCKETS::new_jack(const JACK *copy) noexcept {
    MEMORY *mem = allocate_and_index(sizeof(JACK), copy);

    if (mem) {
        return (JACK *) mem->data;
    }

    return nullptr;
}

void SOCKETS::dump(const char *file, int line) const noexcept {
    for (size_t i=0; i<size_t(EVENT::MAX_EVENTS); ++i) {
        const PIPE *found = find_descriptors(static_cast<EVENT>(i));

        if (found && found->size) {
            printf("event[%lu] of %s, line %d:\n", i, file, line);
            for (size_t j=0; j<found->size; ++j) {
                printf("%d ", ((int *) found->data)[j]);
            }
            printf("\n");
        }
    }
}

constexpr SOCKETS::JACK SOCKETS::make_jack(
    int descriptor, int parent, int group
) noexcept {
#if __cplusplus <= 201703L
    __extension__
#endif
    JACK jack{
        .event_lookup = {},
        .epoll_ev     = { make_pipe(PIPE::TYPE::EPOLL_EVENT) },
        .incoming     = { make_pipe(PIPE::TYPE::UINT8) },
        .outgoing     = { make_pipe(PIPE::TYPE::UINT8) },
        .host         = {'\0'},
        .port         = {'\0'},
        .descriptor   = descriptor,
        .parent       = parent,
        .group        = group,
        .ai_family    = 0,
        .ai_flags     = 0,
        .blacklist    = nullptr,
        .bitset       = {}
    };

    for (auto &lookup_value : jack.event_lookup) {
        lookup_value = std::numeric_limits<uint32_t>::max();
    }

    return jack;
}

constexpr SOCKETS::ALERT SOCKETS::make_alert(
    int descriptor, EVENT event, bool valid
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    ALERT{
        .descriptor = descriptor,
        .event = event,
        .valid = valid
    };
}

constexpr struct SOCKETS::INDEX::ENTRY SOCKETS::make_index_entry(
    PIPE &keys, PIPE &values, size_t index, ERROR error, bool valid
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::INDEX::ENTRY{
        .key_pipe = &keys,
        .val_pipe = &values,
        .index    = index,
        .error    = error,
        .valid    = valid
    };
}

constexpr struct SOCKETS::INDEX::ENTRY SOCKETS::make_index_entry(
    PIPE &keys, PIPE &values, size_t index, ERROR error
) noexcept {
    return make_index_entry(keys, values, index, error, error == ERROR::NONE);
}

constexpr SOCKETS::PIPE SOCKETS::make_pipe(
    const uint8_t *data, size_t size
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE{
        .capacity = size,
        .size = size,
        .data = const_cast<uint8_t *>(data),
        .type = PIPE::TYPE::UINT8,
        .memory = nullptr
    };
}

constexpr SOCKETS::PIPE SOCKETS::make_pipe(PIPE::TYPE type) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE{
        .capacity = 0,
        .size = 0,
        .data = nullptr,
        .type = type,
        .memory = nullptr
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    PIPE::TYPE type
) noexcept {
    PIPE::ENTRY entry{};
    entry.type = type;
    return entry;
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    uint64_t value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_uint64 = value,
        .type = PIPE::TYPE::UINT64
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    int value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_int = value,
        .type = PIPE::TYPE::INT
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    KEY value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_key = value,
        .type = PIPE::TYPE::KEY
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    JACK *value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_ptr = value,
        .type = PIPE::TYPE::JACK_PTR
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    MEMORY *value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_ptr = value,
        .type = PIPE::TYPE::MEMORY_PTR
    };
}

constexpr SOCKETS::KEY SOCKETS::make_key(uintptr_t val) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::KEY{
        .value = val
    };
}

constexpr SOCKETS::KEY SOCKETS::make_key(EVENT val) noexcept {
    return make_key(static_cast<uintptr_t>(val));
}

constexpr epoll_data_t SOCKETS::make_epoll_data(int descriptor) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    epoll_data_t{
        .fd = descriptor
    };
}

constexpr struct epoll_event SOCKETS::make_epoll_event(
    int descriptor, uint32_t events
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    epoll_event{
        .events = events,
        .data   = make_epoll_data(descriptor)
    };
}

bool SOCKETS::is_listed(const addrinfo &info, const addrinfo *list) noexcept {
    for (const struct addrinfo *next = list; next; next = next->ai_next) {
        if (info.ai_flags != next->ai_flags
        ||  info.ai_family != next->ai_family
        ||  info.ai_socktype != next->ai_socktype
        ||  info.ai_protocol != next->ai_protocol
        ||  info.ai_addrlen != next->ai_addrlen) {
            continue;
        }

        if ((info.ai_canonname && !next->ai_canonname)
        ||  (next->ai_canonname && !info.ai_canonname)) {
            continue;
        }

        if (info.ai_canonname && next->ai_canonname
        && std::strcmp(info.ai_canonname, next->ai_canonname)) {
            continue;
        }

        const void *first = (const void *) info.ai_addr;
        const void *second = (const void *) next->ai_addr;

        if (std::memcmp(first, second, (size_t) info.ai_addrlen) != 0) {
            continue;
        }

        return true;
    }

    return false;
}

const char *SOCKETS::get_code(ERROR error) noexcept {
    switch (error) {
        case ERROR::NONE:                return "NONE";
        case ERROR::OUT_OF_MEMORY:       return "OUT_OF_MEMORY";
        case ERROR::UNDEFINED_BEHAVIOR:  return "UNDEFINED_BEHAVIOR";
        case ERROR::FORBIDDEN_CONDITION: return "FORBIDDEN_CONDITION";
        case ERROR::UNHANDLED_EVENTS:    return "UNHANDLED_EVENTS";
    }

    return "UNKNOWN_ERROR";
}

SOCKETS::EVENT SOCKETS::next(EVENT event_type) noexcept {
    return static_cast<EVENT>(
        (static_cast<size_t>(event_type) + 1) % (
            static_cast<size_t>(EVENT::MAX_EVENTS)
        )
    );
}

#endif
