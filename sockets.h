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

    struct EVENT {
        enum class TYPE : uint8_t {
            NONE = 0,
            CONNECTION,
            DISCONNECTION,
            INCOMING
        };

        int descriptor;
        TYPE type;
        bool valid:1;
    };

    static constexpr const EVENT::TYPE
        EV_NONE          = EVENT::TYPE::NONE,
        EV_CONNECTION    = EVENT::TYPE::CONNECTION,
        EV_DISCONNECTION = EVENT::TYPE::DISCONNECTION,
        EV_INCOMING      = EVENT::TYPE::INCOMING;

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
    EVENT next_event() noexcept;

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
    static constexpr const int EPOLL_MAX_EVENTS  = 64;

    enum class FLAG : uint8_t {
        NONE = 0,
        // Do not change the order of the flags above this line.
        RECONNECT,
        READ,
        WRITE,
        ACCEPT,
        NEW_CONNECTION,
        DISCONNECT,
        CLOSE,
        INCOMING,
        MAY_SHUTDOWN,
        LISTENER,
        CONNECTING,
        // Do not change the order of the flags below this line.
        EPOLL,
        MAX_FLAGS
    };

    enum class BUFFER : uint8_t {
        GENERIC_INT,
        GENERIC_BYTE,
        SERVE,
        WRITEF,
        HANDLE_READ,
        HANDLE_WRITE,
        // Do not change the order of items below this line.
        MAX_BUFFERS
    };

    struct MEMORY {
        size_t   size;
        uint8_t *data;
    };

    struct PIPE {
        enum class TYPE : uint8_t {
            NONE = 0,
            UINT8,
            UINT64,
            INT,
            PTR,
            JACK_PTR,
            MEMORY
        };

        struct ENTRY {
            union {
                uint8_t  as_uint8;
                uint64_t as_uint64;
                int      as_int;
                void    *as_ptr;
                MEMORY   as_memory;
            };
            TYPE type;
        };

        size_t capacity;
        size_t size;
        void *data;
        TYPE type;
    };

    struct INDEX {
        enum class TYPE : uint8_t {
            NONE = 0,
            // Do not change the order of the types above this line.
            GROUP_SIZE,
            FLAG_DESCRIPTOR,
            MEM_ADDR_INDEX,
            DESCRIPTOR_JACK,
            // Do not change the order of the types below this line.
            MAX_TYPES
        };

        struct ENTRY {
            const PIPE *key_pipe;
            const PIPE *val_pipe;
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

    struct jack_type {
        uint32_t flags[static_cast<size_t>(FLAG::MAX_FLAGS)]; // TODO: fix type
        epoll_event *events;
        PIPE incoming;
        PIPE outgoing;
        char host[NI_MAXHOST];
        char port[NI_MAXSERV];
        int descriptor; // TODO: typedef descriptor_type and use it instead
        int parent;
        int group;
        int ai_family;
        int ai_flags;
        struct addrinfo *blacklist;
        struct bitset_type {
            bool frozen:1;
        } bitset;
    };

    inline static constexpr MEMORY make_memory(
        uint8_t *data, size_t size
    ) noexcept;

    inline static constexpr struct jack_type make_jack(
        int descriptor, int parent, int group
    ) noexcept;

    inline static constexpr struct EVENT make_event(
        int descriptor, EVENT::TYPE type, bool valid =true
    ) noexcept;

    inline static constexpr struct INDEX::ENTRY make_index_entry(
        const PIPE &keys, const PIPE &values, size_t index, ERROR, bool valid
    ) noexcept;

    inline static constexpr struct INDEX::ENTRY make_index_entry(
        const PIPE &keys, const PIPE &values, size_t index, ERROR
    ) noexcept;

    inline static constexpr PIPE make_pipe(
        const uint8_t *data, size_t size
    ) noexcept;

    inline static constexpr PIPE make_pipe(PIPE::TYPE) noexcept;

    inline static constexpr PIPE::ENTRY make_pipe_entry(PIPE::TYPE ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(uint64_t   ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(int        ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(MEMORY     ) noexcept;
    inline static constexpr PIPE::ENTRY make_pipe_entry(jack_type *) noexcept;

    inline static bool is_listed(
        const addrinfo &info, const addrinfo *list
    ) noexcept;

    inline static FLAG next(FLAG) noexcept;

    ERROR handle_close(int descriptor) noexcept;
    ERROR handle_epoll(int epoll_descriptor, int timeout) noexcept;
    ERROR handle_read(int descriptor) noexcept;
    ERROR handle_write(int descriptor) noexcept;
    ERROR handle_accept(int descriptor) noexcept;

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

    [[nodiscard]] ERROR push(const jack_type jack) noexcept; // TODO: remove
    jack_type pop(int descriptor) noexcept; // TODO: remove

    const jack_type *find_jack(int descriptor) const noexcept;
    jack_type *find_jack(int descriptor) noexcept;
    const jack_type *find_jack(FLAG) const noexcept;
    jack_type *find_jack(FLAG) noexcept;
    const jack_type *find_epoll_jack() const noexcept;
    jack_type *find_epoll_jack() noexcept;
    const jack_type &get_jack(int descriptor) const noexcept;
    jack_type &get_jack(int descriptor) noexcept;
    const jack_type &get_epoll_jack() const noexcept;
    jack_type &get_epoll_jack() noexcept;
    const PIPE *find_descriptors(FLAG) const noexcept;

    [[nodiscard]] ERROR set_group(int descriptor, int group) noexcept;
    void rem_group(int descriptor) noexcept;
    /*TODO: [[nodiscard]]*/ ERROR set_flag(
        int descriptor, FLAG, bool val =true
    ) noexcept;
    void rem_flag(int descriptor, FLAG flag) noexcept;
    bool has_flag(const jack_type &rec, FLAG) const noexcept;
    bool has_flag(int descriptor, FLAG) const noexcept;

    size_t count(INDEX::TYPE, uint64_t key) const noexcept;
    INDEX::ENTRY find(
        INDEX::TYPE, uint64_t key, PIPE::ENTRY value ={},
        size_t start_i =std::numeric_limits<size_t>::max(),
        size_t iterations =std::numeric_limits<size_t>::max()
    ) const noexcept;
    size_t erase( // TODO: use a custom data type to represent key
        INDEX::TYPE, uint64_t key, PIPE::ENTRY value ={},
        size_t start_i =std::numeric_limits<size_t>::max(),
        size_t iterations =std::numeric_limits<size_t>::max()
    ) noexcept;
    [[nodiscard]] INDEX::ENTRY insert(
        INDEX::TYPE, uint64_t key, PIPE::ENTRY value
    ) noexcept;
    void erase(PIPE &pipe, size_t index) noexcept;
    void destroy(PIPE &pipe) noexcept;
    PIPE::ENTRY get_entry(const PIPE &pipe, size_t index) const noexcept;
    PIPE::ENTRY get_last(const PIPE &pipe) const noexcept;
    PIPE::ENTRY pop_back(PIPE &pipe) noexcept;
    [[nodiscard]] ERROR reserve(PIPE&, size_t capacity) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, size_t index, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR copy(const PIPE &src, PIPE &dst) noexcept;
    [[nodiscard]] ERROR append(const PIPE &src, PIPE &dst) noexcept;
    ERROR swap(PIPE &, PIPE &) noexcept;
    PIPE &get_buffer(BUFFER) noexcept;
    INDEX &get_index(INDEX::TYPE) noexcept;

    MEMORY     to_memory(PIPE::ENTRY) const noexcept;
    jack_type *to_jack  (PIPE::ENTRY) const noexcept;
    int        to_int   (PIPE::ENTRY) const noexcept;

    uint8_t *to_uint8 (PIPE &) const noexcept;

    const MEMORY *allocate(size_t bytes, const void *copy =nullptr) noexcept;
    void deallocate(const void *) noexcept;
    jack_type *new_jack(const jack_type *copy =nullptr) noexcept;

    void log(
        const char *fmt, ...
    ) const noexcept __attribute__((format(printf, 2, 3)));
    void bug(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;
    ERROR die(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;
    void out_of_memory(
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
    PIPE  mempool;
    FLAG  serving;
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
    log_callback(nullptr), indices{}, buffers{}, mempool{}, serving{},
    errored{}, bitset{} {
}

SOCKETS::~SOCKETS() {
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
    serving = FLAG::NONE;

    while (mempool.size) {
        deallocate(to_memory(pop_back(mempool)).data);
    }

    destroy(mempool);

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
            case INDEX::TYPE::FLAG_DESCRIPTOR: {
                index.buckets = static_cast<size_t>(FLAG::MAX_FLAGS);
                index.multimap = true;
                break;
            }
        }

        switch (index.type) {
            case INDEX::TYPE::NONE: continue;
            case INDEX::TYPE::FLAG_DESCRIPTOR:
            case INDEX::TYPE::MEM_ADDR_INDEX:
            case INDEX::TYPE::DESCRIPTOR_JACK:
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

            key_pipe.type = PIPE::TYPE::UINT64;

            switch (index.type) {
                case INDEX::TYPE::DESCRIPTOR_JACK: {
                    val_pipe.type = PIPE::TYPE::JACK_PTR;
                    break;
                }
                case INDEX::TYPE::MEM_ADDR_INDEX:
                case INDEX::TYPE::GROUP_SIZE: {
                    val_pipe.type = PIPE::TYPE::UINT64;
                    break;
                }
                case INDEX::TYPE::FLAG_DESCRIPTOR: {
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
            case BUFFER::SERVE:
            case BUFFER::GENERIC_INT: {
                pipe.type = PIPE::TYPE::INT;
                break;
            }
            case BUFFER::GENERIC_BYTE:
            case BUFFER::WRITEF: {
                pipe.type = PIPE::TYPE::UINT8;
                break;
            }
            case BUFFER::HANDLE_WRITE:
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

    mempool.type = PIPE::TYPE::MEMORY;

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
            int descriptor{
                to_jack(
                    get_last(descriptor_jack.table[bucket].value)
                )->descriptor
            };

            if (!close_and_deinit(descriptor)) {
                // If for some reason we couldn't close the descriptor,
                // we still need to deallocate the related memmory.

                pop(descriptor);
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

struct SOCKETS::EVENT SOCKETS::next_event() noexcept {
    int d;

    bitset.unhandled_events = false;

    while (( d = next_connection() ) != NO_DESCRIPTOR) {
        return make_event(d, EV_CONNECTION);
    }

    while (( d = next_incoming() ) != NO_DESCRIPTOR) {
        return make_event(d, EV_INCOMING);
    }

    while (( d = next_disconnection() ) != NO_DESCRIPTOR) {
        return make_event(d, EV_DISCONNECTION);
    }

    return make_event(NO_DESCRIPTOR, EV_NONE, false);
}

int SOCKETS::next_connection() noexcept {
    jack_type *jack = find_jack(FLAG::NEW_CONNECTION);

    if (jack) {
        int descriptor = jack->descriptor;
        rem_flag(descriptor, FLAG::NEW_CONNECTION);

        return descriptor;
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_disconnection() noexcept {
    if (find_jack(FLAG::NEW_CONNECTION)) {
        // We postpone reporting any disconnections until the application
        // has acknowledged all the new incoming connections. This prevents
        // us from reporting a disconnection event before its respective
        // connection event is reported.

        return NO_DESCRIPTOR;
    }

    jack_type *jack = find_jack(FLAG::DISCONNECT);

    if (jack) {
        int descriptor = jack->descriptor;

        if (set_flag(descriptor, FLAG::CLOSE) == ERROR::NONE) {
            rem_flag(descriptor, FLAG::DISCONNECT);

            return descriptor;
        }
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_incoming() noexcept {
    jack_type *jack = find_jack(FLAG::INCOMING);

    if (jack) {
        int descriptor = jack->descriptor;
        rem_flag(descriptor, FLAG::INCOMING);

        return descriptor;
    }

    return NO_DESCRIPTOR;
}

bool SOCKETS::is_listener(int descriptor) const noexcept {
    return has_flag(descriptor, FLAG::LISTENER);
}

int SOCKETS::get_group(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);
    return jack ? jack->group : 0;
}

size_t SOCKETS::get_group_size(int group) const noexcept {
    INDEX::ENTRY found{find(INDEX::TYPE::GROUP_SIZE, group)};

    if (found.valid) {
        return ((uint64_t *) found.val_pipe->data)[found.index];
    }

    return 0;
}

int SOCKETS::get_listener(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);
    return jack ? jack->parent : NO_DESCRIPTOR;
}

const char *SOCKETS::get_host(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);
    return jack ? jack->host : "";
}

const char *SOCKETS::get_port(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);
    return jack ? jack->port : "";
}

void SOCKETS::freeze(int descriptor) noexcept {
    jack_type *jack = find_jack(descriptor);

    if (jack) {
        jack->bitset.frozen = true;
    }
}

void SOCKETS::unfreeze(int descriptor) noexcept {
    if (!has_flag(descriptor, FLAG::DISCONNECT)
    &&  !has_flag(descriptor, FLAG::CLOSE)) {
        jack_type *jack = find_jack(descriptor);

        if (jack) {
            jack->bitset.frozen = false;
        }
    }
}

bool SOCKETS::is_frozen(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);

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

    if (find_jack(FLAG::NEW_CONNECTION)) {
        // We postpone serving any descriptors until the application has
        // acknowledged all the new incoming connections.

        if (!bitset.unhandled_events) {
            bitset.unhandled_events = true;
            return err(ERROR::NONE);
        }

        return err(ERROR::UNHANDLED_EVENTS);
    }

    PIPE &descriptor_buffer = get_buffer(BUFFER::SERVE);

    if (serving == FLAG::NONE) {
        bitset.timeout = false;
        serving = next(serving);
    }

    for (; serving != FLAG::NONE; serving = next(serving)) {
        FLAG flag = serving;

        switch (flag) {
            case FLAG::RECONNECT:
            case FLAG::LISTENER:
            case FLAG::INCOMING:
            case FLAG::NEW_CONNECTION:
            case FLAG::DISCONNECT:
            case FLAG::CONNECTING:
            case FLAG::MAY_SHUTDOWN: {
                // This flag has no handler and is to be ignored here.

                continue;
            }
            default: break;
        }

        const PIPE *flagged_descriptors = find_descriptors(flag);

        if (!flagged_descriptors) continue;

        ERROR error = copy(
            // TODO: if it is possible for descriptors to be closed and reused
            // during a single iteration cycle, then bad things would happen.
            // check if this is a case here and implement a fix if necessary.
            *flagged_descriptors, descriptor_buffer
        );

        if (error != ERROR::NONE) {
            return err(error);
        }

        for (size_t j=0, sz=descriptor_buffer.size; j<sz; ++j) {
            int d = to_int(get_entry(descriptor_buffer, j));
            const jack_type *jack = find_jack(d);

            if (jack == nullptr) continue;

            rem_flag(d, flag);

            ERROR error = ERROR::NONE;

            switch (flag) {
                case FLAG::EPOLL: {
                    error = handle_epoll(d, timeout);
                    break;
                }
                case FLAG::CLOSE: {
                    if (has_flag(d, FLAG::READ) && !jack->bitset.frozen) {
                        // Unless this descriptor is frozen, we postpone
                        // normal closing until there is nothing left to
                        // read from this descriptor.

                        set_flag(d, flag);
                        continue;
                    }

                    error = handle_close(d);
                    break;
                }
                case FLAG::ACCEPT: {
                    if (find_jack(FLAG::DISCONNECT) || jack->bitset.frozen) {
                        // We postpone the acceptance of new connections
                        // until all the recent disconnections have been
                        // acknowledged and the descriptor is not frozen.

                        set_flag(d, flag);
                        continue;
                    }

                    error = handle_accept(d);
                    break;
                }
                case FLAG::WRITE: {
                    if (jack->bitset.frozen) {
                        set_flag(d, flag);
                        continue;
                    }

                    error = handle_write(d);
                    break;
                }
                case FLAG::READ: {
                    if (jack->bitset.frozen) {
                        set_flag(d, flag);
                        continue;
                    }

                    error = handle_read(d);
                    break;
                }
                default: {
                    log(
                        "Flag %lu of descriptor %d was not handled.",
                        static_cast<size_t>(flag), d
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
    const jack_type *jack = find_jack(descriptor);

    if (jack) {
        return jack->incoming.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::outgoing(int descriptor) const noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (jack) {
        return jack->outgoing.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::read(int descriptor, void *buf, size_t count) noexcept {
    if (!count) return 0;

    jack_type *jack = find_jack(descriptor);

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
    jack_type *jack = find_jack(descriptor);

    if (!jack || jack->incoming.size == 0) {
        return "";
    }

    PIPE &buffer = get_buffer(BUFFER::GENERIC_BYTE);

    if (copy(jack->incoming, buffer) != ERROR::NONE
    ||  insert(buffer, make_pipe_entry(buffer.type)) != ERROR::NONE) {
        // TODO: implement better out-of-memory error handling (read partially).
        return "";
    }

    jack->incoming.size = 0;

    return (const char *) buffer.data;
}

SOCKETS::ERROR SOCKETS::write(
    int descriptor, const void *buf, size_t count
) noexcept {
    jack_type *jack = find_jack(descriptor);

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

        set_flag(descriptor, FLAG::WRITE, jack->outgoing.size);
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

    jack_type &jack = get_jack(descriptor);

    if (size_t(retval) < sizeof(stackbuf)) {
        const PIPE wrapper{
            make_pipe(reinterpret_cast<const uint8_t *>(stackbuf), retval)
        };

        ERROR error{ append(wrapper, jack.outgoing) };

        if (error != ERROR::NONE) {
            return error;
        }

        set_flag(descriptor, FLAG::WRITE, jack.outgoing.size);

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

        set_flag(descriptor, FLAG::WRITE, jack.outgoing.size);

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

void SOCKETS::out_of_memory(const char *file, int line) const noexcept {
    log("out of memory (%s:%d)", file, line);
}

SOCKETS::ERROR SOCKETS::handle_close(int descriptor) noexcept {
    if (has_flag(descriptor, FLAG::RECONNECT)) {
        jack_type &rec = get_jack(descriptor);

        int new_descriptor = connect(
            get_host(descriptor), get_port(descriptor),
            get_group(descriptor), rec.ai_family, rec.ai_flags, rec.blacklist
        );

        if (new_descriptor == NO_DESCRIPTOR) {
            rem_flag(descriptor, FLAG::RECONNECT);
        }
        else {
            jack_type &new_rec = get_jack(new_descriptor);

            struct addrinfo *blacklist = new_rec.blacklist;

            if (!blacklist) {
                blacklist = rec.blacklist;
                rec.blacklist = nullptr;
            }
            else {
                for (;; blacklist = blacklist->ai_next) {
                    if (blacklist->ai_next == nullptr) {
                        blacklist->ai_next = rec.blacklist;
                        rec.blacklist = nullptr;
                        break;
                    }
                }
            }
        }
    }

    if (has_flag(descriptor, FLAG::CONNECTING)
    && !has_flag(descriptor, FLAG::RECONNECT)
    && !has_flag(descriptor, FLAG::DISCONNECT)) {
        // Here we postpone closing this descriptor because we first want to
        // notify the user of this library of the connection that could not be
        // established.

        rem_flag(descriptor, FLAG::CONNECTING);
        set_flag(descriptor, FLAG::DISCONNECT);

        return ERROR::NONE;
    }

    if (!close_and_deinit(descriptor)) {
        pop(descriptor);
        return ERROR::FORBIDDEN_CONDITION;
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_epoll(
    int epoll_descriptor, int timeout
) noexcept {
    static constexpr const FLAG blockers[]{
        FLAG::NEW_CONNECTION,
        FLAG::DISCONNECT,
        FLAG::INCOMING
    };

    set_flag(epoll_descriptor, FLAG::EPOLL);

    for (FLAG flag_index : blockers) {
        if (!find_jack(flag_index)) {
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

    jack_type &jack = get_jack(epoll_descriptor);
    epoll_event *events = &(jack.events[1]);

    int pending = epoll_pwait(
        epoll_descriptor, events, EPOLL_MAX_EVENTS, timeout, &sigset_none
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

    for (int i=0; i<pending; ++i) {
        const int d = events[i].data.fd;

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
                            if (has_flag(d, FLAG::CONNECTING)) {
                                set_flag(d, FLAG::RECONNECT);
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

            rem_flag(d, FLAG::MAY_SHUTDOWN);
            terminate(d);

            continue;
        }

        if (is_listener(d)) {
            set_flag(d, FLAG::ACCEPT);
        }
        else {
            if (events[i].events & EPOLLIN) {
                set_flag(d, FLAG::READ);
            }

            if (events[i].events & EPOLLOUT) {
                set_flag(d, FLAG::WRITE);

                if (has_flag(d, FLAG::CONNECTING)) {
                    rem_flag(d, FLAG::CONNECTING);
                    set_flag(d, FLAG::NEW_CONNECTION);
                    set_flag(d, FLAG::MAY_SHUTDOWN);
                    modify_epoll(d, EPOLLIN|EPOLLET|EPOLLRDHUP);
                }
            }
        }
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_read(int descriptor) noexcept {
    // TODO: read directly to the pipework of the jack's incoming buffer in
    // respect to its individual hard limit of incoming bytes.

    jack_type &jack = get_jack(descriptor);
    PIPE &buffer = get_buffer(BUFFER::HANDLE_READ);

    for (size_t total_count = 0;;) {
        ssize_t count;
        char *buf = (char *) buffer.data;
        const size_t buf_sz = buffer.capacity;

        if (!buf_sz) {
            set_flag(descriptor, FLAG::READ);
            return ERROR::NONE;
        }
        else if ((count = ::read(descriptor, buf, buf_sz)) < 0) {
            if (count == -1) {
                int code = errno;

                if (code == EAGAIN || code == EWOULDBLOCK) {
                    if (total_count) {
                        rem_flag(descriptor, FLAG::READ);
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
            set_flag(descriptor, FLAG::READ);
            set_flag(descriptor, FLAG::INCOMING);
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

    rem_flag(descriptor, FLAG::MAY_SHUTDOWN);
    terminate(descriptor);

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::handle_write(int descriptor) noexcept {
    jack_type &jack = get_jack(descriptor);

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
        size_t buf = length - istart;
        size_t nblock = (buf < 4096 ? buf : 4096);

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
                        "write: %s (%s:%d)", strerror(code),
                        __FILE__, __LINE__
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
    else if (istart > 0) {
        PIPE &buffer = get_buffer(BUFFER::HANDLE_WRITE);

        const PIPE wrapper{
            make_pipe(to_uint8(outgoing) + istart, outgoing.size - istart)
        };

        {
            ERROR error{ copy(wrapper, buffer) };

            if (error != ERROR::NONE) {
                // TODO: figure out how to overcome errors here
                return error;
            }
        }

        {
            ERROR error{ copy(buffer, outgoing) };

            if (error != ERROR::NONE) {
                // TODO: figure out how to overcome errors here
                return error;
            }
        }

        if (try_again_later) {
            set_flag(descriptor, FLAG::WRITE);
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

SOCKETS::ERROR SOCKETS::handle_accept(int descriptor) noexcept {
    // New incoming connection detected.
    jack_type &epoll_jack = get_epoll_jack();
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

                    set_flag(descriptor, FLAG::ACCEPT);

                    return ERROR::NONE;
                }
                case EINTR: {
                    set_flag(descriptor, FLAG::ACCEPT);

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
            pop(descriptor);
        }

        return ERROR::FORBIDDEN_CONDITION;
    }

    ERROR error{push(make_jack(client_descriptor, descriptor, 0))};

    if (error != ERROR::NONE) {
        if (!close_and_deinit(client_descriptor)) {
            pop(client_descriptor);
        }

        return ERROR::FORBIDDEN_CONDITION;
    }

    jack_type &client_jack = get_jack(client_descriptor);

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

    epoll_event *event = &(epoll_jack.events[0]);

    event->data.fd = client_descriptor;
    event->events = EPOLLIN|EPOLLET|EPOLLRDHUP;

    retval = epoll_ctl(
        epoll_descriptor, EPOLL_CTL_ADD, client_descriptor, event
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
            pop(client_descriptor);
        }
    }
    else {
        set_flag(client_descriptor, FLAG::NEW_CONNECTION);
        set_flag(client_descriptor, FLAG::MAY_SHUTDOWN);
    }

    // We successfully accepted one client, but since there may be more of
    // them waiting we should recursively retry until we fail to accept any
    // new connections.

    return handle_accept(descriptor);
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
            pop(descriptor);
        }

        return NO_DESCRIPTOR;
    }

    jack_type &jack = get_jack(descriptor);

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
            pop(descriptor);
        }

        return NO_DESCRIPTOR;
    }

    if (has_flag(descriptor, FLAG::CONNECTING)) {
        modify_epoll(descriptor, EPOLLOUT|EPOLLET);
    }
    else {
        set_flag(descriptor, FLAG::MAY_SHUTDOWN);
        set_flag(descriptor, FLAG::NEW_CONNECTION);
    }

    return descriptor;
}

void SOCKETS::terminate(int descriptor, const char *file, int line) noexcept {
    if (descriptor == NO_DESCRIPTOR) return;

    if (has_flag(descriptor, FLAG::CLOSE)
    ||  has_flag(descriptor, FLAG::DISCONNECT)) {
        return;
    }

    if (has_flag(descriptor, FLAG::RECONNECT)
    ||  has_flag(descriptor, FLAG::CONNECTING)) {
        set_flag(descriptor, FLAG::CLOSE);
    }
    else {
        set_flag(descriptor, FLAG::DISCONNECT);
    }

    if (has_flag(descriptor, FLAG::MAY_SHUTDOWN)) {
        rem_flag(descriptor, FLAG::MAY_SHUTDOWN);

        if (has_flag(descriptor, FLAG::WRITE)
        && !has_flag(descriptor, FLAG::CONNECTING)) {
            // Let's handle writing here so that the descriptor would have a
            // chance to receive any pending bytes before being shut down.
            handle_write(descriptor);
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

    if (is_listener(descriptor)) {
        INDEX &index = get_index(INDEX::TYPE::DESCRIPTOR_JACK);

        for (size_t bucket=0; bucket < index.buckets; ++bucket) {
            // TODO: optimize this (use a special index for child-parent rels?)
            for (size_t i=0; i<index.table[bucket].value.size; ++i) {
                jack_type *rec{
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
            pop(descriptor);
        }

        return NO_DESCRIPTOR;
    }

    if (!bind_to_epoll(descriptor, epoll_descriptor)) {
        if (!close_and_deinit(descriptor)) {
            pop(descriptor);
        }

        return NO_DESCRIPTOR;
    }

    set_flag(descriptor, FLAG::ACCEPT);
    set_flag(descriptor, FLAG::LISTENER);

    jack_type *jack = find_jack(descriptor);

    if (jack) {
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);

        retval = getsockname(
            descriptor, (struct sockaddr *)&in_addr, &in_len
        );

        if (retval != 0) {
            if (retval == -1) {
                int code = errno;

                log(
                    "getsockname: %s (%s:%d)", strerror(code),
                    __FILE__, __LINE__
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
                jack->host, socklen_t(std::extent<decltype(jack->host)>::value),
                jack->port, socklen_t(std::extent<decltype(jack->port)>::value),
                NI_NUMERICHOST|NI_NUMERICSERV
            );

            if (retval != 0) {
                log(
                    "getnameinfo: %s (%s:%d)", gai_strerror(retval),
                    __FILE__, __LINE__
                );

                jack->host[0] = '\0';
                jack->port[0] = '\0';
            }
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

    ERROR error{push(make_jack(epoll_descriptor, NO_DESCRIPTOR, 0))};

    if (error != ERROR::NONE) {
        if (!close_and_deinit(epoll_descriptor)) {
            pop(epoll_descriptor);
        }

        return NO_DESCRIPTOR;
    }

    jack_type &jack = get_jack(epoll_descriptor);

    jack.events = new (std::nothrow) epoll_event [1+EPOLL_MAX_EVENTS];

    if (jack.events == nullptr) {
        out_of_memory();

        if (!close_and_deinit(epoll_descriptor)) {
            pop(epoll_descriptor);
        }

        return NO_DESCRIPTOR;
    }

    set_flag(epoll_descriptor, FLAG::EPOLL);

    return epoll_descriptor;
}

bool SOCKETS::bind_to_epoll(int descriptor, int epoll_descriptor) noexcept {
    if (descriptor == NO_DESCRIPTOR) return NO_DESCRIPTOR;

    jack_type &jack = get_jack(epoll_descriptor);
    epoll_event *event = &(jack.events[0]);

    event->data.fd = descriptor;
    event->events = EPOLLIN|EPOLLET|EPOLLRDHUP;

    int retval{
        epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, descriptor, event)
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

        jack_type *rec = nullptr;

        ERROR error{push(make_jack(descriptor, NO_DESCRIPTOR, 0))};

        if (error == ERROR::NONE) {
            rec = &get_jack(descriptor);
            rec->ai_family = ai_family;
            rec->ai_flags  = ai_flags;
        }

        if (rec && accept_incoming_connections) {
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
        else if (rec) {
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
                            set_flag(descriptor, FLAG::CONNECTING);

                            if (info == next) {
                                info = next->ai_next;
                            }
                            else {
                                prev->ai_next = next->ai_next;
                            }

                            next->ai_next = rec->blacklist;
                            rec->blacklist = next;
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

            pop(descriptor);
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

        jack_type jack{pop(descriptor)};
        bool found = jack.descriptor != NO_DESCRIPTOR;

        int close_children_of = NO_DESCRIPTOR;

        if (jack.parent == NO_DESCRIPTOR) {
            close_children_of = descriptor;
        }

        if (!found) {
            log(
                "descriptor %d closed but jack not found (%s:%d)",
                descriptor, __FILE__, __LINE__
            );
        }

        if (close_children_of != NO_DESCRIPTOR) {
            static constexpr const size_t descriptor_buffer_length = 1024;
            int descriptor_buffer[descriptor_buffer_length];
            size_t to_be_closed = 0;

            INDEX &index = get_index(INDEX::TYPE::DESCRIPTOR_JACK);

            Again:

            for (size_t bucket=0; bucket < index.buckets; ++bucket) {
                // TODO: optimize this (use a special index?)
                for (size_t i=0; i<index.table[bucket].value.size; ++i) {
                    jack_type *rec{
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
                        jack = pop(d);

                        if (jack.descriptor == NO_DESCRIPTOR) {
                            log(
                                "descriptor %d closed but jack not found "
                                "(%s:%d)", d, __FILE__, __LINE__
                            );
                        }

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

SOCKETS::ERROR SOCKETS::push(const jack_type jack) noexcept {
    if (jack.descriptor == NO_DESCRIPTOR) {
        die();
    }

    int descriptor = jack.descriptor;
    int group = jack.group;

    jack_type *jack_ptr = new_jack(&jack);

    if (!jack_ptr) {
        return ERROR::OUT_OF_MEMORY;
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::DESCRIPTOR_JACK,
            static_cast<uint64_t>(descriptor), make_pipe_entry(jack_ptr)
        )
    };

    if (!entry.valid) {
        deallocate(jack_ptr);

        return entry.error;
    }

    {
        // If the newly pushed jack has not its group set to zero at first, then
        // set_group would falsely reduce the group size.

        jack_ptr->group = 0;
    }

    return set_group(descriptor, group);
}

SOCKETS::jack_type SOCKETS::pop(int descriptor) noexcept {
    if (descriptor == NO_DESCRIPTOR) {
        return make_jack(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
    }

    jack_type *jack = find_jack(descriptor);

    if (jack) {
        int parent_descriptor = jack->parent;

        // First, let's free the flags.
        for (auto &flag : jack->flags) {
            rem_flag(
                jack->descriptor, static_cast<FLAG>(&flag - &(jack->flags[0]))
            );
        }

        // Then, we free the dynamically allocated memory.
        if (jack->events) {
            delete [] jack->events;
        }

        destroy(jack->incoming);
        destroy(jack->outgoing);

        if (jack->blacklist) {
            freeaddrinfo(jack->blacklist);
        }

        rem_group(descriptor);

        // Finally, we remove the jack.
        size_t erased{
            erase(
                INDEX::TYPE::DESCRIPTOR_JACK, static_cast<uint64_t>(descriptor)
            )
        };

        if (!erased) die();

        deallocate(jack); // TODO: recycle instead

        return make_jack(descriptor, parent_descriptor, 0);
    }

    return make_jack(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
}

const SOCKETS::jack_type *SOCKETS::find_jack(
    int descriptor
) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::DESCRIPTOR_JACK, static_cast<uint64_t>(descriptor))
    };

    if (!entry.valid) {
        return nullptr;
    }

    return to_jack(get_entry(*entry.val_pipe, entry.index));
}

SOCKETS::jack_type *SOCKETS::find_jack(int descriptor) noexcept {
    return const_cast<jack_type *>(
        static_cast<const SOCKETS &>(*this).find_jack(descriptor)
    );
}

const SOCKETS::jack_type *SOCKETS::find_jack(FLAG flag) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::FLAG_DESCRIPTOR, static_cast<uint64_t>(flag))
    };

    if (entry.valid) {
        const int descriptor = ((int *) entry.val_pipe->data)[entry.index];
        return &get_jack(descriptor); // If it's indexed, then it must exist.
    }

    return nullptr;
}

SOCKETS::jack_type *SOCKETS::find_jack(FLAG flag) noexcept {
    return const_cast<jack_type *>(
        static_cast<const SOCKETS &>(*this).find_jack(flag)
    );
}

const SOCKETS::jack_type *SOCKETS::find_epoll_jack() const noexcept {
    return find_jack(FLAG::EPOLL);
}

SOCKETS::jack_type *SOCKETS::find_epoll_jack() noexcept {
    return const_cast<jack_type *>(
        static_cast<const SOCKETS &>(*this).find_epoll_jack()
    );
}

const SOCKETS::jack_type &SOCKETS::get_jack(int descriptor) const noexcept {
    const jack_type *rec = find_jack(descriptor);

    if (!rec) die();

    return *rec;
}

SOCKETS::jack_type &SOCKETS::get_jack(int descriptor) noexcept {
    jack_type *rec = find_jack(descriptor);

    if (!rec) die();

    return *rec;
}

const SOCKETS::jack_type &SOCKETS::get_epoll_jack() const noexcept {
    const jack_type *rec = find_epoll_jack();

    if (!rec) die();

    return *rec;
}

SOCKETS::jack_type &SOCKETS::get_epoll_jack() noexcept {
    jack_type *rec = find_epoll_jack();

    if (!rec) die();

    return *rec;
}

const SOCKETS::PIPE *SOCKETS::find_descriptors(FLAG flag) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::FLAG_DESCRIPTOR, static_cast<uint64_t>(flag))
    };

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

SOCKETS::MEMORY SOCKETS::to_memory(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::MEMORY) die();

    return entry.as_memory;
}

SOCKETS::jack_type *SOCKETS::to_jack(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::JACK_PTR) die();

    return static_cast<jack_type *>(entry.as_ptr);
}

int SOCKETS::to_int(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::INT) die();

    return entry.as_int;
}

uint8_t *SOCKETS::to_uint8(PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::UINT8) die();

    return static_cast<uint8_t *>(pipe.data);
}

bool SOCKETS::modify_epoll(int descriptor, uint32_t events) noexcept {
    jack_type &epoll_jack = get_epoll_jack();
    int epoll_descriptor = epoll_jack.descriptor;
    epoll_event *event = &(epoll_jack.events[0]);

    event->data.fd = descriptor;
    event->events = events;

    int retval = epoll_ctl(
        epoll_descriptor, EPOLL_CTL_MOD, descriptor, event
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
    jack_type &rec = get_jack(descriptor);

    INDEX::ENTRY found{find(INDEX::TYPE::GROUP_SIZE, rec.group)};

    if (found.valid) {
        if (found.val_pipe->type == PIPE::TYPE::UINT64) {
            uint64_t &value = ((uint64_t *) found.val_pipe->data)[found.index];

            if (--value == 0) {
                erase(INDEX::TYPE::GROUP_SIZE, rec.group);
            }
        }
        else die();
    }

    rec.group = group;

    if (group == 0) {
        // 0 stands for no group. We don't keep track of its size.
        return ERROR::NONE;
    }

    found = find(INDEX::TYPE::GROUP_SIZE, group);

    if (found.valid) {
        if (found.val_pipe->type == PIPE::TYPE::UINT64) {
            ++((uint64_t *) found.val_pipe->data)[found.index];
        }
        else die();
    }
    else {
        return insert(INDEX::TYPE::GROUP_SIZE, group, make_pipe_entry(1)).error;
    }

    return ERROR::NONE;
}

void SOCKETS::rem_group(int descriptor) noexcept {
    if (set_group(descriptor, 0) != ERROR::NONE) {
        die(); // Removing descriptor from its group should never fail.
    }
}

SOCKETS::ERROR SOCKETS::set_flag(
    int descriptor, FLAG flag, bool value
) noexcept {
    if (value == false) {
        rem_flag(descriptor, flag);
        return ERROR::NONE;
    }

    jack_type &rec = get_jack(descriptor);

    size_t index = static_cast<size_t>(flag);

    if (index >= std::extent<decltype(rec.flags)>::value) {
        return die();
    }

    uint32_t pos = rec.flags[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        return ERROR::NONE; // Already set.
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::FLAG_DESCRIPTOR, static_cast<uint64_t>(flag),
            make_pipe_entry(descriptor)
        )
    };

    if (entry.valid) {
        if (entry.index >= std::numeric_limits<uint32_t>::max()) {
            die(); // the number of descriptors is limited by the max of int.
        }

        rec.flags[index] = uint32_t(entry.index);
        return ERROR::NONE;
    }

    return entry.error;
}

void SOCKETS::rem_flag(int descriptor, FLAG flag) noexcept {
    size_t index = static_cast<size_t>(flag);

    jack_type &rec = get_jack(descriptor);

    if (index >= std::extent<decltype(rec.flags)>::value) {
        die();
    }

    uint32_t pos = rec.flags[index];

    if (pos == std::numeric_limits<uint32_t>::max()) {
        return;
    }

    size_t erased = erase(
        INDEX::TYPE::FLAG_DESCRIPTOR,
        static_cast<uint64_t>(flag), make_pipe_entry(descriptor), pos, 1
    );

    if (!erased) {
        die();
        return;
    }

    INDEX::ENTRY entry{
        find(
            INDEX::TYPE::FLAG_DESCRIPTOR,
            static_cast<uint64_t>(flag), {}, pos, 1
        )
    };

    if (entry.valid && entry.index == pos) {
        int other_descriptor = ((int *) entry.val_pipe->data)[entry.index];
        get_jack(other_descriptor).flags[index] = pos;
    }

    rec.flags[index] = std::numeric_limits<uint32_t>::max();
}

bool SOCKETS::has_flag(const jack_type &rec, FLAG flag) const noexcept {
    size_t index = static_cast<size_t>(flag);

    if (index >= std::extent<decltype(rec.flags)>::value) {
        return false;
    }

    uint32_t pos = rec.flags[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        return true;
    }

    return false;
}

bool SOCKETS::has_flag(int descriptor, FLAG flag) const noexcept {
    const jack_type *rec = find_jack(descriptor);
    return rec ? has_flag(*rec, flag) : false;
}

SOCKETS::INDEX::ENTRY SOCKETS::find(
    INDEX::TYPE index_type, uint64_t key, PIPE::ENTRY value,
    size_t start_i, size_t iterations
) const noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    const INDEX::TABLE &table = index.table[key % index.buckets];
    const PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::UINT64) {
        die(); // The only valid key type is uint64_t.
    }

    const uint64_t *data = (const uint64_t *) key_pipe.data;

    if (!data) {
        return {};
    }

    const PIPE &value_pipe = table.value;

    if (value.type != PIPE::TYPE::NONE && value.type != value_pipe.type) {
        die();
    }

    size_t sz = key_pipe.size;
    size_t i = std::min(sz-1, start_i);

    for (; i<sz && iterations; --i, --iterations) {
        if (data[i] != key) {
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
                case PIPE::TYPE::JACK_PTR:
                case PIPE::TYPE::PTR: {
                    const void **vd = (const void **) value_pipe.data;

                    if (vd[i] != value.as_ptr) {
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::MEMORY: {
                    const MEMORY *vd = (const MEMORY *) value_pipe.data;

                    if (std::memcmp(vd+i, &value.as_memory, sizeof(MEMORY))) {
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
    INDEX::TYPE index_type, uint64_t key, PIPE::ENTRY value
) noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::UINT64) die();

    PIPE &val_pipe = table.value;

    if (!index.multimap) {
        uint64_t *key_data = (uint64_t *) key_pipe.data;

        for (size_t i=0; i<key_pipe.size; ++i) {
            if (key_data[i] != key) {
                continue;
            }

            return make_index_entry(
                key_pipe, val_pipe, i, insert(val_pipe, i, value)
            );
        }
    }

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
    INDEX::TYPE index_type, uint64_t key, PIPE::ENTRY value,
    size_t start_i, size_t iterations
) noexcept {
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::UINT64) die();

    uint64_t *key_data = (uint64_t *) key_pipe.data;

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
        if (key_data[i] != key) {
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
                case PIPE::TYPE::JACK_PTR:
                case PIPE::TYPE::PTR: {
                    const void **vd = (const void **) val_pipe.data;

                    if (vd[i] != value.as_ptr) {
                        i = index.multimap ? i-1 : key_pipe.size;
                        continue;
                    }

                    break;
                }
                case PIPE::TYPE::MEMORY: {
                    const MEMORY *vd = (const MEMORY *) val_pipe.data;

                    if (std::memcmp(vd+i, &value.as_memory, sizeof(MEMORY))) {
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

size_t SOCKETS::count(INDEX::TYPE index_type, uint64_t key) const noexcept {
    size_t count = 0;
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets > 0) {
        const INDEX::TABLE &table = index.table[key % index.buckets];
        const PIPE &pipe = table.key;

        if (pipe.type == PIPE::TYPE::UINT64) {
            const uint64_t *data = (const uint64_t *) pipe.data;

            if (data) {
                for (size_t i=0, sz=pipe.size; i<sz; ++i) {
                    if (data[i] == key) {
                        ++count;

                        if (!index.multimap) return count;
                    }
                }
            }
        }
    }
    else die();

    return count;
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

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            ((uint8_t *) pipe.data)[index] = value.as_uint8;
            break;
        }
        case PIPE::TYPE::UINT64: {
            ((uint64_t *) pipe.data)[index] = value.as_uint64;
            break;
        }
        case PIPE::TYPE::INT: {
            ((int *) pipe.data)[index] = value.as_int;
            break;
        }
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            ((void **) pipe.data)[index] = value.as_ptr;
            break;
        }
        case PIPE::TYPE::MEMORY: {
            ((MEMORY *) pipe.data)[index] = value.as_memory;
            break;
        }
        case PIPE::TYPE::NONE: return die();
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::insert(PIPE &pipe, PIPE::ENTRY value) noexcept {
    return insert(pipe, pipe.size, value);
}

SOCKETS::ERROR SOCKETS::reserve(PIPE &pipe, size_t capacity) noexcept {
    if (pipe.capacity >= capacity) {
        return ERROR::NONE;
    }

    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            uint8_t *new_data = new (std::nothrow) uint8_t [capacity];

            if (!new_data) {
                return ERROR::OUT_OF_MEMORY;
            }

            uint8_t *old_data = (uint8_t *) pipe.data;

            if (old_data) {
                std::memcpy(new_data, old_data, pipe.size * sizeof(uint8_t));
                delete [] old_data;
            }

            pipe.data = new_data;
            pipe.capacity = capacity;

            break;
        }
        case PIPE::TYPE::UINT64: {
            uint64_t *new_data = new (std::nothrow) uint64_t [capacity];

            if (!new_data) {
                return ERROR::OUT_OF_MEMORY;
            }

            uint64_t *old_data = (uint64_t *) pipe.data;

            if (old_data) {
                std::memcpy(new_data, old_data, pipe.size * sizeof(uint64_t));
                delete [] old_data;
            }

            pipe.data = new_data;
            pipe.capacity = capacity;

            break;
        }
        case PIPE::TYPE::INT: {
            int *new_data = new (std::nothrow) int [capacity];

            if (!new_data) {
                return ERROR::OUT_OF_MEMORY;
            }

            int *old_data = (int *) pipe.data;

            if (old_data) {
                std::memcpy(new_data, old_data, pipe.size * sizeof(int));
                delete [] old_data;
            }

            pipe.data = new_data;
            pipe.capacity = capacity;

            break;
        }
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            void **new_data = new (std::nothrow) void* [capacity];

            if (!new_data) {
                return ERROR::OUT_OF_MEMORY;
            }

            void **old_data = (void **) pipe.data;

            if (old_data) {
                std::memcpy(new_data, old_data, pipe.size * sizeof(void*));
                delete [] old_data;
            }

            pipe.data = new_data;
            pipe.capacity = capacity;

            break;
        }
        case PIPE::TYPE::MEMORY: {
            MEMORY *new_data = new (std::nothrow) MEMORY [capacity];

            if (!new_data) {
                return ERROR::OUT_OF_MEMORY;
            }

            MEMORY *old_data = (MEMORY *) pipe.data;

            if (old_data) {
                std::memcpy(new_data, old_data, pipe.size * sizeof(MEMORY));
                delete [] old_data;
            }

            pipe.data = new_data;
            pipe.capacity = capacity;

            break;
        }
        case PIPE::TYPE::NONE: return die();
    }

    return ERROR::NONE;
}


SOCKETS::ERROR SOCKETS::swap(PIPE &first, PIPE &second) noexcept {
    if (first.type != second.type) {
        return die();
    }

    std::swap(first.capacity, second.capacity);
    std::swap(first.size,     second.size);
    std::swap(first.data,     second.data);

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
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            std::memcpy(
                static_cast<void **>(dst.data) + old_size, src.data,
                count * sizeof(void*)
            );
            break;
        }
        case PIPE::TYPE::MEMORY: {
            std::memcpy(
                static_cast<MEMORY *>(dst.data) + old_size, src.data,
                count * sizeof(MEMORY)
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
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            void **data = (void **) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::MEMORY: {
            MEMORY *data = (MEMORY *) pipe.data;
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
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            entry.as_ptr = static_cast<void **>(data)[index];
            break;
        }
        case PIPE::TYPE::MEMORY: {
            entry.as_memory = static_cast<MEMORY *>(data)[index];
            break;
        }
        case PIPE::TYPE::NONE: die(); break;
    }

    return entry;
}

void SOCKETS::destroy(PIPE &pipe) noexcept {
    switch (pipe.type) {
        case PIPE::TYPE::UINT8: {
            if (pipe.data) delete [] ((uint8_t *) pipe.data);

            break;
        }
        case PIPE::TYPE::UINT64: {
            if (pipe.data) delete [] ((uint64_t *) pipe.data);

            break;
        }
        case PIPE::TYPE::INT: {
            if (pipe.data) delete [] ((int *) pipe.data);

            break;
        }
        case PIPE::TYPE::JACK_PTR:
        case PIPE::TYPE::PTR: {
            if (pipe.data) delete [] ((void **) pipe.data);

            break;
        }
        case PIPE::TYPE::MEMORY: {
            if (pipe.data) delete [] ((MEMORY *) pipe.data);

            break;
        }
        case PIPE::TYPE::NONE: die(); break;
    }

    pipe.data = nullptr;
    pipe.capacity = 0;
    pipe.size = 0;
}

const SOCKETS::MEMORY *SOCKETS::allocate(
    size_t bytes, const void *copy
) noexcept {
    MEMORY memory{
        make_memory(new (std::nothrow) uint8_t[bytes](), bytes)
    };

    if (memory.data == nullptr) {
        return nullptr;
    }

    size_t index = mempool.size;

    if (insert(mempool, make_pipe_entry(memory)) != ERROR::NONE) {
        delete [] memory.data;

        return nullptr;
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::MEM_ADDR_INDEX,
            reinterpret_cast<uintptr_t>(memory.data), make_pipe_entry(index)
        )
    };

    if (!entry.valid) {
        delete [] memory.data;

        return nullptr;
    }

    if (copy) {
        std::memcpy(memory.data, copy, memory.size);
    }

    log("Allocated %lu byte%s.", memory.size, memory.size == 1 ? "" : "s");

    return ((MEMORY *) mempool.data) + index;
}

void SOCKETS::deallocate(const void *resource) noexcept {
    size_t mempool_index = 0;

    {
        INDEX::ENTRY entry{
            find(
                INDEX::TYPE::MEM_ADDR_INDEX,
                reinterpret_cast<uintptr_t>(resource)
            )
        };

        if (!entry.valid) {
            die();
            return;
        }

        mempool_index = ((uint64_t *) entry.val_pipe->data)[entry.index];
    }

    MEMORY memory = ((MEMORY *) mempool.data)[mempool_index];

    delete [] memory.data;

    erase(mempool, mempool_index);

    log("Deallocated %lu byte%s.", memory.size, memory.size == 1 ? "" : "s");

    if (mempool.size > mempool_index) {
        memory = ((MEMORY *) mempool.data)[mempool_index];

        INDEX::ENTRY entry{
            find(
                INDEX::TYPE::MEM_ADDR_INDEX,
                reinterpret_cast<uintptr_t>(memory.data)
            )
        };

        if (!entry.valid) {
            die();
            return;
        }

        // TODO: call insert or implement "replace" method instead of this hack:
        ((uint64_t *) entry.val_pipe->data)[entry.index] = mempool_index;
    }
}

SOCKETS::jack_type *SOCKETS::new_jack(const jack_type *copy) noexcept {
    const MEMORY *mem = allocate(sizeof(jack_type), copy);

    return mem ? (jack_type *) mem->data : nullptr;
}

void SOCKETS::dump(const char *file, int line) const noexcept {
    for (size_t i=0; i<size_t(FLAG::MAX_FLAGS); ++i) {
        const PIPE *found = find_descriptors(static_cast<FLAG>(i));

        if (found && found->size) {
            printf("flag[%lu] of %s, line %d:\n", i, file, line);
            for (size_t j=0; j<found->size; ++j) {
                printf("%d ", ((int *) found->data)[j]);
            }
            printf("\n");
        }
    }
}

constexpr SOCKETS::jack_type SOCKETS::make_jack(
    int descriptor, int parent, int group
) noexcept {
#if __cplusplus <= 201703L
    __extension__
#endif
    jack_type jack{
        .flags      = {},
        .events     = nullptr,
        .incoming   = { make_pipe(PIPE::TYPE::UINT8) },
        .outgoing   = { make_pipe(PIPE::TYPE::UINT8) },
        .host       = {'\0'},
        .port       = {'\0'},
        .descriptor = descriptor,
        .parent     = parent,
        .group      = group,
        .ai_family  = 0,
        .ai_flags   = 0,
        .blacklist  = nullptr,
        .bitset     = {}
    };

    for (auto &flag : jack.flags) {
        flag = std::numeric_limits<uint32_t>::max();
    }

    return jack;
}

constexpr SOCKETS::EVENT SOCKETS::make_event(
    int descriptor, EVENT::TYPE type, bool valid
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    EVENT{
        .descriptor = descriptor,
        .type = type,
        .valid = valid
    };
}

constexpr struct SOCKETS::INDEX::ENTRY SOCKETS::make_index_entry(
    const PIPE &keys, const PIPE &values, size_t index, ERROR error, bool valid
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
    const PIPE &keys, const PIPE &values, size_t index, ERROR error
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
        .type = PIPE::TYPE::UINT8
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
        .type = type
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
    MEMORY value
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::PIPE::ENTRY{
        .as_memory = value,
        .type = PIPE::TYPE::MEMORY
    };
}

constexpr struct SOCKETS::PIPE::ENTRY SOCKETS::make_pipe_entry(
    jack_type *value
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

constexpr struct SOCKETS::MEMORY SOCKETS::make_memory(
    uint8_t *data, size_t size
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::MEMORY{
        .size = size,
        .data = data
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

SOCKETS::FLAG SOCKETS::next(FLAG flag) noexcept {
    return static_cast<FLAG>(
        (static_cast<size_t>(flag) + 1) % static_cast<size_t>(FLAG::MAX_FLAGS)
    );
}

#endif
