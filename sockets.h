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

#include <array>
#include <vector>
#include <string>
#include <unordered_map>
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
    static constexpr const int NO_DESCRIPTOR = -1;

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

    bool is_listener(int descriptor) const noexcept;
    bool is_frozen(int descriptor) const noexcept;
    int get_group(int descriptor) const noexcept;
    size_t get_group_size(int group) const noexcept;
    int get_listener(int descriptor) const noexcept;
    const char *get_host(int descriptor) const noexcept;
    const char *get_port(int descriptor) const noexcept;

    void freeze(int descriptor) noexcept;
    void unfreeze(int descriptor) noexcept;
    void disconnect(int descriptor) noexcept;

    private:
    static constexpr const int EPOLL_MAX_EVENTS  = 64;
    static constexpr const size_t MAX_CACHE_SIZE = 1024 * 1024;

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
        FROZEN         = 10,
        MAY_SHUTDOWN   = 11,
        LISTENER       = 12,
        CONNECTING     = 13,
        FUSED          = 14,
        // Do not change the order of the flags below this line.
        EPOLL          = 15,
        MAX_FLAGS      = 16
    };

    struct record_type {
        std::array<uint32_t, static_cast<size_t>(FLAG::MAX_FLAGS)> flags;
        epoll_event *events;
        std::vector<uint8_t> *incoming;
        std::vector<uint8_t> *outgoing;
        std::array<char, NI_MAXHOST> host;
        std::array<char, NI_MAXSERV> port;
        int descriptor;
        int parent;
        int group;
        int ai_family;
        int ai_flags;
        struct addrinfo *blacklist;
    };

    struct flag_type {
        int descriptor;
        FLAG index;
    };

    inline static constexpr struct record_type make_record(
        int descriptor, int parent, int group
    ) noexcept;

    inline static constexpr struct EVENT make_event(
        int descriptor, EVENT::TYPE type, bool valid =true
    ) noexcept;

    inline static constexpr struct flag_type make_flag(
        int descriptor =NO_DESCRIPTOR, FLAG index =FLAG::NONE
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

    void push(record_type record) noexcept;
    record_type pop(int descriptor) noexcept;

    const record_type *find_record(int descriptor) const noexcept;
    record_type *find_record(int descriptor) noexcept;
    const record_type *find_epoll_record() const noexcept;
    record_type *find_epoll_record() noexcept;
    const record_type &get_record(int descriptor) const noexcept;
    record_type &get_record(int descriptor) noexcept;
    const record_type &get_epoll_record() const noexcept;
    record_type &get_epoll_record() noexcept;

    bool set_group(int descriptor, int group) noexcept;
    bool rem_group(int descriptor) noexcept;
    bool set_flag(int descriptor, FLAG flag, bool value =true) noexcept;
    bool rem_flag(int descriptor, FLAG flag) noexcept;
    bool has_flag(const record_type &rec, const FLAG flag) const noexcept;
    bool has_flag(int descriptor, const FLAG flag) const noexcept;

    void log(
        const char *fmt, ...
    ) const noexcept __attribute__((format(printf, 2, 3)));
    void bug(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;
    void die(
        const char *file =__builtin_FILE(), int line =__builtin_LINE()
    ) const noexcept;

    void (*log_callback)(const char *text) noexcept;
    std::unordered_map<int, size_t> groups;
    std::array<std::vector<record_type>, 1024> descriptors;
    std::array<
        std::vector<flag_type>,
        static_cast<size_t>(FLAG::MAX_FLAGS)
    > flags;
    std::vector<uint8_t> cache;
    sigset_t sigset_all;
    sigset_t sigset_none;
    sigset_t sigset_orig;
};

SOCKETS::SOCKETS() noexcept : log_callback(nullptr) {
}

SOCKETS::~SOCKETS() {
    if (find_epoll_record()) {
        log(
            "%s\n", "destroying instance without having it deinitialized first"
        );
    }
}

bool SOCKETS::init() noexcept {
    if (find_epoll_record()) {
        log("%s: already initialized", __FUNCTION__);

        return false;
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

    int epoll_descriptor = create_epoll();

    if (epoll_descriptor == NO_DESCRIPTOR) {
        log(
            "%s: %s (%s:%d)", __FUNCTION__,
            "epoll record could not be created", __FILE__, __LINE__
        );

        return false;
    }

    return true;
}

bool SOCKETS::deinit() noexcept {
    if (!find_epoll_record()) {
        log("%s: already deinitialized", __FUNCTION__);

        return false;
    }

    bool success = true;

    for (size_t key_hash=0; key_hash<descriptors.size(); ++key_hash) {
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

    record_type &epoll_record = get_epoll_record();
    int epoll_descriptor = epoll_record.descriptor;
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
    static constexpr const size_t flg_connect_index{
        static_cast<size_t>(FLAG::NEW_CONNECTION)
    };

    if (!flags[flg_connect_index].empty()) {
        int descriptor = flags[flg_connect_index].back().descriptor;
        rem_flag(descriptor, FLAG::NEW_CONNECTION);
        return descriptor;
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_disconnection() noexcept {
    static constexpr const size_t flg_connect_index{
        static_cast<size_t>(FLAG::NEW_CONNECTION)
    };

    static constexpr const size_t flg_disconnect_index{
        static_cast<size_t>(FLAG::DISCONNECT)
    };

    if (!flags[flg_connect_index].empty()) {
        // We postpone reporting any disconnections until the application
        // has acknowledged all the new incoming connections. This prevents
        // us from reporting a disconnection event before its respective
        // connection event is reported.

        return NO_DESCRIPTOR;
    }

    if (!flags[flg_disconnect_index].empty()) {
        int descriptor = flags[flg_disconnect_index].back().descriptor;
        rem_flag(descriptor, FLAG::DISCONNECT);
        set_flag(descriptor, FLAG::CLOSE);
        return descriptor;
    }

    return NO_DESCRIPTOR;
}

int SOCKETS::next_incoming() noexcept {
    static constexpr const size_t flg_incoming_index{
        static_cast<size_t>(FLAG::INCOMING)
    };

    if (!flags[flg_incoming_index].empty()) {
        int descriptor = flags[flg_incoming_index].back().descriptor;
        rem_flag(descriptor, FLAG::INCOMING);
        return descriptor;
    }

    return NO_DESCRIPTOR;
}

bool SOCKETS::is_listener(int descriptor) const noexcept {
    return has_flag(descriptor, FLAG::LISTENER);
}

int SOCKETS::get_group(int descriptor) const noexcept {
    const record_type *record = find_record(descriptor);
    return record ? record->group : 0;
}

size_t SOCKETS::get_group_size(int group) const noexcept {
    if (groups.count(group)) return groups.at(group);
    return 0;
}

int SOCKETS::get_listener(int descriptor) const noexcept {
    const record_type *record = find_record(descriptor);
    return record ? record->parent : NO_DESCRIPTOR;
}

const char *SOCKETS::get_host(int descriptor) const noexcept {
    const record_type *record = find_record(descriptor);
    return record ? record->host.data() : "";
}

const char *SOCKETS::get_port(int descriptor) const noexcept {
    const record_type *record = find_record(descriptor);
    return record ? record->port.data() : "";
}

void SOCKETS::freeze(int descriptor) noexcept {
    set_flag(descriptor, FLAG::FROZEN);
}

void SOCKETS::unfreeze(int descriptor) noexcept {
    if (!has_flag(descriptor, FLAG::DISCONNECT)
    &&  !has_flag(descriptor, FLAG::CLOSE)) {
        rem_flag(descriptor, FLAG::FROZEN);
    }
}

bool SOCKETS::is_frozen(int descriptor) const noexcept {
    return has_flag(descriptor, FLAG::FROZEN);
}

bool SOCKETS::idle() const noexcept {
    const record_type *epoll_record = find_epoll_record();
    return epoll_record ? has_flag(*epoll_record, FLAG::TIMEOUT) : false;
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
    const record_type *record = find_record(descriptor);

    if (record && record->incoming) {
        if (!bytes.empty()) {
            set_flag(descriptor, FLAG::INCOMING);
        }

        record->incoming->swap(bytes);

        return true;
    }

    return false;
}

bool SOCKETS::swap_outgoing(
    int descriptor, std::vector<uint8_t> &bytes
) noexcept {
    const record_type *record = find_record(descriptor);

    if (record && record->outgoing) {
        record->outgoing->swap(bytes);
        set_flag(descriptor, FLAG::WRITE);

        return true;
    }

    return false;
}

bool SOCKETS::append_outgoing(
    int descriptor, const uint8_t *buffer, size_t size
) noexcept {
    const record_type *record = find_record(descriptor);

    if (record && record->outgoing) {
        if (!buffer) die();

        if (size) {
            record->outgoing->insert(
                record->outgoing->end(), buffer, buffer + size
            );

            set_flag(descriptor, FLAG::WRITE);
        }

        return true;
    }

    return false;
}

bool SOCKETS::serve(int timeout) noexcept {
    static constexpr const size_t flg_connect_index{
        static_cast<size_t>(FLAG::NEW_CONNECTION)
    };

    static constexpr const size_t flg_disconnect_index{
        static_cast<size_t>(FLAG::DISCONNECT)
    };

    if (!flags[flg_connect_index].empty()) {
        // We postpone serving any descriptors until the application has
        // acknowledged all the new incoming connections.

        return true;
    }

    std::vector<int> recbuf;

    for (size_t i=0; i<flags.size(); ++i) {
        FLAG flag = static_cast<FLAG>(i);

        switch (flag) {
            case FLAG::RECONNECT:
            case FLAG::LISTENER:
            case FLAG::FROZEN:
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

        recbuf.reserve(flags[i].size());

        for (size_t j=0, sz=flags[i].size(); j<sz; ++j) {
            int d = flags[i][j].descriptor;
            recbuf.emplace_back(d);
        }

        for (size_t j=0, sz=recbuf.size(); j<sz; ++j) {
            int d = recbuf[j];
            const record_type *record = find_record(d);

            if (record == nullptr) continue;

            rem_flag(d, flag);

            switch (flag) {
                case FLAG::EPOLL: {
                    if (handle_epoll(d, timeout)) continue;
                    break;
                }
                case FLAG::CLOSE: {
                    if (has_flag(d, FLAG::READ)
                    && !has_flag(d, FLAG::FROZEN)) {
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
                    if (!flags[flg_disconnect_index].empty()
                    || has_flag(d, FLAG::FROZEN)) {
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
                    if (has_flag(d, FLAG::FROZEN)) {
                        set_flag(d, flag);
                        continue;
                    }

                    if (handle_write(d)) continue;
                    break;
                }
                case FLAG::READ: {
                    if (has_flag(d, FLAG::FROZEN)) {
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

        recbuf.clear();
    }

    return true;
}

const char *SOCKETS::read(int descriptor) noexcept {
    const record_type *record = find_record(descriptor);

    if (!record || record->incoming == nullptr || record->incoming->empty()) {
        return "";
    }

    if (cache.size() < record->incoming->size() + 1) {
        cache.resize(record->incoming->size() + 1);
    }

    std::memcpy(
        cache.data(), record->incoming->data(), record->incoming->size()
    );

    cache[record->incoming->size()] = '\0';

    record->incoming->clear();

    return (const char *) cache.data();
}

void SOCKETS::write(int descriptor, const char *text) noexcept {
    const record_type *record = find_record(descriptor);

    if (!record || record->outgoing == nullptr) {
        return;
    }

    record->outgoing->insert(
        record->outgoing->end(), text, text + std::strlen(text)
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

    const record_type *record = find_record(descriptor);

    if (!record || record->outgoing == nullptr) {
        return;
    }

    if (size_t(retval) < sizeof(stackbuf)) {
        record->outgoing->insert(
            record->outgoing->end(), stackbuf, stackbuf + retval
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
        record->outgoing->insert(
            record->outgoing->end(), heapbuf, heapbuf + retval
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
            }

            break;
        }

        if (bufptr == stackbuf) {
            bufsz = cx + 1;
            bufptr = new (std::nothrow) char[bufsz];

            if (!bufptr) {
                const char *OOM = "out of memory";

                if (log_callback) {
                    log_callback(OOM);
                }
                else {
                    ::write(STDERR_FILENO, OOM, strlen(OOM));
                }
            }
        }
        else {
            if (log_callback) {
                log_callback(bufptr);
            }
            else {
                ::write(STDERR_FILENO, bufptr, strlen(bufptr));
            }

            break;
        }
    }

    if (bufptr && bufptr != stackbuf) delete [] bufptr;
}

void SOCKETS::bug(const char *file, int line) const noexcept {
    log("Forbidden condition met in %s on line %d.", file, line);
}

void SOCKETS::die(const char *file, int line) const noexcept {
    bug(file, line);
    fflush(nullptr);
    std::raise(SIGSEGV);
}

bool SOCKETS::handle_close(int descriptor) noexcept {
    if (has_flag(descriptor, FLAG::RECONNECT)) {
        record_type &rec = get_record(descriptor);

        int new_descriptor = connect(
            get_host(descriptor), get_port(descriptor),
            get_group(descriptor), rec.ai_family, rec.ai_flags, rec.blacklist
        );

        if (new_descriptor == NO_DESCRIPTOR) {
            rem_flag(descriptor, FLAG::RECONNECT);
        }
        else {
            record_type &new_rec = get_record(new_descriptor);

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
    static constexpr const std::array blockers{
        static_cast<size_t>(FLAG::NEW_CONNECTION),
        static_cast<size_t>(FLAG::DISCONNECT),
        static_cast<size_t>(FLAG::INCOMING)
    };

    set_flag(epoll_descriptor, FLAG::EPOLL);

    for (size_t flag_index : blockers) {
        if (!flags[flag_index].empty()) {
            if (!has_flag(epoll_descriptor, FLAG::FUSED)) {
                set_flag(epoll_descriptor, FLAG::FUSED);

                return true;
            }

            log("Cannot serve sockets if there are unhandled events.\n");

            return false;
        }
    }

    rem_flag(epoll_descriptor, FLAG::FUSED);

    record_type &record = get_record(epoll_descriptor);
    epoll_event *events = &(record.events[1]);

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
    record_type &record = get_record(descriptor);

    if (!record.incoming->empty()) {
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

        record.incoming->insert(record.incoming->end(), buf, buf+count);

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
    record_type &record = get_record(descriptor);

    std::vector<uint8_t> *outgoing = record.outgoing;

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
    record_type &epoll_record = get_epoll_record();
    int epoll_descriptor = epoll_record.descriptor;

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

    push(make_record(client_descriptor, descriptor, 0));

    record_type &client_record = get_record(client_descriptor);

    client_record.incoming = new (std::nothrow) std::vector<uint8_t>;
    client_record.outgoing = new (std::nothrow) std::vector<uint8_t>;

    if (!client_record.incoming
    ||  !client_record.outgoing) {
        log(
            "new: out of memory (%s:%d)", __FILE__, __LINE__
        );

        if (!close_and_deinit(client_descriptor)) {
            pop(client_descriptor);
        }

        return false;
    }

    int retval = getnameinfo(
        &in_addr, in_len,
        client_record.host.data(), socklen_t(client_record.host.size()),
        client_record.port.data(), socklen_t(client_record.port.size()),
        NI_NUMERICHOST|NI_NUMERICSERV
    );

    if (retval != 0) {
        log(
            "getnameinfo: %s (%s:%d)", gai_strerror(retval),
            __FILE__, __LINE__
        );

        client_record.host[0] = '\0';
        client_record.port[0] = '\0';
    }

    epoll_event *event = &(epoll_record.events[0]);

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
    int epoll_descriptor = get_epoll_record().descriptor;

    std::vector<uint8_t> *incoming{new (std::nothrow) std::vector<uint8_t>};
    std::vector<uint8_t> *outgoing{new (std::nothrow) std::vector<uint8_t>};

    if (!incoming || !outgoing) {
        log(
            "new: out of memory (%s:%d)", __FILE__, __LINE__
        );

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

    set_group(descriptor, group);

    record_type &record = get_record(descriptor);

    record.incoming = incoming;
    record.outgoing = outgoing;

    if (record.host.front() == '\0') {
        strncpy(record.host.data(), host, record.host.size()-1);
        record.host.back() = '\0';
    }

    if (record.port.front() == '\0') {
        strncpy(record.port.data(), port, record.port.size()-1);
        record.port.back() = '\0';
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
        for (size_t key=0; key<descriptors.size(); ++key) {
            for (size_t i=0, sz=descriptors[key].size(); i<sz; ++i) {
                const record_type &rec = descriptors[key][i];

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
    int epoll_descriptor = get_epoll_record().descriptor;
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

    record_type *record = find_record(descriptor);

    if (record) {
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
                record->host.data(), socklen_t(record->host.size()),
                record->port.data(), socklen_t(record->port.size()),
                NI_NUMERICHOST|NI_NUMERICSERV
            );

            if (retval != 0) {
                log(
                    "getnameinfo: %s (%s:%d)", gai_strerror(retval),
                    __FILE__, __LINE__
                );

                record->host[0] = '\0';
                record->port[0] = '\0';
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

    push(make_record(epoll_descriptor, NO_DESCRIPTOR, 0));

    record_type &record = get_record(epoll_descriptor);

    record.events = new (std::nothrow) epoll_event [1+EPOLL_MAX_EVENTS];

    if (record.events == nullptr) {
        log(
            "new: out of memory (%s:%d)", __FILE__, __LINE__
        );

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

    record_type &record = get_record(epoll_descriptor);
    epoll_event *event = &(record.events[0]);

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

        push(make_record(descriptor, NO_DESCRIPTOR, 0));

        record_type &rec = get_record(descriptor);

        rec.ai_family = ai_family;
        rec.ai_flags  = ai_flags;

        if (accept_incoming_connections) {
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
                            set_flag(descriptor, FLAG::CONNECTING);

                            if (info == next) {
                                info = next->ai_next;
                            }
                            else {
                                prev->ai_next = next->ai_next;
                            }

                            next->ai_next = rec.blacklist;
                            rec.blacklist = next;
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

        record_type record{pop(descriptor)};
        bool found = record.descriptor != NO_DESCRIPTOR;

        int close_children_of = NO_DESCRIPTOR;

        if (record.parent == NO_DESCRIPTOR) {
            close_children_of = descriptor;
        }

        if (!found) {
            log(
                "descriptor %d closed but record not found (%s:%d)",
                descriptor, __FILE__, __LINE__
            );
        }

        if (close_children_of != NO_DESCRIPTOR) {
            std::vector<int> to_be_closed;

            for (size_t key=0; key<descriptors.size(); ++key) {
                for (size_t i=0, sz=descriptors[key].size(); i<sz; ++i) {
                    const record_type &rec = descriptors[key][i];

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
                    record = pop(d);

                    if (record.descriptor == NO_DESCRIPTOR) {
                        log(
                            "descriptor %d closed but record not found "
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

void SOCKETS::push(record_type record) noexcept {
    if (record.descriptor == NO_DESCRIPTOR) {
        return;
    }

    int descriptor = record.descriptor;
    int group = record.group;

    size_t descriptor_key{
        descriptor % descriptors.size()
    };

    descriptors[descriptor_key].emplace_back(record);

    set_group(descriptor, group);
}

SOCKETS::record_type SOCKETS::pop(int descriptor) noexcept {
    if (descriptor == NO_DESCRIPTOR) {
        return make_record(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
    }

    size_t key_hash = descriptor % descriptors.size();

    for (size_t i=0, sz=descriptors[key_hash].size(); i<sz; ++i) {
        const record_type &rec = descriptors[key_hash][i];

        if (rec.descriptor != descriptor) continue;

        int parent_descriptor = rec.parent;

        // First, let's free the flags.
        for (size_t j=0, fsz=flags.size(); j<fsz; ++j) {
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

        // Finally, we remove the record.
        descriptors[key_hash][i] = descriptors[key_hash].back();
        descriptors[key_hash].pop_back();

        return make_record(descriptor, parent_descriptor, 0);
    }

    return make_record(NO_DESCRIPTOR, NO_DESCRIPTOR, 0);
}

const SOCKETS::record_type *SOCKETS::find_record(
    int descriptor
) const noexcept {
    size_t key = descriptor % descriptors.size();

    for (size_t i=0, sz=descriptors.at(key).size(); i<sz; ++i) {
        if (descriptors.at(key).at(i).descriptor != descriptor) continue;

        return &(descriptors.at(key).at(i));
    }

    return nullptr;
}

SOCKETS::record_type *SOCKETS::find_record(int descriptor) noexcept {
    return const_cast<record_type *>(
        static_cast<const SOCKETS &>(*this).find_record(descriptor)
    );
}

const SOCKETS::record_type *SOCKETS::find_epoll_record() const noexcept {
    const record_type *epoll_record = nullptr;

    static constexpr const size_t flag_index{
        static_cast<size_t>(FLAG::EPOLL)
    };

    for (size_t i=0, sz=flags[flag_index].size(); i<sz; ++i) {
        int epoll_descriptor = flags[flag_index][i].descriptor;
        const record_type *rec = find_record(epoll_descriptor);

        if (rec) {
            epoll_record = rec;
            break;
        }
    }

    return epoll_record;
}

SOCKETS::record_type *SOCKETS::find_epoll_record() noexcept {
    return const_cast<record_type *>(
        static_cast<const SOCKETS &>(*this).find_epoll_record()
    );
}

const SOCKETS::record_type &SOCKETS::get_record(int descriptor) const noexcept {
    const record_type *rec = find_record(descriptor);

    if (!rec) die();

    return *rec;
}

SOCKETS::record_type &SOCKETS::get_record(int descriptor) noexcept {
    record_type *rec = find_record(descriptor);

    if (!rec) die();

    return *rec;
}

const SOCKETS::record_type &SOCKETS::get_epoll_record() const noexcept {
    const record_type *rec = find_epoll_record();

    if (!rec) die();

    return *rec;
}

SOCKETS::record_type &SOCKETS::get_epoll_record() noexcept {
    record_type *rec = find_epoll_record();

    if (!rec) die();

    return *rec;
}

bool SOCKETS::modify_epoll(int descriptor, uint32_t events) noexcept {
    record_type &epoll_record = get_epoll_record();
    int epoll_descriptor = epoll_record.descriptor;
    epoll_event *event = &(epoll_record.events[0]);

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

bool SOCKETS::set_group(int descriptor, int group) noexcept {
    record_type *rec = find_record(descriptor);

    if (!rec) return false;

    if (groups.count(rec->group)) {
        if (groups[rec->group]) {
            if (--groups[rec->group] == 0) {
                groups.erase(rec->group);
            }
        }
        else {
            log(
                "Forbidden condition met (%s:%d).", __FILE__, __LINE__
             );
        }
    }

    rec->group = group;

    if (group == 0) {
        // 0 stands for no group. We don't keep track of its size.
        return true;
    }

    groups[group]++;

    return true;
}

bool SOCKETS::rem_group(int descriptor) noexcept {
    return set_group(descriptor, 0);
}

bool SOCKETS::set_flag(int descriptor, FLAG flag, bool value) noexcept {
    if (value == false) {
        return rem_flag(descriptor, flag);
    }

    size_t index = static_cast<size_t>(flag);

    if (index > flags.size()) {
        return false;
    }

    record_type *rec = find_record(descriptor);

    if (!rec) return false;

    uint32_t pos = rec->flags[index];

    if (pos == std::numeric_limits<uint32_t>::max()) {
        if (flags[index].size() < std::numeric_limits<uint32_t>::max()) {
            rec->flags[index] = uint32_t(flags[index].size());
            flags[index].emplace_back(make_flag(descriptor, flag));
        }
        else {
            log(
                "flag buffer is full (%s:%d)", __FILE__, __LINE__
            );

            return false;
        }
    }

    return true;
}

bool SOCKETS::rem_flag(int descriptor, FLAG flag) noexcept {
    size_t index = static_cast<size_t>(flag);

    if (index > flags.size()) {
        return false;
    }

    record_type *rec = find_record(descriptor);

    if (!rec) return false;

    uint32_t pos = rec->flags[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        flags[index][pos] = flags[index].back();

        int other_descriptor = flags[index].back().descriptor;
        get_record(other_descriptor).flags[index] = pos;

        flags[index].pop_back();
        rec->flags[index] = std::numeric_limits<uint32_t>::max();
    }

    return true;
}

bool SOCKETS::has_flag(const record_type &rec, const FLAG flag) const noexcept {
    size_t index = static_cast<size_t>(flag);

    if (index >= rec.flags.size()) {
        return false;
    }

    uint32_t pos = rec.flags[index];

    if (pos != std::numeric_limits<uint32_t>::max()) {
        return true;
    }

    return false;
}

bool SOCKETS::has_flag(int descriptor, const FLAG flag) const noexcept {
    const record_type *rec = find_record(descriptor);
    return rec ? has_flag(*rec, flag) : false;
}

constexpr SOCKETS::record_type SOCKETS::make_record(
    int descriptor, int parent, int group
) noexcept {
#if __cplusplus <= 201703L
    __extension__
#endif
    record_type record{
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
        .blacklist  = nullptr
    };

    for (size_t i = 0; i != record.flags.size(); ++i) {
        record.flags[i] = std::numeric_limits<uint32_t>::max();
    }

    return record;
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

constexpr SOCKETS::flag_type SOCKETS::make_flag(
    int descriptor, FLAG index
) noexcept {
    return
#if __cplusplus <= 201703L
    __extension__
#endif
    flag_type{
        .descriptor = descriptor,
        .index = index
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
