// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <chrono>
#include <vector>

volatile sig_atomic_t
    sig_interruption{ 0 },
    sig_alarm       { 0 };

struct USER{
    size_t session_id;
    std::string name;
    std::string incoming;
    std::string outgoing;

    enum class STATE : uint8_t {
        NONE = 0,
        GREET, NAME, CHAT, EXIT
    } state;
    bool connected:1;
};

struct CHAT{
    const char *port;
    SOCKETS *sockets;
    std::map<size_t, USER> *users;
    size_t online;
    uint16_t pulse_per_second;
    bool valid:1;
};

static CHAT init_chat(int argc, char **argv);
static void signal_handler(int);
static void log(const char *str) noexcept;
static void log(const std::string &str);
static void log(SOCKETS::SESSION, const char *txt) noexcept;
static void main_loop(CHAT &);
static void handle_sockets(CHAT &);
static void handle_updates(CHAT &);
static void update_user(CHAT &, USER &);
static void interpret(CHAT &, USER &, std::string &command);
static void do_say(CHAT &, USER &, std::string &);

int main(int argc, char **argv) {
    std::map<size_t, USER> users;
    SOCKETS sockets;

    CHAT chat{ init_chat(argc, argv) };

    chat.users = &users;
    chat.sockets = &sockets;

    if (!chat.valid) {
        std::cerr << "Usage: " << argv[0] << " PORT\n";
        return EXIT_FAILURE;
    }

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGALRM, signal_handler);

    log(
        std::string("Initializing networking using SOCKETS v").append(
            SOCKETS::VERSION
        ).append(".")
    );

    sockets.set_logger(
        [](SOCKETS::SESSION session, const char *txt) noexcept {
            log(session, txt);
        }
    );

    if (!sockets.init()) {
        log("Failed to initialize sockets.");
        return EXIT_FAILURE;
    }

    SOCKETS::SESSION listener{
        sockets.listen(
            chat.port,
            AF_UNSPEC, // Accept connections from any address family.
            {
                SO_REUSEADDR // Allow instantaneous server restarting.
            }
        )
    };

    if (listener.valid) {
        log(
            std::string("Listening for TCP connections on ").append(
                sockets.get_host(listener.id)
            ).append(":").append(sockets.get_port(listener.id)).append(".")
        );

        sockets.set_memcap(8192);

        main_loop(chat);

        log("Shutting down the server.");
        sockets.disconnect(listener.id);
    }
    else {
        log(
            std::string("Failed to listen on port '").append(
                chat.port
            ).append("'").append(
                !listener.error ? (
                    std::string(".")
                ) : (
                    std::string(" (").append(
                        sockets.to_string(listener.error)
                    ).append(").")
                )
            )
        );
    };

    return sockets.deinit() ? EXIT_SUCCESS : EXIT_FAILURE;
}

static CHAT init_chat(int argc, char **argv) {
    CHAT chat{};

    if (argc >= 2) {
        chat.port = argv[1];
        chat.pulse_per_second = 4;
        chat.valid = true;
    }

    return chat;
}

static void main_loop(CHAT &chat) {
    static constexpr const long ns_per_s = 1000000000;
    static constexpr const long ns_per_ms= ns_per_s / 1000;
    auto cycle_start = std::chrono::high_resolution_clock::now();
    long cycle_time{};
    long timeout = ns_per_s / chat.pulse_per_second;

    alarm(1);

    Again:

    while (!chat.sockets->next_error(static_cast<int>(timeout / ns_per_ms))) {
        auto cycle_end = std::chrono::high_resolution_clock::now();

        if (sig_interruption) {
            log("An interruption signal has been caught.");
            break;
        }

        if (!chat.sockets->idle()) {
            handle_sockets(chat);
        }

        cycle_time += (
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                cycle_end - cycle_start
            ).count()
        );

        if (cycle_time >= timeout) {
            cycle_start = std::chrono::high_resolution_clock::now();

            handle_updates(chat);

            cycle_time = (
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::high_resolution_clock::now() - cycle_start
                ).count()
            );
        }

        timeout = static_cast<decltype(timeout)>(
            std::min(
                std::max(
                    (ns_per_s / chat.pulse_per_second) - cycle_time,
                    decltype(cycle_time){1}
                ),
                static_cast<decltype(cycle_time)>(
                    std::numeric_limits<decltype(timeout)>::max()
                )
            )
        );

        if (sig_alarm) {
            sig_alarm = 0;

            if (chat.online != chat.users->size()) {
                chat.online = chat.users->size();
                log(
                    std::to_string(chat.online).append(
                        chat.online == 1 ? " user is" : " users are"
                    ).append(" currently online.")
                );
            }

            alarm(1);
        }
    }

    switch (chat.sockets->last_error()) {
        case SOCKETS::NO_ERROR: break;
        case SOCKETS::OUT_OF_MEMORY: {
            if (chat.sockets->get_memtop() * 256 < chat.sockets->get_memcap()) {
                log("Memory usage has reached its hard limit.");

                break;
            }

            chat.sockets->set_memcap(chat.sockets->get_memcap() * 2);

            log(
                std::string(
                    "Out of Memory: new memcap set to "
                ).append(std::to_string(chat.sockets->get_memcap())).append(
                    " bytes."
                )
            );

            timeout = 0;

            goto Again;
        }
        default: {
            log(
                std::string(
                    "Error serving the sockets ("
                ).append(
                    chat.sockets->to_string(chat.sockets->last_error())
                ).append(").")
            );

            break;
        }
    }
}

static void handle_updates(CHAT &chat) {
    std::vector<size_t> erased;

    for (auto &p : *(chat.users)) {
        USER &user = p.second;

        update_user(chat, user);

        if (user.state == USER::STATE::EXIT) {
            erased.push_back(user.session_id);
        }
    }

    while (!erased.empty()) {
        chat.users->erase(erased.back());
        erased.pop_back();
    }
}

static void update_user(CHAT &chat, USER &user) {
    switch (user.state) {
        case USER::STATE::NONE:
        case USER::STATE::GREET: {
            user.outgoing.append("Welcome to the chat!\n\r\n\r");
            user.outgoing.append("Please enter your name:\n\r");

            user.state = USER::STATE::NAME;
            break;
        }
        case USER::STATE::NAME:
        case USER::STATE::CHAT: {
            break;
        }
        case USER::STATE::EXIT: {
            for (auto &p : *chat.users) {
                USER &u = p.second;

                if (u.state != USER::STATE::CHAT
                ||  u.session_id == user.session_id) {
                    continue;
                }

                u.outgoing.append(user.name).append(" has left.\n\r");
            }

            break;
        }
    }

    if (!user.incoming.empty()) {
        size_t command_length = user.incoming.find_first_of('\n');

        if (command_length != std::string::npos) {
            std::string command(user.incoming, 0, command_length);

            user.incoming.erase(0, command_length + 1);

            interpret(chat, user, command);
        }
    }

    if (!user.outgoing.empty() && user.connected) {
        SOCKETS::ERROR error{
            chat.sockets->write(
                user.session_id, user.outgoing.c_str(), user.outgoing.size()
            )
        };

        if (!error) {
            user.outgoing.clear();
        }
    }
}

static void handle_sockets(CHAT &chat) {
    SOCKETS::ALERT alert;
    std::vector<uint8_t> buffer;

    while ((alert = chat.sockets->next_alert()).valid) {
        size_t sid = alert.session;

        switch (alert.event) {
            case SOCKETS::CONNECTION: {
                log(
                    std::string(
                        "New connection from "
                    ).append(chat.sockets->get_host(sid)).append(":").append(
                        chat.sockets->get_port(sid)
                    ).append(".")
                );

                if (!chat.users->count(sid)) {
                    USER &u = chat.users->try_emplace(sid).first->second;

                    u.session_id = sid;
                    u.connected = true;
                }

                continue;
            }
            case SOCKETS::DISCONNECTION: {
                log(
                    std::string(
                        "Disconnected "
                    ).append(chat.sockets->get_host(sid)).append(":").append(
                        chat.sockets->get_port(sid)
                    ).append(".")
                );

                if (chat.users->count(sid)) {
                    USER &u = chat.users->at(sid);

                    u.state = USER::STATE::EXIT;
                    u.connected = false;
                }

                continue;
            }
            case SOCKETS::INCOMING: {
                buffer.resize(
                    std::max(buffer.capacity(), chat.sockets->incoming(sid))
                );

                size_t count = chat.sockets->read(
                    sid, buffer.data(), buffer.size()
                );

                chat.users->at(sid).incoming.append(
                    reinterpret_cast<const char *>(buffer.data()), count
                );

                continue;
            }
            default: continue;
        }
    }
}

static void interpret(CHAT &chat, USER &user, std::string &command) {
    switch (user.state) {
        case USER::STATE::NONE:
        case USER::STATE::GREET: {
            command.append(1, '\n').append(user.incoming);
            command.swap(user.incoming);
            break;
        }
        case USER::STATE::NAME: {
            user.name.assign(command);

            if (user.name.empty()) {
                user.outgoing.append("Please enter your name:\n\r");
                return;
            }

            user.name[0] = static_cast<char>(toupper(user.name[0]));

            user.outgoing.append("Hello, ").append(user.name).append("! ");
            user.outgoing.append("You are now ready to chat.\n\r");

            user.state = USER::STATE::CHAT;

            for (auto &p : *chat.users) {
                USER &u = p.second;

                if (u.state != USER::STATE::CHAT
                || u.session_id == user.session_id) {
                    continue;
                }

                u.outgoing.append(user.name).append(" has joined.\n\r");
            }

            break;
        }
        case USER::STATE::CHAT: {
            do_say(chat, user, command);
            break;
        }
        default: break;
    }
}

static void do_say(CHAT &chat, USER &user, std::string &text) {
    static const constexpr char
        *ansi_cyan  = "\x1B[36m",
        *ansi_aqua  = "\x1B[1;36m",
        *ansi_reset = "\x1B[0m";

    for (auto &p : *chat.users) {
        USER &u = p.second;

        if (u.state != USER::STATE::CHAT) {
            continue;
        }

        if (u.session_id == user.session_id) {
            u.outgoing.append(ansi_cyan).append(
                "You say "
            ).append(ansi_aqua).append("'").append(ansi_reset).append(
                text
            ).append(ansi_aqua).append("'").append(ansi_reset).append("\n\r");
        }
        else {
            u.outgoing.append(ansi_cyan).append(user.name).append(
                " says "
            ).append(ansi_aqua).append("'").append(ansi_reset).append(
                text
            ).append(ansi_aqua).append("'").append(ansi_reset).append("\n\r");
        }
    }
}

static void log(const char *line) noexcept {
    time_t rawtime;
    struct tm *timeinfo;
    char now[80];

    time (&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(now, sizeof(now), "%d-%m-%Y %H:%M:%S", timeinfo);

    const char *segments[5]{
        "[ ", now, " ] :: ", line, "\x1B[0m\n\n"
    };

    for (const char *segment : segments) {
        size_t len = strlen(segment);

        write(
            STDERR_FILENO, segment, len && segment[len-1] == '\n' ? len-1 : len
        );
    }
}

static void log(const std::string &str) {
    return log(str.c_str());
}

static void log(SOCKETS::SESSION session, const char *txt) noexcept {
    const char *esc = "\x1B[0;31m";

    switch (session.error) {
        case SOCKETS::BAD_TIMING:    esc = "\x1B[1;33m"; break;
        case SOCKETS::LIBRARY_ERROR: esc = "\x1B[1;31m"; break;
        case SOCKETS::NO_ERROR:      esc = "\x1B[0;32m"; break;
        default: break;
    }

    char stackbuf[1024];

    if (!session) {
        snprintf(stackbuf, sizeof(stackbuf), "Sockets: %s%s", esc, txt);
    }
    else {
        snprintf(
            stackbuf, sizeof(stackbuf), "#%06lx: %s%s", session.id, esc, txt
        );
    }

    log(stackbuf);
}

static void signal_handler(int signal) {
    switch (signal) {
        case SIGALRM: sig_alarm = 1;        break;
        case SIGINT:  sig_interruption = 1; break;
        default: break;
    }
}
