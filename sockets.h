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
#include <unistd.h>

class SOCKETS final {
    public:
    static constexpr const char *const VERSION = "1.0";

    enum class ERROR : uint8_t {
        NONE = 0,
        OUT_OF_MEMORY,
        UNDEFINED_BEHAVIOR,
        FORBIDDEN_CONDITION,
        UNHANDLED_EVENTS,
        UNSPECIFIED
    };

    static constexpr const ERROR
        ERR_NONE                = ERROR::NONE,
        ERR_OUT_OF_MEMORY       = ERROR::OUT_OF_MEMORY,
        ERR_UNDEFINED_BEHAVIOR  = ERROR::UNDEFINED_BEHAVIOR,
        ERR_FORBIDDEN_CONDITION = ERROR::FORBIDDEN_CONDITION,
        ERR_UNHANDLED_EVENTS    = ERROR::UNHANDLED_EVENTS,
        ERR_UNSPECIFIED         = ERROR::UNSPECIFIED;

    inline static const char *get_code(ERROR) noexcept;

    struct SESSION {
        size_t id;
        ERROR error;
        bool valid:1;
    };

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
        size_t session;
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
    void set_memcap(size_t bytes) noexcept;
    void set_intake(
        size_t session_id, size_t bytes,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    SESSION listen(
        const char *port, int family =AF_UNSPEC,
        const std::initializer_list<int> options ={SO_REUSEADDR}, int flags =0
    ) noexcept;
    SESSION connect(const char *host, const char *port) noexcept;
    bool idle() const noexcept;

    ERROR next_error(int timeout =-1) noexcept;
    ERROR last_error() noexcept;
    ALERT next_alert() noexcept;

    size_t incoming(size_t session_id) const noexcept;
    size_t outgoing(size_t session_id) const noexcept;
    size_t read(size_t session_id, void *buf, size_t count) noexcept;
    const char *read(size_t session_id) noexcept;
    ERROR write(size_t session_id, const void *buf, size_t count) noexcept;
    ERROR write(size_t session_id, const char *text) noexcept;
    ERROR writef(
        size_t session_id, const char *fmt, ...
    ) noexcept __attribute__((format(printf, 3, 4)));

    [[nodiscard]] bool is_listener(size_t session_id) const noexcept;
    [[nodiscard]] bool is_frozen(size_t session_id) const noexcept;
    [[nodiscard]] SESSION get_listener(size_t session_id) const noexcept;
    [[nodiscard]] const char *get_host(size_t session_id) const noexcept;
    [[nodiscard]] const char *get_port(size_t session_id) const noexcept;
    [[nodiscard]] SESSION get_session(int descriptor) const noexcept;
    [[nodiscard]] const int *get_descriptor(size_t session_id) const noexcept;

    void freeze(size_t session_id) noexcept;
    void unfreeze(size_t session_id) noexcept;
    void disconnect(size_t session_id) noexcept;

    private:
    static constexpr const size_t BITS_PER_BYTE{
        std::numeric_limits<unsigned char>::digits
    };

    enum class BUFFER : uint8_t {
        GENERIC_INT,
        GENERIC_BYTE,
        NEXT_ERROR,
        READ,
        WRITEF,
        // Do not change the order of items below this line.
        MAX_BUFFERS
    };

    struct MEMORY {
        size_t     size;
        uint8_t   *data;
        MEMORY    *next;
        MEMORY    *prev;
        bool  indexed:1;
        bool recycled:1;
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
            EVENT_DESCRIPTOR,
            DESCRIPTOR_JACK,
            SESSION_JACK,
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
        size_t entries;
        struct TABLE {
            PIPE key;
            PIPE value;
        } *table;
        TYPE type;
        bool multimap:1;
        bool autogrow:1;
    };

    struct JACK {
        size_t id;
        size_t intake;
        unsigned event_lookup[ static_cast<size_t>(EVENT::MAX_EVENTS) ];
        PIPE epoll_ev;
        PIPE children;
        PIPE incoming;
        PIPE outgoing;
        PIPE host;
        PIPE port;
        int descriptor;
        struct PARENT {
            int descriptor;
            int child_index;
        } parent;
        int ai_family;
        int ai_flags;
        struct addrinfo *blacklist;
        struct BITSET {
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
        int descriptor, int parent =-1
    ) noexcept;

    inline static constexpr struct ALERT make_alert(
        size_t session, EVENT type, bool valid =true
    ) noexcept;

    inline static constexpr struct SESSION make_session(ERROR) noexcept;
    inline static constexpr struct SESSION make_session(
        size_t id, ERROR error =ERROR::NONE, bool valid =true
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

    inline static constexpr bool is_descriptor(int) noexcept;
    inline static constexpr EVENT next(EVENT) noexcept;
    inline static constexpr size_t size(PIPE::TYPE) noexcept;
    inline static constexpr auto fmt_bytes(size_t) noexcept;
    inline static int clz(unsigned int) noexcept;
    inline static int clz(unsigned long) noexcept;
    inline static int clz(unsigned long long) noexcept;
    inline static unsigned int       next_pow2(unsigned int) noexcept;
    inline static unsigned long      next_pow2(unsigned long) noexcept;
    inline static unsigned long long next_pow2(unsigned long long) noexcept;

    ERROR handle_close (JACK &) noexcept;
    ERROR handle_epoll (JACK &, int timeout) noexcept;
    ERROR handle_read  (JACK &) noexcept;
    ERROR handle_write (JACK &) noexcept;
    ERROR handle_accept(JACK &) noexcept;

    SESSION connect(
        const char *host, const char *port, int family, int flags,
        const struct addrinfo *blacklist =nullptr,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    SESSION listen(
        const char *host, const char *port, int family, int flags,
        const std::initializer_list<int> options,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    void terminate(
        int descriptor,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    size_t next_connection() noexcept;
    size_t next_disconnection() noexcept;
    size_t next_incoming() noexcept;

    SESSION create_epoll() noexcept;
    bool bind_to_epoll(int descriptor, int epoll_descriptor) noexcept;
    bool modify_epoll(int descriptor, uint32_t events) noexcept;

    SESSION open_and_capture(
        const char *host, const char *port, int family, int flags,
        const std::initializer_list<int> options ={},
        const struct addrinfo *blacklist =nullptr,
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) noexcept;

    void close_and_release(JACK&) noexcept;
    void close_descriptor(int) noexcept;

    [[nodiscard]] SESSION capture(const JACK &copy) noexcept;
    void release(JACK *) noexcept;

    JACK *find_jack(int descriptor) const noexcept;
    JACK *find_jack(SESSION) const noexcept;
    JACK *find_jack(EVENT) const noexcept;
    JACK *find_epoll_jack() const noexcept;
    JACK &get_jack(int descriptor) const noexcept;
    JACK &get_jack(SESSION) const noexcept;
    JACK &get_epoll_jack() const noexcept;
    const PIPE *find_descriptors(EVENT) const noexcept;

    /*TODO: [[nodiscard]]*/ ERROR set_event(
        JACK &, EVENT, bool val =true
    ) noexcept;
    void rem_event(JACK &, EVENT) noexcept;
    bool has_event(const JACK &, EVENT) const noexcept;
    void rem_child(JACK &, JACK &child) const noexcept;
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
    [[nodiscard]] ERROR reindex() noexcept;
    void erase(PIPE &pipe, size_t index) const noexcept;
    void destroy(PIPE &pipe) noexcept;
    void set_value(INDEX::ENTRY, PIPE::ENTRY) noexcept;
    PIPE::ENTRY get_value(INDEX::ENTRY) const noexcept;
    PIPE::ENTRY get_entry(const PIPE &pipe, size_t index) const noexcept;
    PIPE::ENTRY get_last(const PIPE &pipe) const noexcept;
    PIPE::ENTRY pop_back(PIPE &pipe) const noexcept;
    [[nodiscard]] ERROR reserve(PIPE&, size_t capacity) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, size_t index, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR copy(const PIPE &src, PIPE &dst) noexcept;
    [[nodiscard]] ERROR append(const PIPE &src, PIPE &dst) noexcept;
    void replace(PIPE&, size_t index, PIPE::ENTRY) const noexcept;
    ERROR swap(PIPE &, PIPE &) noexcept;
    PIPE &get_buffer(BUFFER) noexcept;
    INDEX &get_index(INDEX::TYPE) noexcept;

    KEY       to_key   (PIPE::ENTRY) const noexcept;
    JACK     *to_jack  (PIPE::ENTRY) const noexcept;
    MEMORY   *to_memory(PIPE::ENTRY) const noexcept;
    int       to_int   (PIPE::ENTRY) const noexcept;
    uint64_t  to_uint64(PIPE::ENTRY) const noexcept;

    int         *to_int        (const PIPE &) const noexcept;
    char        *to_char       (const PIPE &) const noexcept;
    uint8_t     *to_uint8      (const PIPE &) const noexcept;
    uint64_t    *to_uint64     (const PIPE &) const noexcept;
    KEY         *to_key        (const PIPE &) const noexcept;
    void       **to_ptr        (const PIPE &) const noexcept;
    epoll_event *to_epoll_event(const PIPE &) const noexcept;

    void *to_ptr(PIPE::ENTRY &) const noexcept;
    void *to_ptr(PIPE &, size_t index) const noexcept;
    const void *to_ptr(const PIPE &, size_t index) const noexcept;

    void enlist(MEMORY &, MEMORY *&list) noexcept;
    void unlist(MEMORY &, MEMORY *&list) noexcept;
    const MEMORY *find_memory(const void *) const noexcept;
    MEMORY *find_memory(const void *) noexcept;
    const MEMORY &get_memory(const void *) const noexcept;
    MEMORY &get_memory(const void *) noexcept;
    INDEX::TABLE *allocate_tables(size_t count) noexcept;
    void destroy_and_delete(INDEX::TABLE *tables, size_t count) noexcept;
    MEMORY *allocate_and_index(
        size_t byte_count, const void *copy =nullptr
    ) noexcept;
    MEMORY *allocate(size_t byte_count) noexcept;
    void deallocate(MEMORY &) noexcept;
    void recycle(MEMORY &) noexcept;
    JACK *new_jack(const JACK *copy =nullptr) noexcept;

    void log(
        ERROR, const char *file =__builtin_FILE(), int line =__builtin_LINE(),
        char const *function = __builtin_FUNCTION()
    ) const noexcept;
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

    ERROR err(ERROR) noexcept;

    void (*log_callback)(const char *text) noexcept;
    INDEX indices[static_cast<size_t>(INDEX::TYPE::MAX_TYPES)];
    PIPE  buffers[static_cast<size_t>(BUFFER::MAX_BUFFERS)];

    struct MEMPOOL {
        MEMORY *free[sizeof(size_t) * BITS_PER_BYTE];
        MEMORY *list;
        size_t usage;
        size_t top;
        size_t cap;
    } mempool;

    inline static constexpr struct MEMPOOL make_mempool() noexcept;

    EVENT handled;
    ERROR errored;

    size_t last_jack_id;

    struct BITSET {
        bool unhandled_events:1;
        bool timeout:1;
        bool reindex:1;
    } bitset;

    sigset_t sigset_all;
    sigset_t sigset_none;
    sigset_t sigset_orig;
};

bool operator!(SOCKETS::ERROR error) noexcept {
    return error == static_cast<SOCKETS::ERROR>(0);
}

SOCKETS::SOCKETS() noexcept :
    log_callback(nullptr), indices{}, buffers{}, mempool{make_mempool()},
    handled{}, errored{}, last_jack_id{}, bitset{} {
}

SOCKETS::~SOCKETS() {
    if (mempool.usage > sizeof(SOCKETS)) {
        log(
            "memory usage remains at %lu byte%s (leak?)",
            mempool.usage, mempool.usage == 1 ? "" : "s"
        );
    }

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
            destroy_and_delete(index.table, index.buckets);
            index.table = nullptr;
        }

        index.buckets = 0;
        index.entries = 0;
        index.type = INDEX::TYPE::NONE;
    }

    if (mempool.list) {
        bug(); // We should have already explicitly deallocated all memory.

        while (mempool.list) {
            deallocate(*mempool.list);
        }
    }

    for (MEMORY *&free : mempool.free) {
        while (free) {
            deallocate(*free);
        }
    }

    mempool.usage = sizeof(SOCKETS);
    mempool.top = mempool.usage;

    last_jack_id = 0;

    bitset = {};
}

bool SOCKETS::init() noexcept {
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

    clear();

    for (INDEX &index : indices) {
        index.type = static_cast<INDEX::TYPE>(&index - &indices[0]);

        switch (index.type) {
            default: {
                index.buckets = 1;
                index.multimap = false;
                index.autogrow = true;
                break;
            }
            case INDEX::TYPE::EVENT_DESCRIPTOR: {
                index.buckets = static_cast<size_t>(EVENT::MAX_EVENTS);
                index.multimap = true;
                index.autogrow = false;
                break;
            }
        }

        switch (index.type) {
            case INDEX::TYPE::NONE: continue;
            case INDEX::TYPE::EVENT_DESCRIPTOR:
            case INDEX::TYPE::DESCRIPTOR_JACK:
            case INDEX::TYPE::SESSION_JACK:
            case INDEX::TYPE::RESOURCE_MEMORY: {
                index.table = allocate_tables(index.buckets);
                break;
            }
            default: die();
        }

        if (index.table == nullptr) {
            log("%s: out of memory (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
            clear();
            return false;
        }

        for (size_t j=0; j<index.buckets; ++j) {
            INDEX::TABLE &table = index.table[j];
            PIPE &key_pipe = table.key;
            PIPE &val_pipe = table.value;

            key_pipe.type = PIPE::TYPE::KEY;

            switch (index.type) {
                case INDEX::TYPE::SESSION_JACK:
                case INDEX::TYPE::DESCRIPTOR_JACK: {
                    val_pipe.type = PIPE::TYPE::JACK_PTR;
                    break;
                }
                case INDEX::TYPE::RESOURCE_MEMORY: {
                    val_pipe.type = PIPE::TYPE::MEMORY_PTR;
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
                bug();
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
            case BUFFER::READ: {
                static constexpr const size_t buffer_length{ 1024 };

                pipe.type = PIPE::TYPE::UINT8;

                ERROR error{ reserve(pipe, buffer_length) };

                if (error != ERROR::NONE) {
                    log(error);
                    clear();

                    return false;
                }

                break;
            }
            case BUFFER::MAX_BUFFERS: die();
        }
    }

    SESSION epoll_session{ create_epoll() };

    if (!epoll_session.valid) {
        log(
            "%s: %s, %s (%s:%d)", __FUNCTION__,
            "epoll jack could not be created", get_code(epoll_session.error),
            __FILE__, __LINE__
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
            JACK *const jack{
                to_jack(
                    get_last(descriptor_jack.table[bucket].value)
                )
            };

            close_and_release(*jack);
        }
    }

    clear();

    return success;
}

void SOCKETS::set_logger(void (*callback)(const char *) noexcept) noexcept {
    log_callback = callback;
}

void SOCKETS::set_memcap(size_t bytes) noexcept {
    mempool.cap = bytes;
}

void SOCKETS::set_intake(
    size_t sid, size_t bytes, const char *file, int line
) noexcept {
    JACK *jack = find_jack(make_session(sid));

    if (jack) {
        jack->intake = bytes;
    }
    else log("session %lu not found (%s:%d)", sid, file, line);
}

SOCKETS::SESSION SOCKETS::listen(
    const char *port, int family, const std::initializer_list<int> options,
    int flags
) noexcept {
    return listen(nullptr, port, family, AI_PASSIVE|flags, options);
}

struct SOCKETS::ALERT SOCKETS::next_alert() noexcept {
    size_t session;

    bitset.unhandled_events = false;

    while (( session = next_connection() ) != 0) {
        return make_alert(session, EV_CONNECTION);
    }

    while (( session = next_incoming() ) != 0) {
        return make_alert(session, EV_INCOMING);
    }

    while (( session = next_disconnection() ) != 0) {
        return make_alert(session, EV_DISCONNECTION);
    }

    return make_alert(0, EV_NONE, false);
}

size_t SOCKETS::next_connection() noexcept {
    JACK *const jack = find_jack(EVENT::CONNECTION);

    if (jack) {
        rem_event(*jack, EVENT::CONNECTION);

        return jack->id;
    }

    return 0;
}

size_t SOCKETS::next_disconnection() noexcept {
    if (find_jack(EVENT::CONNECTION)) {
        // We postpone reporting any disconnections until the application
        // has acknowledged all the new incoming connections. This prevents
        // us from reporting a disconnection event before its respective
        // connection event is reported.

        return 0;
    }

    JACK *const jack = find_jack(EVENT::DISCONNECTION);

    if (jack) {
        if (set_event(*jack, EVENT::CLOSE) == ERROR::NONE) {
            rem_event(*jack, EVENT::DISCONNECTION);

            return jack->id;
        }
    }

    return 0;
}

size_t SOCKETS::next_incoming() noexcept {
    JACK *const jack = find_jack(EVENT::INCOMING);

    if (jack) {
        rem_event(*jack, EVENT::INCOMING);

        return jack->id;
    }

    return 0;
}

bool SOCKETS::is_listener(const JACK &jack) const noexcept {
    return jack.bitset.listener;
}

bool SOCKETS::is_listener(size_t sid) const noexcept {
    const JACK *const jack = find_jack(make_session(sid));
    return jack ? is_listener(*jack) : false;
}

SOCKETS::SESSION SOCKETS::get_listener(size_t sid) const noexcept {
    const JACK *const jack = find_jack(make_session(sid));

    if (!jack) {
        return make_session(ERROR::NONE);
    }

    const JACK *const parent = find_jack(jack->parent.descriptor);

    return parent ? make_session(parent->id) : make_session(ERROR::NONE);
}

const char *SOCKETS::get_host(size_t session_id) const noexcept {
    const JACK *const jack = find_jack(make_session(session_id));
    return jack && jack->host.size ? to_char(jack->host) : "";
}

const char *SOCKETS::get_port(size_t session_id) const noexcept {
    const JACK *const jack = find_jack(make_session(session_id));
    return jack && jack->port.size ? to_char(jack->port) : "";
}

SOCKETS::SESSION SOCKETS::get_session(int descriptor) const noexcept {
    const JACK *const jack = find_jack(descriptor);
    return jack ? make_session(jack->id) : make_session(0, ERROR::NONE, false);
}

const int *SOCKETS::get_descriptor(size_t session) const noexcept {
    const JACK *const jack = find_jack(make_session(session));

    return jack ? &(jack->descriptor) : nullptr;
}

void SOCKETS::freeze(size_t sid) noexcept {
    JACK *const jack = find_jack(make_session(sid));

    if (jack) {
        jack->bitset.frozen = true;
    }
}

void SOCKETS::unfreeze(size_t sid) noexcept {
    JACK *const jack = find_jack(make_session(sid));

    if (jack) {
        if (!has_event(*jack, EVENT::DISCONNECTION)
        &&  !has_event(*jack, EVENT::CLOSE)) {
            jack->bitset.frozen = false;
        }
    }
}

bool SOCKETS::is_frozen(size_t sid) const noexcept {
    const JACK *const jack = find_jack(make_session(sid));

    return jack ? jack->bitset.frozen : false;
}

bool SOCKETS::idle() const noexcept {
    return bitset.timeout;
}

SOCKETS::SESSION SOCKETS::connect(const char *host, const char *port) noexcept {
    return connect(host, port, AF_UNSPEC, 0);
}

void SOCKETS::disconnect(size_t session_id) noexcept {
    JACK *jack = find_jack(make_session(session_id));

    if (jack) {
        terminate(jack->descriptor);
    }
}

SOCKETS::ERROR SOCKETS::err(ERROR e) noexcept {
    return (errored = e);
}

SOCKETS::ERROR SOCKETS::last_error() noexcept {
    return errored;
}

constexpr auto SOCKETS::fmt_bytes(size_t b) noexcept {
    constexpr const size_t one{1};
    struct format_type{
        double value;
        const char *unit;
    };

    return (
        (sizeof(b) * BITS_PER_BYTE > 40) && b > (one << 40) ? (
            format_type{
                double((long double)(b) / (long double)(one << 40)), "TiB"
            }
        ) :
        (sizeof(b) * BITS_PER_BYTE > 30) && b > (one << 30) ? (
            format_type{
                double((long double)(b) / (long double)(one << 30)), "GiB"
            }
        ) :
        (sizeof(b) * BITS_PER_BYTE > 20) && b > (one << 20) ? (
            format_type{ double(b) / double(one << 20), "MiB" }
        ) : format_type{ double(b) / double(one << 10), "KiB" }
    );
}

SOCKETS::ERROR SOCKETS::next_error(int timeout) noexcept {
    if (mempool.usage > mempool.top) {
        mempool.top = mempool.usage;

        log(
            "top memory usage is %.3f %s",
            fmt_bytes(mempool.top).value, fmt_bytes(mempool.top).unit
        );
    }

    if (bitset.reindex) {
        ERROR error{ reindex() };

        if (error != ERROR::NONE) {
            return err(error);
        }
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

        const PIPE *const event_subscribers = find_descriptors(event);

        if (!event_subscribers) continue;

        ERROR error = copy(*event_subscribers, descriptor_buffer);

        if (error != ERROR::NONE) {
            return err(error);
        }

        for (size_t j=0, sz=descriptor_buffer.size; j<sz; ++j) {
            int d = to_int(get_entry(descriptor_buffer, j));
            JACK *const jack = find_jack(d);

            if (jack == nullptr) continue;

            rem_event(*jack, event); // TODO: remove when handled successfullly!

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

size_t SOCKETS::incoming(size_t sid) const noexcept {
    const JACK *const jack = find_jack(make_session(sid));

    if (jack) {
        return jack->incoming.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::outgoing(size_t sid) const noexcept {
    const JACK *const jack = find_jack(make_session(sid));

    if (jack) {
        return jack->outgoing.size;
    }

    bug();

    return 0;
}

size_t SOCKETS::read(size_t sid, void *buf, size_t count) noexcept {
    if (!count) return 0;

    JACK *const jack = find_jack(make_session(sid));

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

const char *SOCKETS::read(size_t sid) noexcept {
    JACK *const jack = find_jack(make_session(sid));

    if (!jack || jack->incoming.size == 0) {
        return "";
    }

    PIPE &buffer = get_buffer(BUFFER::READ);

    if (buffer.capacity <= 1) {
        return "";
    }

    size_t count{
        read(
            sid, to_uint8(buffer),
            std::min(buffer.capacity - 1, jack->incoming.size)
        )
    };

    to_uint8(buffer)[count] = '\0';
    buffer.size = count + 1;

    return to_char(buffer);
}

SOCKETS::ERROR SOCKETS::write(
    size_t sid, const void *buf, size_t count
) noexcept {
    JACK *const jack = find_jack(make_session(sid));

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

SOCKETS::ERROR SOCKETS::write(size_t sid, const char *text) noexcept {
    return write(sid, text, std::strlen(text));
}

SOCKETS::ERROR SOCKETS::writef(size_t sid, const char *fmt, ...) noexcept {
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

    JACK &jack = get_jack(make_session(sid));

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

    heapbuf = static_cast<char *>(buffer.data);

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
                static constexpr const char *const OOM = "Out Of Memory!";

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

void SOCKETS::log(
    ERROR error, const char *file, int line, char const *function
) const noexcept {
    log("%s: %s (%s:%d)", function, get_code(error), file, line);
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
    if (jack.bitset.reconnect) {
        bool success = false;
        SESSION new_session{
            connect(
                get_host(jack.id), get_port(jack.id),
                jack.ai_family, jack.ai_flags, jack.blacklist
            )
        };

        if (new_session.valid) {
            JACK &new_jack = get_jack(new_session);

            if (new_jack.blacklist == nullptr) {
                new_jack.blacklist = jack.blacklist;
                jack.blacklist = nullptr;
            }
            else {
                struct addrinfo *blacklist = new_jack.blacklist;

                for (;; blacklist = blacklist->ai_next) {
                    if (blacklist->ai_next == nullptr) {
                        blacklist->ai_next = jack.blacklist;
                        jack.blacklist = nullptr;
                        break;
                    }
                }
            }

            // Let's swap the session IDs of these jacks so that the original
            // session ID would prevail.

            INDEX::ENTRY old_session_entry{
                find(INDEX::TYPE::SESSION_JACK, make_key(jack.id))
            };

            INDEX::ENTRY new_session_entry{
                find(INDEX::TYPE::SESSION_JACK, make_key(new_jack.id))
            };

            if (old_session_entry.valid && new_session_entry.valid) {
                set_value(old_session_entry, make_pipe_entry(&new_jack));
                set_value(new_session_entry, make_pipe_entry(&jack));
                std::swap(jack.id, new_jack.id);
                success = true;
            }
            else {
                bug();
                close_and_release(new_jack);
            }
        }
        else if (new_session.error != ERROR::NONE) {
            if (new_session.error == ERROR::OUT_OF_MEMORY) {
                set_event(jack, EVENT::CLOSE);

                return new_session.error;
            }
            else log(new_session.error);
        }

        if (!success) {
            jack.bitset.reconnect = false;
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

    close_and_release(jack);

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

    static constexpr const unsigned int max_int{
        static_cast<unsigned int>(std::numeric_limits<int>::max())
    };

    int maxevents{
        epoll_jack.epoll_ev.size > max_int ? (
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
            log(error);
        }

        if (old_size != new_size) {
            log("changed epoll event buffer size to %lu", new_size);
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
                    d, SOL_SOCKET, SO_ERROR, static_cast<void*>(&socket_error),
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
    PIPE &buffer = jack.incoming;

    if (!buffer.capacity) {
        ERROR error{ reserve(buffer, 1) };

        if (error != ERROR::NONE) {
            log(error);
            set_event(jack, EVENT::READ);

            return error;
        }
    }

    const int descriptor = jack.descriptor;

    for (size_t total_count = 0;;) {
        ssize_t count;
        char *const buf = to_char(buffer) + buffer.size;
        const size_t buf_sz = buffer.capacity - buffer.size;

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

        if (!total_count) {
            set_event(jack, EVENT::READ);
            set_event(jack, EVENT::INCOMING);
        }

        total_count += count;
        buffer.size += count;

        if (buffer.size == buffer.capacity && buffer.capacity < jack.intake) {
            ERROR error{
                // Errors here are not fatal because we are just trying to
                // increase the capacity of the buffer.

                reserve(buffer, std::min(2 * buffer.capacity, jack.intake))
            };

            if (error != ERROR::NONE
            &&  error != ERROR::OUT_OF_MEMORY) {
                log(error);
            }
        }
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

    const unsigned char *const bytes = to_uint8(outgoing);
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

        return ERROR::FORBIDDEN_CONDITION;
    }

    SESSION session{capture(make_jack(client_descriptor, descriptor))};

    if (!session.valid) {
        log(session.error);
        close_descriptor(client_descriptor);
        set_event(jack, EVENT::ACCEPT);
        return session.error;
    }

    JACK &client_jack = get_jack(client_descriptor);

    char host[NI_MAXHOST];
    char port[NI_MAXSERV];

    int retval = getnameinfo(
        &in_addr, in_len,
        host, socklen_t(std::extent<decltype(host)>::value),
        port, socklen_t(std::extent<decltype(port)>::value),
        NI_NUMERICHOST|NI_NUMERICSERV
    );

    if (retval != 0) {
        log(
            "getnameinfo: %s (%s:%d)", gai_strerror(retval),
            __FILE__, __LINE__
        );

        close_and_release(client_jack);

        return ERROR::FORBIDDEN_CONDITION;
    }
    else {
        const PIPE host_wrapper{
            make_pipe(
                reinterpret_cast<const uint8_t *>(host), std::strlen(host) + 1
            )
        };

        {
            ERROR error{ copy(host_wrapper, client_jack.host) };

            if (error != ERROR::NONE) {
                close_and_release(client_jack);
                set_event(jack, EVENT::ACCEPT);

                return error;
            }
        }

        const PIPE port_wrapper{
            make_pipe(
                reinterpret_cast<const uint8_t *>(port), std::strlen(port) + 1
            )
        };

        {
            ERROR error{ copy(port_wrapper, client_jack.port) };

            if (error != ERROR::NONE) {
                close_and_release(client_jack);
                set_event(jack, EVENT::ACCEPT);

                return error;
            }
        }
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

        close_and_release(client_jack);

        return ERROR::FORBIDDEN_CONDITION;
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

SOCKETS::SESSION SOCKETS::connect(
    const char *host, const char *port, int ai_family, int ai_flags,
    const struct addrinfo *blacklist, const char *file, int line
) noexcept {
    int epoll_descriptor = get_epoll_jack().descriptor;

    SESSION session{
        open_and_capture(
            host, port, ai_family, ai_flags, {}, blacklist, file, line
        )
    };

    if (!session.valid) {
        return session;
    }

    JACK &jack = get_jack(session);
    const int descriptor = jack.descriptor;

    const PIPE host_wrapper{
        make_pipe(
            reinterpret_cast<const uint8_t *>(host), std::strlen(host) + 1
        )
    };

    {
        ERROR error{ copy(host_wrapper, jack.host) };

        if (error != ERROR::NONE) {
            close_and_release(jack);

            return make_session(error);
        }
    }

    const PIPE port_wrapper{
        make_pipe(
            reinterpret_cast<const uint8_t *>(port), std::strlen(port) + 1
        )
    };

    {
        ERROR error{ copy(port_wrapper, jack.port) };

        if (error != ERROR::NONE) {
            close_and_release(jack);

            return make_session(error);
        }
    }

    if (!bind_to_epoll(descriptor, epoll_descriptor)) {
        close_and_release(jack);

        return make_session(ERROR::UNSPECIFIED);
    }

    if (jack.bitset.connecting) {
        modify_epoll(descriptor, EPOLLOUT|EPOLLET);
    }
    else {
        jack.bitset.may_shutdown = true;
        set_event(jack, EVENT::CONNECTION);
    }

    return session;
}

void SOCKETS::terminate(int descriptor, const char *file, int line) noexcept {
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
        for (size_t i=0; i<jack.children.size; ++i) {
            terminate(to_int(get_entry(jack.children, i)), file, line);
        }
    }
}

SOCKETS::SESSION SOCKETS::listen(
    const char *host, const char *port, int ai_family, int ai_flags,
    const std::initializer_list<int> options, const char *file, int line
) noexcept {
    SESSION session{open_and_capture(host, port, ai_family, ai_flags, options)};

    if (!session.valid) return session;

    JACK &jack = get_jack(session);
    const int descriptor = jack.descriptor;
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

        close_and_release(jack);

        return make_session(ERROR::UNSPECIFIED);
    }

    const int epoll_descriptor = get_epoll_jack().descriptor;

    if (!bind_to_epoll(descriptor, epoll_descriptor)) {
        close_and_release(jack);

        return make_session(ERROR::UNSPECIFIED);
    }

    set_event(jack, EVENT::ACCEPT);

    jack.bitset.listener = true;

    struct sockaddr in_addr;
    socklen_t in_len = sizeof(in_addr);

    retval = getsockname(
        descriptor, static_cast<struct sockaddr *>(&in_addr), &in_len
    );

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

        close_and_release(jack);

        return make_session(ERROR::UNSPECIFIED);
    }

    char host_buf[NI_MAXHOST];
    char port_buf[NI_MAXSERV];

    retval = getnameinfo(
        &in_addr, in_len,
        host_buf, socklen_t(std::extent<decltype(host_buf)>::value),
        port_buf, socklen_t(std::extent<decltype(port_buf)>::value),
        NI_NUMERICHOST|NI_NUMERICSERV
    );

    if (retval != 0) {
        log(
            "getnameinfo: %s (%s:%d)", gai_strerror(retval),
            __FILE__, __LINE__
        );

        close_and_release(jack);

        return make_session(ERROR::UNSPECIFIED);
    }

    const PIPE host_wrapper{
        make_pipe(
            reinterpret_cast<const uint8_t *>(host_buf),
            std::strlen(host_buf) + 1
        )
    };

    {
        ERROR error{ copy(host_wrapper, jack.host) };

        if (error != ERROR::NONE) {
            close_and_release(jack);

            return make_session(error);
        }
    }

    const PIPE port_wrapper{
        make_pipe(
            reinterpret_cast<const uint8_t *>(port_buf),
            std::strlen(port_buf) + 1
        )
    };

    {
        ERROR error{ copy(port_wrapper, jack.port) };

        if (error != ERROR::NONE) {
            close_and_release(jack);

            return make_session(error);
        }
    }

    return session;
}

SOCKETS::SESSION SOCKETS::create_epoll() noexcept {
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

        return make_session(ERROR::UNSPECIFIED);
    }

    SESSION session{capture(make_jack(epoll_descriptor))};

    if (!session.valid) {
        log(session.error);
        close_descriptor(epoll_descriptor);

        return make_session(session.error);
    }

    JACK &jack = get_jack(epoll_descriptor);

    {
        ERROR error{ reserve(jack.epoll_ev, 1) };

        if (error != ERROR::NONE) {
            log(error);
            close_and_release(jack);

            return make_session(error);
        }

        jack.epoll_ev.size = jack.epoll_ev.capacity;
    }

    set_event(jack, EVENT::EPOLL);

    return session;
}

bool SOCKETS::bind_to_epoll(int descriptor, int epoll_descriptor) noexcept {
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

SOCKETS::SESSION SOCKETS::open_and_capture(
    const char *host, const char *port, int ai_family, int ai_flags,
    const std::initializer_list<int> options,
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

    JACK *jack = nullptr;
    ERROR error = ERROR::NONE;

    int retval = getaddrinfo(host, port, &hint, &info);
    if (retval != 0) {
        log("getaddrinfo: %s (%s:%d)", gai_strerror(retval), file, line);

        goto CleanUp;
    }

    for (next = info; next; prev = next, next = next->ai_next) {
        if (is_listed(*next, blacklist)) {
            continue;
        }

        int descriptor{
            accept_incoming_connections ? (
                socket(
                    next->ai_family,
                    next->ai_socktype|SOCK_NONBLOCK|SOCK_CLOEXEC,
                    next->ai_protocol
                )
            ) : (
                socket(
                    next->ai_family,
                    next->ai_socktype|SOCK_CLOEXEC|(
                        establish_nonblocking_connections ? SOCK_NONBLOCK : 0
                    ),
                    next->ai_protocol
                )
            )
        };

        if (descriptor == -1) {
            int code = errno;

            log("socket: %s (%s:%d)", strerror(code), __FILE__, __LINE__);

            continue;
        }

        SESSION session{capture(make_jack(descriptor))};
        error = session.error;

        if (session.valid) {
            jack = &get_jack(descriptor);
            jack->ai_family = ai_family;
            jack->ai_flags  = ai_flags;
        }
        else {
            log(error);
            close_descriptor(descriptor);

            continue;
        }

        if (accept_incoming_connections) {
            bool success = true;

            for (int option : options) {
                int optval = 1;

                setsockopt(
                    descriptor, SOL_SOCKET, option,
                    static_cast<const void *>(&optval), sizeof(optval)
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

                    success = false;
                    break;
                }
            }

            if (success) {
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
        else {
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

        close_and_release(*jack);

        jack = nullptr;
    }

    CleanUp:
    if (info) freeaddrinfo(info);

    return jack ? make_session(jack->id) : make_session(error);
}

void SOCKETS::close_descriptor(int descriptor) noexcept {
    // Let's block all signals before calling close because we don't
    // want it to fail due to getting interrupted by a singal.

    int retval = sigprocmask(SIG_SETMASK, &sigset_all, &sigset_orig);
    if (retval == -1) {
        int code = errno;
        log("sigprocmask: %s (%s:%d)", strerror(code), __FILE__, __LINE__);
        die();

        return;
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );
        die();

        return;
    }

    retval = close(descriptor);
    if (retval) {
        if (retval == -1) {
            int code = errno;

            log(
                "close(%d): %s (%s:%d)", descriptor, strerror(code),
                __FILE__, __LINE__
            );
        }
        else {
            log(
                "close(%d): unexpected return value %d (%s:%d)",
                descriptor, retval, __FILE__, __LINE__
            );
        }
    }

    retval = sigprocmask(SIG_SETMASK, &sigset_orig, nullptr);
    if (retval == -1) {
        int code = errno;
        log("sigprocmask: %s (%s:%d)", strerror(code), __FILE__, __LINE__);
        die();
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );
        die();
    }
}

void SOCKETS::close_and_release(JACK &jack) noexcept {
    // Let's block all signals before calling close because we don't
    // want it to fail due to getting interrupted by a singal.
    int retval = sigprocmask(SIG_SETMASK, &sigset_all, &sigset_orig);
    if (retval == -1) {
        int code = errno;
        log(
            "sigprocmask: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );
        die();

        return;
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );
        die();

        return;
    }

    JACK *last = &jack;

    for (;;) {
        JACK *next_last{
            last->children.size ? (
                find_jack(to_int(get_last(last->children)))
            ) : nullptr
        };

        if (next_last) {
            last = next_last;
            continue;
        }

        int d = last->descriptor;
        retval = close(d);

        if (retval) {
            if (retval == -1) {
                int code = errno;

                log(
                    "close(%d): %s (%s:%d)", d, strerror(code),
                    __FILE__, __LINE__
                );
            }
            else {
                log(
                    "close(%d): unexpected return value %d (%s:%d)", d, retval,
                    __FILE__, __LINE__
                );
            }
        }

        release(last);

        if (last == &jack) {
            break;
        }

        last = &jack;
    }

    retval = sigprocmask(SIG_SETMASK, &sigset_orig, nullptr);
    if (retval == -1) {
        int code = errno;
        log(
            "sigprocmask: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );
        die();
    }
    else if (retval) {
        log(
            "sigprocmask: unexpected return value %d (%s:%d)", retval,
            __FILE__, __LINE__
        );
        die();
    }
}

SOCKETS::SESSION SOCKETS::capture(const JACK &copy) noexcept {
    if (!is_descriptor(copy.descriptor)) {
        die();
    }

    PIPE *siblings = nullptr;

    if (is_descriptor(copy.parent.descriptor)) {
        siblings = &get_jack(copy.parent.descriptor).children;

        ERROR error{ insert(*siblings, make_pipe_entry(copy.descriptor)) };

        if (error != ERROR::NONE) {
            return make_session(error);
        }
    }

    int descriptor = copy.descriptor;
    JACK *const jack = new_jack(&copy);

    if (!jack) {
        if (siblings) {
            pop_back(*siblings);
        }

        return make_session(ERROR::OUT_OF_MEMORY);
    }

    if (siblings) {
        if (!siblings->size) {
            return make_session(die());
        }

        size_t index = siblings->size - 1;
        static constexpr const size_t max_index{
            std::numeric_limits<decltype(jack->parent.child_index)>::max()
        };

        if (index > max_index) {
            return make_session(die());
        }

        jack->parent.child_index = (
            static_cast<decltype(jack->parent.child_index)>(index)
        );
    }

    jack->id = last_jack_id + 1;

    ERROR error = ERROR::NONE;

    for (;;) {
        {
            INDEX::ENTRY entry{
                insert(
                    INDEX::TYPE::SESSION_JACK,
                    make_key(jack->id), make_pipe_entry(jack)
                )
            };

            if (!entry.valid) {
                error = entry.error;
                break;
            }
        }

        {
            INDEX::ENTRY entry{
                insert(
                    INDEX::TYPE::DESCRIPTOR_JACK,
                    make_key(descriptor), make_pipe_entry(jack)
                )
            };

            if (!entry.valid) {
                error = entry.error;
                break;
            }
        }

        break;
    }

    if (!error) {
        return make_session( (last_jack_id = jack->id) );
    }

    release(jack);

    return make_session(error);
}

void SOCKETS::release(JACK *jack) noexcept {
    if (!jack) {
        return bug();
    }

    for (auto &ev : jack->event_lookup) {
        rem_event(*jack, static_cast<EVENT>(&ev - &(jack->event_lookup[0])));
    }

    if (is_descriptor(jack->parent.descriptor)) {
        rem_child(get_jack(jack->parent.descriptor), *jack);
    }

    if (jack->children.size) {
        bug(); // Children should have been already released.
    }

    destroy(jack->epoll_ev);
    destroy(jack->children);
    destroy(jack->incoming);
    destroy(jack->outgoing);
    destroy(jack->host);
    destroy(jack->port);

    if (jack->blacklist) {
        freeaddrinfo(jack->blacklist);
    }

    erase(INDEX::TYPE::DESCRIPTOR_JACK, make_key(jack->descriptor));
    erase(INDEX::TYPE::SESSION_JACK,    make_key(jack->id));

    recycle(get_memory(jack));
}

SOCKETS::JACK *SOCKETS::find_jack(
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

SOCKETS::JACK *SOCKETS::find_jack(
    SESSION session //TODO: refactor so that size_t could be used directly
) const noexcept {
    if (!session.valid) return nullptr;

    INDEX::ENTRY entry{
        find(INDEX::TYPE::SESSION_JACK, make_key(session.id))
    };

    if (!entry.valid) {
        return nullptr;
    }

    return to_jack(get_entry(*entry.val_pipe, entry.index));
}

SOCKETS::JACK *SOCKETS::find_jack(EVENT ev) const noexcept {
    INDEX::ENTRY entry{
        find(INDEX::TYPE::EVENT_DESCRIPTOR, make_key(ev))
    };

    if (entry.valid) {
        const int descriptor = to_int(get_value(entry));
        return &get_jack(descriptor); // If it's indexed, then it must exist.
    }

    return nullptr;
}

SOCKETS::JACK *SOCKETS::find_epoll_jack() const noexcept {
    return find_jack(EVENT::EPOLL);
}

SOCKETS::JACK &SOCKETS::get_jack(int descriptor) const noexcept {
    JACK *const rec = find_jack(descriptor);

    if (!rec) die();

    return *rec;
}

SOCKETS::JACK &SOCKETS::get_jack(SESSION session) const noexcept {
    JACK *const rec = find_jack(session);

    if (!rec) die();

    return *rec;
}

SOCKETS::JACK &SOCKETS::get_epoll_jack() const noexcept {
    JACK *const rec = find_epoll_jack();

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

SOCKETS::KEY SOCKETS::to_key(PIPE::ENTRY entry) const noexcept {
    if (entry.type != PIPE::TYPE::KEY) die();

    return entry.as_key;
}

int *SOCKETS::to_int(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::INT) die();

    return static_cast<int *>(pipe.data);
}

uint8_t *SOCKETS::to_uint8(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::UINT8) die();

    return static_cast<uint8_t *>(pipe.data);
}

char *SOCKETS::to_char(const PIPE &pipe) const noexcept {
    if (pipe.type != PIPE::TYPE::UINT8) die();

    return static_cast<char *>(pipe.data);
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

void *SOCKETS::to_ptr(PIPE::ENTRY &entry) const noexcept {
    switch (entry.type) {
        case PIPE::TYPE::PTR:
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:    return &(entry.as_ptr);
        case PIPE::TYPE::UINT8:       return &(entry.as_uint8);
        case PIPE::TYPE::UINT64:      return &(entry.as_uint64);
        case PIPE::TYPE::INT:         return &(entry.as_int);
        case PIPE::TYPE::KEY:         return &(entry.as_key);
        case PIPE::TYPE::EPOLL_EVENT: return &(entry.as_epoll_event);
        case PIPE::TYPE::NONE:        break;
    }

    die();

    return nullptr;
}

void *SOCKETS::to_ptr(PIPE &pipe, size_t index) const noexcept {
    switch (pipe.type) {
        case PIPE::TYPE::PTR:
        case PIPE::TYPE::MEMORY_PTR:
        case PIPE::TYPE::JACK_PTR:    return to_ptr(pipe) + index;
        case PIPE::TYPE::UINT8:       return to_uint8(pipe) + index;
        case PIPE::TYPE::UINT64:      return to_uint64(pipe) + index;
        case PIPE::TYPE::INT:         return to_int(pipe) + index;
        case PIPE::TYPE::KEY:         return to_key(pipe) + index;
        case PIPE::TYPE::EPOLL_EVENT: return to_epoll_event(pipe) + index;
        case PIPE::TYPE::NONE:        break;
    }

    die();

    return nullptr;
}

const void *SOCKETS::to_ptr(const PIPE &pipe, size_t index) const noexcept {
    return to_ptr(const_cast<PIPE&>(pipe), index);
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

    unsigned pos = jack.event_lookup[index];

    if (pos != std::numeric_limits<unsigned>::max()) {
        return ERROR::NONE; // Already set.
    }

    INDEX::ENTRY entry{
        insert(
            INDEX::TYPE::EVENT_DESCRIPTOR,
            make_key(event), make_pipe_entry(jack.descriptor)
        )
    };

    if (entry.valid) {
        if (entry.index >= std::numeric_limits<unsigned>::max()) {
            die(); // the number of descriptors is limited by the max of int.
        }

        jack.event_lookup[index] = unsigned(entry.index);
        return ERROR::NONE;
    }

    return entry.error;
}

void SOCKETS::rem_event(JACK &jack, EVENT event) noexcept {
    size_t index = static_cast<size_t>(event);

    if (index >= std::extent<decltype(jack.event_lookup)>::value) {
        die();
    }

    unsigned pos = jack.event_lookup[index];

    if (pos == std::numeric_limits<unsigned>::max()) {
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
        int other_descriptor = to_int(get_value(entry));
        get_jack(other_descriptor).event_lookup[index] = pos;
    }

    jack.event_lookup[index] = std::numeric_limits<unsigned>::max();
}

bool SOCKETS::has_event(const JACK &jack, EVENT event) const noexcept {
    size_t index = static_cast<size_t>(event);

    if (index >= std::extent<decltype(jack.event_lookup)>::value) {
        return false;
    }

    unsigned pos = jack.event_lookup[index];

    if (pos != std::numeric_limits<unsigned>::max()) {
        return true;
    }

    return false;
}

void SOCKETS::rem_child(JACK &jack, JACK &child) const noexcept {
    if (child.parent.child_index < 0
    || jack.descriptor != child.parent.descriptor) {
        die();
        return;
    }

    const decltype(PIPE::size) index = child.parent.child_index;

    if (index + 1 == jack.children.size) {
        pop_back(jack.children);
    }
    else {
        PIPE::ENTRY entry { pop_back(jack.children) };
        replace(jack.children, index, entry);

        JACK &sibling = get_jack(to_int(entry));

        sibling.parent.child_index = (
            static_cast<decltype(sibling.parent.child_index)>(index)
        );
    }

    child.parent.descriptor = -1;
    child.parent.child_index = -1;
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

    const KEY *const data = to_key(key_pipe);

    if (!data) {
        return {};
    }

    PIPE &val_pipe = table.value;

    if (value.type != PIPE::TYPE::NONE && value.type != val_pipe.type) {
        die();
    }

    size_t sz = key_pipe.size;
    size_t i = std::min(sz-1, start_i);

    for (; i<sz && iterations; --i, --iterations) {
        if (data[i].value != key.value) {
            continue;
        }

        if (value.type != PIPE::TYPE::NONE
        && std::memcmp(to_ptr(val_pipe, i), to_ptr(value), size(value.type))) {
            continue;
        }

        INDEX::ENTRY entry{};

        entry.index = i;
        entry.valid = true;
        entry.key_pipe = &key_pipe;
        entry.val_pipe = &val_pipe;

        return entry;
    }

    return {};
}

SOCKETS::INDEX::ENTRY SOCKETS::insert(
    INDEX::TYPE index_type, KEY key, PIPE::ENTRY value
) noexcept {
    INDEX &index = indices[size_t(index_type)];

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

        if (error == ERROR::NONE) {
            if (++index.entries > index.buckets && index.autogrow) {
                bitset.reindex = true;
            }
        }
        else {
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
    INDEX &index = indices[size_t(index_type)];

    if (index.buckets <= 0) die();

    INDEX::TABLE &table = index.table[key.value % index.buckets];
    PIPE &key_pipe = table.key;

    if (key_pipe.type != PIPE::TYPE::KEY) die();

    KEY *const key_data = to_key(key_pipe);

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

        if (value.type != PIPE::TYPE::NONE
        && std::memcmp(to_ptr(val_pipe, i), to_ptr(value), size(value.type))) {
            i = index.multimap ? i-1 : key_pipe.size;
            continue;
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

    index.entries -= erased;

    return erased;
}

size_t SOCKETS::count(INDEX::TYPE index_type, KEY key) const noexcept {
    size_t count = 0;
    const INDEX &index = indices[size_t(index_type)];

    if (index.buckets > 0) {
        const INDEX::TABLE &table = index.table[key.value % index.buckets];
        const PIPE &pipe = table.key;
        const KEY *const data = to_key(pipe);

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

SOCKETS::ERROR SOCKETS::reindex() noexcept {
    for (INDEX &index : indices) {
        if (!index.autogrow || index.entries <= index.buckets) {
            continue;
        }

        const size_t new_buckets = next_pow2(index.entries);

        INDEX::TABLE *new_table = allocate_tables(new_buckets);
        INDEX::TABLE *old_table = index.table;

        if (new_table) {
            for (size_t i=0; i<new_buckets; ++i) {
                new_table[i].key.type = old_table->key.type;
                new_table[i].value.type = old_table->value.type;
            }
        }
        else {
            return ERROR::OUT_OF_MEMORY;
        }

        const size_t old_buckets = index.buckets;
        const size_t old_entries = index.entries;

        index.table = new_table;
        index.buckets = new_buckets;
        index.entries = 0;

        for (size_t i=0; i<old_buckets; ++i) {
            INDEX::TABLE &table = old_table[i];

            for (size_t j=0, sz=table.value.size; j<sz; ++j) {
                INDEX::ENTRY entry{
                    insert(
                        index.type,
                        to_key(get_entry(table.key, j)),
                        get_entry(table.value, j)
                    )
                };

                if (!entry.valid) {
                    index.table = old_table;
                    index.buckets = old_buckets;
                    index.entries = old_entries;

                    destroy_and_delete(new_table, new_buckets);

                    return entry.error;
                }
            }
        }

        destroy_and_delete(old_table, old_buckets);

        if (index.entries != old_entries) {
            bug();
        }
    }

    bitset.reindex = false;

    return ERROR::NONE;
}

void SOCKETS::replace(
    PIPE &pipe, size_t index, PIPE::ENTRY value
) const noexcept {
    if (index >= pipe.size) {
        die();
    }
    else if (pipe.type != value.type) {
        die();
    }

    std::memcpy(to_ptr(pipe, index), to_ptr(value), size(value.type));
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

    size_t element_size = size(pipe.type);
    size_t byte_count = element_size * capacity;

    if (!byte_count) {
        die();
        return ERROR::FORBIDDEN_CONDITION;
    }

    MEMORY *const old_memory = pipe.memory;

    if (old_memory && old_memory->size / element_size >= capacity) {
        pipe.capacity = capacity;

        return ERROR::NONE;
    }

    MEMORY *const new_memory = allocate(byte_count);

    if (!new_memory) {
        return ERROR::OUT_OF_MEMORY;
    }

    void *const old_data = pipe.data;
    void *const new_data = new_memory->data;

    if (old_data) {
        std::memcpy(new_data, old_data, pipe.size * element_size);
    }

    if (old_memory) {
        recycle(*old_memory);
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

    std::memcpy(to_ptr(dst, old_size), to_ptr(src, 0), count * size(dst.type));

    return ERROR::NONE;
}

void SOCKETS::erase(PIPE &pipe, size_t index) const noexcept {
    if (index >= pipe.size) {
        die();
    }

    if (index + 1 >= pipe.size) {
        --pipe.size;
        return;
    }

    std::memcpy(
        to_ptr(pipe, index), to_ptr(pipe, pipe.size - 1), size(pipe.type)
    );

    --pipe.size;
}

SOCKETS::PIPE::ENTRY SOCKETS::pop_back(PIPE &pipe) const noexcept {
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

    PIPE::ENTRY entry{};

    entry.type = pipe.type;

    std::memcpy(to_ptr(entry), to_ptr(pipe, index), size(entry.type));

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
        recycle(*pipe.memory);
        pipe.memory = nullptr;
    }

    pipe.data = nullptr;
    pipe.capacity = 0;
    pipe.size = 0;
}

void SOCKETS::enlist(MEMORY &memory, MEMORY *&list) noexcept {
    if (memory.next || memory.prev) die();

    memory.next = list;

    if (list) {
        list->prev = &memory;
    }

    list = &memory;
}

void SOCKETS::unlist(MEMORY &memory, MEMORY *&list) noexcept {
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

        memory.indexed = false;
    }

    if (list == &memory) {
        list = memory.next;

        if (list) {
            list->prev = nullptr;
        }
    }
    else {
        memory.prev->next = memory.next;

        if (memory.next) {
            memory.next->prev = memory.prev;
        }
    }

    memory.next = nullptr;
    memory.prev = nullptr;
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
    const MEMORY *const memory = find_memory(resource);

    if (!memory) die();

    return *memory;
}

SOCKETS::MEMORY &SOCKETS::get_memory(const void *resource) noexcept {
    MEMORY *const memory = find_memory(resource);

    if (!memory) die();

    return *memory;
}

SOCKETS::INDEX::TABLE *SOCKETS::allocate_tables(size_t count) noexcept {
    const size_t total_size = sizeof(INDEX::TABLE) * count;
    const auto usage_left{
        std::numeric_limits<decltype(mempool.usage)>::max() - mempool.usage
    };

    INDEX::TABLE *tables = (
        usage_left >= total_size &&
        mempool.cap >= mempool.usage + total_size ? (
            new (std::nothrow) INDEX::TABLE [count]()
        ) : nullptr
    );

    if (!tables) {
        return nullptr;
    }

    mempool.usage += total_size;

    return tables;
}

void SOCKETS::destroy_and_delete(INDEX::TABLE *tables, size_t count) noexcept {
    for (size_t i=0; i<count; ++i) {
        destroy(tables[i].key);
        destroy(tables[i].value);
    }

    delete [] tables;

    mempool.usage -= sizeof(INDEX::TABLE) * count;
}

SOCKETS::MEMORY *SOCKETS::allocate(size_t requested_byte_count) noexcept {
    //TODO: if memcap is met, try to deallocate before reporting OOM
    size_t byte_count = next_pow2(requested_byte_count);

    const size_t total_size = sizeof(MEMORY) + byte_count;

    MEMORY *memory = nullptr;
    uint8_t *array = nullptr;

    MEMORY *&free = mempool.free[clz(byte_count)];

    if (free) {
        memory = free;
        unlist(*memory, free);
        array = reinterpret_cast<uint8_t*>(memory);
    }

    if (!memory) {
        const auto usage_left{
            std::numeric_limits<decltype(mempool.usage)>::max() - mempool.usage
        };

        array = (
            usage_left >= total_size &&
            mempool.cap >= mempool.usage + total_size ? (
                new (std::nothrow) uint8_t[total_size]
            ) : nullptr
        );

        if (!array) {
            return nullptr;
        }

        mempool.usage += total_size;

        memory = reinterpret_cast<MEMORY *>(array);

        memory->size = byte_count;
        memory->data = byte_count ? (array + sizeof(MEMORY)) : nullptr;
    }

    memory->next = nullptr;
    memory->prev = nullptr;
    memory->indexed = false;
    memory->recycled = false;

    enlist(*memory, mempool.list);

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
        recycle(*memory);
        return nullptr;
    }

    return memory;
}

void SOCKETS::deallocate(MEMORY &memory) noexcept {
    if (memory.recycled) {
        MEMORY *&free = mempool.free[clz(memory.size)];
        unlist(memory, free);
    }
    else {
        unlist(memory, mempool.list);
    }

    const size_t total_size = sizeof(MEMORY) + memory.size;

    delete [] reinterpret_cast<uint8_t *>(&memory);

    if (total_size <= mempool.usage) {
        mempool.usage -= total_size;
    }
    else {
        bug();
        mempool.usage = 0;
    }
}

void SOCKETS::recycle(MEMORY &memory) noexcept {
    if (memory.recycled) {
        return;
    }

    unlist(memory, mempool.list);
    enlist(memory, mempool.free[clz(memory.size)]);

    memory.recycled = true;
}

SOCKETS::JACK *SOCKETS::new_jack(const JACK *copy) noexcept {
    MEMORY *const mem = allocate_and_index(sizeof(JACK), copy);
    return mem ? reinterpret_cast<JACK *>(mem->data) : nullptr;
}

constexpr SOCKETS::JACK SOCKETS::make_jack(
    int descriptor, int parent
) noexcept {
#if __cplusplus <= 201703L
    __extension__
#endif
    JACK jack{
        .id           = 0,
        .intake       = std::numeric_limits<size_t>::max(),
        .event_lookup = {},
        .epoll_ev     = { make_pipe(PIPE::TYPE::EPOLL_EVENT) },
        .children     = { make_pipe(PIPE::TYPE::INT  ) },
        .incoming     = { make_pipe(PIPE::TYPE::UINT8) },
        .outgoing     = { make_pipe(PIPE::TYPE::UINT8) },
        .host         = { make_pipe(PIPE::TYPE::UINT8) },
        .port         = { make_pipe(PIPE::TYPE::UINT8) },
        .descriptor   = descriptor,
        .parent       = { .descriptor = parent, .child_index = -1 },
        .ai_family    = 0,
        .ai_flags     = 0,
        .blacklist    = nullptr,
        .bitset       = {}
    };

    for (auto &lookup_value : jack.event_lookup) {
        lookup_value = std::numeric_limits<unsigned>::max();
    }

    return jack;
}

constexpr SOCKETS::ALERT SOCKETS::make_alert(
    size_t session, EVENT event, bool valid
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    ALERT{
        .session = session,
        .event = event,
        .valid = valid
    };
}

constexpr SOCKETS::SESSION SOCKETS::make_session(
    size_t id, ERROR error, bool valid
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SESSION{
        .id    = id,
        .error = error,
        .valid = valid
    };
}

constexpr SOCKETS::SESSION SOCKETS::make_session(ERROR error) noexcept {
    return make_session(0, error, false);
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

constexpr SOCKETS::MEMPOOL SOCKETS::make_mempool() noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    SOCKETS::MEMPOOL{
        .free  = {},
        .list  = nullptr,
        .usage = 0,
        .top   = 0,
        .cap   = std::numeric_limits<decltype(MEMPOOL::cap)>::max()
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

        const void *const first = static_cast<const void *>(info.ai_addr);
        const void *const second = static_cast<const void *>(next->ai_addr);

        if (std::memcmp(first, second, (size_t) info.ai_addrlen) != 0) {
            continue;
        }

        return true;
    }

    return false;
}

constexpr bool SOCKETS::is_descriptor(int d) noexcept {
    return d >= 0;
}

const char *SOCKETS::get_code(ERROR error) noexcept {
    switch (error) {
        case ERROR::NONE:                return "NONE";
        case ERROR::OUT_OF_MEMORY:       return "OUT_OF_MEMORY";
        case ERROR::UNDEFINED_BEHAVIOR:  return "UNDEFINED_BEHAVIOR";
        case ERROR::FORBIDDEN_CONDITION: return "FORBIDDEN_CONDITION";
        case ERROR::UNHANDLED_EVENTS:    return "UNHANDLED_EVENTS";
        case ERROR::UNSPECIFIED:         return "UNSPECIFIED";
    }

    return "UNKNOWN_ERROR";
}

constexpr SOCKETS::EVENT SOCKETS::next(EVENT event_type) noexcept {
    return static_cast<EVENT>(
        (static_cast<size_t>(event_type) + 1) % (
            static_cast<size_t>(EVENT::MAX_EVENTS)
        )
    );
}

constexpr size_t SOCKETS::size(PIPE::TYPE type) noexcept {
    switch (type) {
        case PIPE::TYPE::UINT8:       return sizeof(uint8_t);
        case PIPE::TYPE::UINT64:      return sizeof(uint64_t);
        case PIPE::TYPE::INT:         return sizeof(int);
        case PIPE::TYPE::PTR:         return sizeof(void *);
        case PIPE::TYPE::JACK_PTR:    return sizeof(JACK *);
        case PIPE::TYPE::MEMORY_PTR:  return sizeof(MEMORY *);
        case PIPE::TYPE::KEY:         return sizeof(KEY);
        case PIPE::TYPE::EPOLL_EVENT: return sizeof(epoll_event);
        case PIPE::TYPE::NONE:        break;
    }

    return 0;
}

int SOCKETS::clz(unsigned int x) noexcept {
    return __builtin_clz(x);
}

int SOCKETS::clz(unsigned long x) noexcept {
    return __builtin_clzl(x);
}

int SOCKETS::clz(unsigned long long x) noexcept {
    return __builtin_clzll(x);
}

unsigned int SOCKETS::next_pow2(unsigned int x) noexcept {
    return x <= 1 ? 1 : 1 << ((sizeof(x) * BITS_PER_BYTE) - clz(x - 1));
}

unsigned long SOCKETS::next_pow2(unsigned long x) noexcept {
    return x <= 1 ? 1 : 1 << ((sizeof(x) * BITS_PER_BYTE) - clz(x - 1));
}

unsigned long long SOCKETS::next_pow2(unsigned long long x) noexcept {
    return x <= 1 ? 1 : 1 << ((sizeof(x) * BITS_PER_BYTE) - clz(x - 1));
}

#endif
