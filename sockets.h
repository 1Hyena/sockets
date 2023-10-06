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

#include <vector> // TODO: get rid of this
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
    bool serve(int timeout =-1) noexcept;
    bool idle() const noexcept;

    struct EVENT next_event() noexcept;

    const char *read(int descriptor) noexcept;
    void write(int descriptor, const char *text) noexcept;
    void writef(
        int descriptor, const char *fmt, ...
    ) noexcept __attribute__((format(printf, 3, 4)));

    bool swap_incoming(int descriptor, std::vector<uint8_t> &bytes) noexcept;
    bool swap_outgoing(int descriptor, std::vector<uint8_t> &bytes) noexcept;
    bool append_outgoing(
        int descriptor, const uint8_t *buffer, size_t size
    ) noexcept;

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
    static constexpr const size_t MAX_CACHE_SIZE = 1024 * 1024;

    enum class ERROR : uint8_t {
        NONE = 0,
        OUT_OF_MEMORY,
        UNDEFINED_BEHAVIOR
    };

    enum class FLAG : uint8_t {
        NONE           =  0,
        TIMEOUT        =  1,
        // Do not change the order of the flags above this line.
        RECONNECT      =  2,
        READ           =  3,
        WRITE          =  4,
        ACCEPT         =  5,
        NEW_CONNECTION =  6,
        DISCONNECT     =  7,
        CLOSE          =  8,
        INCOMING       =  9,
        MAY_SHUTDOWN   = 10,
        LISTENER       = 11,
        CONNECTING     = 12,
        FUSED          = 13,
        // Do not change the order of the flags below this line.
        EPOLL          = 14,
        MAX_FLAGS      = 15
    };

    enum class CACHE : uint8_t {
        GENERIC_INT = 0,
        // Do not change the order of items below this line.
        MAX_CACHES  = 1
    };

    struct PIPE {
        enum class TYPE : uint8_t {
            NONE = 0,
            UINT8,
            UINT64,
            INT,
            JACK_PTR
        };

        struct ENTRY {
            union {
                uint8_t  as_uint8;
                uint64_t as_uint64;
                int      as_int;
                void    *as_ptr;
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
            NONE            = 0,
            // Do not change the order of the types above this line.
            GROUP_SIZE      = 1,
            FLAG_DESCRIPTOR = 2,
            // Do not change the order of the types below this line.
            MAX_TYPES       = 3
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
        std::vector<uint8_t> *incoming;
        std::vector<uint8_t> *outgoing;
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

    inline static constexpr struct PIPE::ENTRY make_pipe_entry(
        uint64_t value
    ) noexcept;

    inline static constexpr struct PIPE::ENTRY make_pipe_entry(
        int value
    ) noexcept;

    inline static bool is_listed(
        const addrinfo &info, const addrinfo *list
    ) noexcept;

    bool handle_close(int descriptor) noexcept;
    bool handle_epoll(int epoll_descriptor, int timeout) noexcept;
    bool handle_read(int descriptor) noexcept;
    bool handle_write(int descriptor) noexcept;
    bool handle_accept(int descriptor) noexcept;

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

    [[nodiscard]] ERROR push(const jack_type jack) noexcept;
    jack_type pop(int descriptor) noexcept;

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
    size_t erase(
        INDEX::TYPE, uint64_t key, PIPE::ENTRY value ={},
        size_t start_i =std::numeric_limits<size_t>::max(),
        size_t iterations =std::numeric_limits<size_t>::max()
    ) noexcept;
    [[nodiscard]] INDEX::ENTRY insert(
        INDEX::TYPE, uint64_t key, PIPE::ENTRY value
    ) noexcept;
    void erase(PIPE &pipe, size_t index) noexcept;
    void destroy(PIPE &pipe) noexcept;
    [[nodiscard]] ERROR reserve(PIPE&, size_t capacity) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR insert(PIPE&, size_t index, PIPE::ENTRY) noexcept;
    [[nodiscard]] ERROR copy(const PIPE &src, PIPE &dst) noexcept;
    PIPE &get_cache(CACHE) noexcept;

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

    void (*log_callback)(const char *text) noexcept;
    INDEX indices[static_cast<size_t>(INDEX::TYPE::MAX_TYPES)];
    PIPE  caches [static_cast<size_t>(CACHE::MAX_CACHES)];
    std::vector<jack_type> descriptors[1024];
    std::vector<uint8_t> cache;
    sigset_t sigset_all;
    sigset_t sigset_none;
    sigset_t sigset_orig;
};

SOCKETS::SOCKETS() noexcept : log_callback(nullptr), indices{}, caches{} {
}

SOCKETS::~SOCKETS() {
    for (size_t i=0; i<std::size(indices); ++i) {
        INDEX &index = indices[i];

        if (index.type == INDEX::TYPE::NONE) {
            continue;
        }

        log(
            "%s\n", "destroying instance without having it deinitialized first"
        );
    }
}

void SOCKETS::clear() noexcept {
    for (size_t i=0; i<std::size(caches); ++i) {
        destroy(caches[i]);
    }

    for (size_t i=0; i<std::size(indices); ++i) {
        INDEX &index = indices[i];

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
}

bool SOCKETS::init() noexcept {
    static constexpr const size_t max_key_hash = 1024;

    for (size_t i=0; i<std::size(indices); ++i) {
        INDEX &index = indices[i];

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

    for (size_t i=0; i<std::size(indices); ++i) {
        INDEX &index = indices[i];
        index.type = static_cast<INDEX::TYPE>(i);

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

    for (size_t i=0; i<std::size(caches); ++i) {
        PIPE &pipe = caches[i];

        switch (static_cast<CACHE>(i)) {
            case CACHE::GENERIC_INT: {
                pipe.type = PIPE::TYPE::INT;
                break;
            }
            case CACHE::MAX_CACHES: die();
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

    for (size_t key_hash=0; key_hash<std::size(descriptors); ++key_hash) {
        while (!descriptors[key_hash].empty()) {
            int descriptor = descriptors[key_hash].back().descriptor;

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

    jack_type &epoll_jack = get_epoll_jack();
    int epoll_descriptor = epoll_jack.descriptor;
    rem_flag(epoll_descriptor, FLAG::FUSED);

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
    const jack_type *epoll_jack = find_epoll_jack();
    return epoll_jack ? has_flag(*epoll_jack, FLAG::TIMEOUT) : false;
}

bool SOCKETS::connect(const char *host, const char *port, int group) noexcept {
    int descriptor = connect(host, port, group, AF_UNSPEC, 0);

    return descriptor != NO_DESCRIPTOR;
}

void SOCKETS::disconnect(int descriptor) noexcept {
    terminate(descriptor);
}

bool SOCKETS::swap_incoming(
    int descriptor, std::vector<uint8_t> &bytes
) noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (jack && jack->incoming) {
        if (!bytes.empty()) {
            set_flag(descriptor, FLAG::INCOMING);
        }

        jack->incoming->swap(bytes);

        return true;
    }

    return false;
}

bool SOCKETS::swap_outgoing(
    int descriptor, std::vector<uint8_t> &bytes
) noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (jack && jack->outgoing) {
        jack->outgoing->swap(bytes);
        set_flag(descriptor, FLAG::WRITE);

        return true;
    }

    return false;
}

bool SOCKETS::append_outgoing(
    int descriptor, const uint8_t *buffer, size_t size
) noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (jack && jack->outgoing) {
        if (!buffer) die();

        if (size) {
            jack->outgoing->insert(
                jack->outgoing->end(), buffer, buffer + size
            );

            set_flag(descriptor, FLAG::WRITE);
        }

        return true;
    }

    return false;
}

bool SOCKETS::serve(int timeout) noexcept {
    if (find_jack(FLAG::NEW_CONNECTION)) {
        // We postpone serving any descriptors until the application has
        // acknowledged all the new incoming connections.

        return true;
    }

    PIPE &cached_descriptors = get_cache(CACHE::GENERIC_INT);

    for (size_t i=0; i<size_t(FLAG::MAX_FLAGS); ++i) {
        FLAG flag = static_cast<FLAG>(i);

        switch (flag) {
            case FLAG::RECONNECT:
            case FLAG::LISTENER:
            case FLAG::INCOMING:
            case FLAG::NEW_CONNECTION:
            case FLAG::DISCONNECT:
            case FLAG::CONNECTING:
            case FLAG::MAY_SHUTDOWN:
            case FLAG::FUSED: {
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
            *flagged_descriptors, cached_descriptors
        );

        if (error != ERROR::NONE) {
            return false; // TODO: implement better out-of-memory error handling
        }

        for (size_t j=0, sz=cached_descriptors.size; j<sz; ++j) {
            int d = ((int *) cached_descriptors.data)[j];
            const jack_type *jack = find_jack(d);

            if (jack == nullptr) continue;

            rem_flag(d, flag);

            switch (flag) {
                case FLAG::EPOLL: {
                    if (handle_epoll(d, timeout)) continue;
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

                    if (handle_close(d)) {
                        continue;
                    }

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

                    if (handle_accept(d)) continue;
                    break;
                }
                case FLAG::WRITE: {
                    if (jack->bitset.frozen) {
                        set_flag(d, flag);
                        continue;
                    }

                    if (handle_write(d)) continue;
                    break;
                }
                case FLAG::READ: {
                    if (jack->bitset.frozen) {
                        set_flag(d, flag);
                        continue;
                    }

                    if (handle_read(d)) continue;
                    break;
                }
                case FLAG::TIMEOUT: {
                    // This temporary flag is removed on each service cycle.
                    // For that reason it should be handled before others.
                    continue;
                }
                default: {
                    log(
                        "Flag %lu of descriptor %d was not handled.", i, d
                    );

                    break;
                }
            }

            return false;
        }
    }

    return true;
}

const char *SOCKETS::read(int descriptor) noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (!jack || jack->incoming == nullptr || jack->incoming->empty()) {
        return "";
    }

    if (cache.size() < jack->incoming->size() + 1) {
        cache.resize(jack->incoming->size() + 1);
    }

    std::memcpy(
        cache.data(), jack->incoming->data(), jack->incoming->size()
    );

    cache[jack->incoming->size()] = '\0';

    jack->incoming->clear();

    return (const char *) cache.data();
}

void SOCKETS::write(int descriptor, const char *text) noexcept {
    const jack_type *jack = find_jack(descriptor);

    if (!jack || jack->outgoing == nullptr) {
        return;
    }

    jack->outgoing->insert(
        jack->outgoing->end(), text, text + std::strlen(text)
    );

    set_flag(descriptor, FLAG::WRITE);
}

void SOCKETS::writef(int descriptor, const char *fmt, ...) noexcept {
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

        return;
    }

    const jack_type *jack = find_jack(descriptor);

    if (!jack || jack->outgoing == nullptr) {
        return;
    }

    if (size_t(retval) < sizeof(stackbuf)) {
        jack->outgoing->insert(
            jack->outgoing->end(), stackbuf, stackbuf + retval
        );

        set_flag(descriptor, FLAG::WRITE);

        return;
    }

    size_t heapbuf_sz = size_t(retval) + 1;
    char *heapbuf = new (std::nothrow) char [heapbuf_sz];

    if (heapbuf == nullptr) {
        log(
            "%s: out of memory when formatting '%s' (%s:%d).",
            __FUNCTION__, fmt, __FILE__, __LINE__
        );

        return;
    }

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
        jack->outgoing->insert(
            jack->outgoing->end(), heapbuf, heapbuf + retval
        );

        set_flag(descriptor, FLAG::WRITE);
    }
    else {
        log(
            "%s: unexpected program flow (%s:%d).",
            __FUNCTION__, __FILE__, __LINE__
        );
    }

    delete [] heapbuf;
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

bool SOCKETS::handle_close(int descriptor) noexcept {
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

        return true;
    }

    if (!close_and_deinit(descriptor)) {
        pop(descriptor);
        return false;
    }

    return true;
}

bool SOCKETS::handle_epoll(int epoll_descriptor, int timeout) noexcept {
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

        if (!has_flag(epoll_descriptor, FLAG::FUSED)) {
            set_flag(epoll_descriptor, FLAG::FUSED);

            return true;
        }

        log("Cannot serve sockets if there are unhandled events.\n");

        return false;
    }

    rem_flag(epoll_descriptor, FLAG::FUSED);

    jack_type &jack = get_jack(epoll_descriptor);
    epoll_event *events = &(jack.events[1]);

    int pending = epoll_pwait(
        epoll_descriptor, events, EPOLL_MAX_EVENTS, timeout, &sigset_none
    );

    if (pending == -1) {
        int code = errno;

        if (code == EINTR) return true;

        log(
            "epoll_pwait: %s (%s:%d)", strerror(code), __FILE__, __LINE__
        );

        return false;
    }
    else if (pending < 0) {
        log(
            "epoll_pwait: unexpected return value %d (%s:%d)", pending,
            __FILE__, __LINE__
        );

        return false;
    }
    else if (pending == 0) {
        set_flag(epoll_descriptor, FLAG::TIMEOUT);
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

    return true;
}

bool SOCKETS::handle_read(int descriptor) noexcept {
    jack_type &jack = get_jack(descriptor);

    if (!jack.incoming->empty()) {
        set_flag(descriptor, FLAG::READ);
        set_flag(descriptor, FLAG::INCOMING);

        return true;
    }

    if (cache.size() < 1024) {
        cache.resize(1024);
    }

    for (size_t total_count = 0;;) {
        ssize_t count;
        char *buf = (char *) cache.data();
        const size_t buf_sz = cache.size();

        if ((count = ::read(descriptor, buf, buf_sz)) < 0) {
            if (count == -1) {
                int code = errno;

                if (code == EAGAIN || code == EWOULDBLOCK) {
                    if (total_count) {
                        rem_flag(descriptor, FLAG::READ);
                    }

                    return true;
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

        jack.incoming->insert(jack.incoming->end(), buf, buf+count);

        if (!total_count) {
            set_flag(descriptor, FLAG::READ);
            set_flag(descriptor, FLAG::INCOMING);
        }

        total_count += count;

        if (size_t(count) == buf_sz && cache.size() < MAX_CACHE_SIZE) {
            cache.resize(std::min(2 * cache.size(), MAX_CACHE_SIZE));
        }

        if (total_count >= MAX_CACHE_SIZE) {
            return true;
        }
    }

    rem_flag(descriptor, FLAG::MAY_SHUTDOWN);
    terminate(descriptor);

    return true;
}

bool SOCKETS::handle_write(int descriptor) noexcept {
    jack_type &jack = get_jack(descriptor);

    std::vector<uint8_t> *outgoing = jack.outgoing;

    if (outgoing->empty()) {
        return true;
    }

    const unsigned char *bytes = &(outgoing->at(0));
    size_t length = outgoing->size();

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
        outgoing->clear();
    }
    else if (istart > 0) {
        outgoing->erase(outgoing->begin(), outgoing->begin()+istart);

        if (try_again_later) {
            set_flag(descriptor, FLAG::WRITE);
        }
    }

    if (try_again_later) {
        return modify_epoll(descriptor, EPOLLIN|EPOLLET|EPOLLRDHUP);
    }

    return modify_epoll(descriptor, EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP);
}

bool SOCKETS::handle_accept(int descriptor) noexcept {
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

                    return true;
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

                    return true;
                }
                case EINTR: {
                    set_flag(descriptor, FLAG::ACCEPT);

                    return true;
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

        return false;
    }

    ERROR error{push(make_jack(client_descriptor, descriptor, 0))};

    if (error != ERROR::NONE) {
        if (!close_and_deinit(client_descriptor)) {
            pop(client_descriptor);
        }

        return false;
    }

    jack_type &client_jack = get_jack(client_descriptor);

    client_jack.incoming = new (std::nothrow) std::vector<uint8_t>;
    client_jack.outgoing = new (std::nothrow) std::vector<uint8_t>;

    if (!client_jack.incoming
    ||  !client_jack.outgoing) {
        out_of_memory();

        if (!close_and_deinit(client_descriptor)) {
            pop(client_descriptor);
        }

        return false;
    }

    int retval = getnameinfo(
        &in_addr, in_len,
        client_jack.host, socklen_t(std::size(client_jack.host)),
        client_jack.port, socklen_t(std::size(client_jack.port)),
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

    std::vector<uint8_t> *incoming{new (std::nothrow) std::vector<uint8_t>};
    std::vector<uint8_t> *outgoing{new (std::nothrow) std::vector<uint8_t>};

    if (!incoming || !outgoing) {
        out_of_memory();

        if (incoming) delete incoming;
        if (outgoing) delete outgoing;

        return NO_DESCRIPTOR;
    }

    int descriptor{
        open_and_init(
            host, port, ai_family, ai_flags, blacklist, file, line
        )
    };

    if (descriptor == NO_DESCRIPTOR) {
        delete incoming;
        delete outgoing;

        return NO_DESCRIPTOR;
    }

    if (set_group(descriptor, group) != ERROR::NONE) {
        if (!close_and_deinit(descriptor)) {
            pop(descriptor);
        }

        delete incoming;
        delete outgoing;

        return NO_DESCRIPTOR;
    }

    jack_type &jack = get_jack(descriptor);

    jack.incoming = incoming;
    jack.outgoing = outgoing;

    if (jack.host[0] == '\0') {
        strncpy(jack.host, host, std::size(jack.host)-1);
        jack.host[std::size(jack.host)-1] = '\0';
    }

    if (jack.port[0] == '\0') {
        strncpy(jack.port, port, std::size(jack.port)-1);
        jack.port[std::size(jack.port)-1] = '\0';
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
        for (size_t key=0; key<std::size(descriptors); ++key) {
            for (size_t i=0, sz=descriptors[key].size(); i<sz; ++i) {
                const jack_type &rec = descriptors[key][i];

                if (rec.parent != descriptor) {
                    continue;
                }

                terminate(rec.descriptor, file, line);
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
                jack->host, socklen_t(std::size(jack->host)),
                jack->port, socklen_t(std::size(jack->port)),
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
            std::vector<int> to_be_closed;

            for (size_t key=0; key<std::size(descriptors); ++key) {
                for (size_t i=0, sz=descriptors[key].size(); i<sz; ++i) {
                    const jack_type &rec = descriptors[key][i];

                    if (rec.parent != close_children_of) {
                        continue;
                    }

                    to_be_closed.emplace_back(rec.descriptor);
                }
            }

            for (int d : to_be_closed) {
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

    size_t descriptor_key{
        descriptor % std::size(descriptors)
    };

    descriptors[descriptor_key].emplace_back(jack);

    {
        // If the newly pushed jack has not its group set to zero at first, then
        // set_group would falsely reduce the group size.

        descriptors[descriptor_key].back().group = 0;
    }

    return set_group(descriptor, group);
}

SOCKETS::jack_type SOCKETS::pop(int descriptor) noexcept {
    if (descriptor == NO_DESCRIPTOR) {
        return make_jack(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
    }

    size_t key_hash = descriptor % std::size(descriptors);

    for (size_t i=0, sz=descriptors[key_hash].size(); i<sz; ++i) {
        const jack_type &rec = descriptors[key_hash][i];

        if (rec.descriptor != descriptor) continue;

        int parent_descriptor = rec.parent;

        // First, let's free the flags.
        for (size_t j=0, fsz=std::size(rec.flags); j<fsz; ++j) {
            rem_flag(rec.descriptor, static_cast<FLAG>(j));
        }

        // Then, we free the dynamically allocated memory.
        if (rec.events) {
            delete [] rec.events;
        }

        if (rec.incoming) delete rec.incoming;
        if (rec.outgoing) delete rec.outgoing;

        if (rec.blacklist) {
            freeaddrinfo(rec.blacklist);
        }

        rem_group(descriptor);

        // Finally, we remove the jack.
        descriptors[key_hash][i] = descriptors[key_hash].back();
        descriptors[key_hash].pop_back();

        return make_jack(descriptor, parent_descriptor, 0);
    }

    return make_jack(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
}

const SOCKETS::jack_type *SOCKETS::find_jack(
    int descriptor
) const noexcept {
    size_t key = descriptor % std::size(descriptors);

    for (size_t i=0, sz=descriptors[key].size(); i<sz; ++i) {
        if (descriptors[key].at(i).descriptor != descriptor) continue;

        return &(descriptors[key].at(i));
    }

    return nullptr;
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

SOCKETS::PIPE &SOCKETS::get_cache(CACHE cache) noexcept {
    size_t index = size_t(cache);

    if (index >= std::size(caches)) die();

    return caches[index];
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

    if (index >= std::size(rec.flags)) {
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

    jack_type *rec = find_jack(descriptor);

    if (!rec) return;

    if (index >= std::size(rec->flags)) {
        die();
    }

    uint32_t pos = rec->flags[index];

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

    rec->flags[index] = std::numeric_limits<uint32_t>::max();
}

bool SOCKETS::has_flag(const jack_type &rec, FLAG flag) const noexcept {
    size_t index = static_cast<size_t>(flag);

    if (index >= std::size(rec.flags)) {
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
                case PIPE::TYPE::JACK_PTR: {
                    const void **vd = (const void **) value_pipe.data;

                    if (vd[i] != value.as_ptr) {
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

        if (!key_data) {
            die();
        }

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
                case PIPE::TYPE::JACK_PTR: {
                    const void **vd = (const void **) val_pipe.data;

                    if (vd[i] != value.as_ptr) {
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
        case PIPE::TYPE::JACK_PTR: {
            ((void **) pipe.data)[index] = value.as_ptr;
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
        case PIPE::TYPE::JACK_PTR: {
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
        case PIPE::TYPE::NONE: return die();
    }

    return ERROR::NONE;
}

SOCKETS::ERROR SOCKETS::copy(const PIPE &src, PIPE &dst) noexcept {
    if (&src == &dst) {
        return ERROR::NONE;
    }

    if (src.type != dst.type || dst.type == PIPE::TYPE::NONE) {
        return die();
    }

    if (src.size > dst.capacity) {
        ERROR error = reserve(dst, src.size);

        if (error != ERROR::NONE) {
            return error;
        }
    }

    dst.size = src.size;

    if (src.data == nullptr) {
        return ERROR::NONE;
    }

    switch (dst.type) {
        case PIPE::TYPE::UINT8: {
            std::memcpy(dst.data, src.data, dst.size * sizeof(uint8_t));
            break;
        }
        case PIPE::TYPE::UINT64: {
            std::memcpy(dst.data, src.data, dst.size * sizeof(uint64_t));
            break;
        }
        case PIPE::TYPE::INT: {
            std::memcpy(dst.data, src.data, dst.size * sizeof(int));
            break;
        }
        case PIPE::TYPE::JACK_PTR: {
            std::memcpy(dst.data, src.data, dst.size * sizeof(void*));
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
        case PIPE::TYPE::JACK_PTR: {
            void **data = (void **) pipe.data;
            data[index] = data[pipe.size-1];
            break;
        }
        case PIPE::TYPE::NONE: die(); return;
    }

    --pipe.size;
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
        case PIPE::TYPE::JACK_PTR: {
            if (pipe.data) delete [] ((void **) pipe.data);

            break;
        }
        case PIPE::TYPE::NONE: die(); break;
    }

    pipe.data = nullptr;
    pipe.capacity = 0;
    pipe.size = 0;
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
        .incoming   = nullptr,
        .outgoing   = nullptr,
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

    for (size_t i = 0; i != std::size(jack.flags); ++i) {
        jack.flags[i] = std::numeric_limits<uint32_t>::max();
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

#endif
