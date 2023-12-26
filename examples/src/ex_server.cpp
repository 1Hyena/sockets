// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>
#include <vector>

volatile sig_atomic_t interruption = 0;

static void handle(SOCKETS &sockets);

void interruption_handler(int) {
    interruption = 1;
}

int main(int argc, char **argv) {
    static constexpr const char *SERVER_PORT = "4000";

    std::signal(SIGINT, interruption_handler);

    SOCKETS sockets;

    printf("Initializing networking using SOCKETS v%s.\n", SOCKETS::VERSION);

    sockets.set_logger(
        [](SOCKETS::SESSION, const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize sockets.");
        return EXIT_FAILURE;
    }

    SOCKETS::SESSION session{
        sockets.listen(
            SERVER_PORT,
            AF_UNSPEC, // Accept connections from any address family.
            {
                SO_REUSEADDR, // Allow instantaneous server restarting.
                SO_REUSEPORT  // Allow multiple listeners on the same port.
            }
        )
    };

    if (session.valid) {
        constexpr int timeout_ms = 3000;

        printf(
            "Listening for TCP connections on %s:%s.\n",
            sockets.get_host(session.id), sockets.get_port(session.id)
        );

        while (!sockets.next_error(timeout_ms)) {
            if (sockets.idle()) {
                printf(
                    "Nothing happened in the last %d seconds.\n",
                    timeout_ms / 1000
                );
            }
            else {
                handle(sockets);
            }

            if (interruption) {
                printf("Shutting down due to interruption.\n");
                break;
            }
        }

        if (sockets.last_error() != SOCKETS::NO_ERROR) {
            printf(
                "Error serving the sockets (%s).\n",
                sockets.to_string(sockets.last_error())
            );
        }

        sockets.disconnect(session.id);
    }

    sockets.deinit();

    return EXIT_SUCCESS;
}

static void handle(SOCKETS &sockets) {
    SOCKETS::ALERT alert;
    std::vector<uint8_t> buffer;

    while ((alert = sockets.next_alert()).valid) {
        size_t sid = alert.session;

        switch (alert.event) {
            case SOCKETS::CONNECTION: {
                printf(
                    "New connection from %s:%s (session %lu).\n",
                    sockets.get_host(sid), sockets.get_port(sid), sid
                );

                sockets.writef(
                    sid, "Hello, %s:%s!\n",
                    sockets.get_host(sid), sockets.get_port(sid)
                );

                continue;
            }
            case SOCKETS::DISCONNECTION: {
                printf(
                    "Disconnected %s:%s (session %lu).\n",
                    sockets.get_host(sid), sockets.get_port(sid), sid
                );

                continue;
            }
            case SOCKETS::INCOMING: {
                buffer.resize(
                    std::max(buffer.capacity(), sockets.incoming(sid))
                );

                size_t count = sockets.read(sid, buffer.data(), buffer.size());

                printf(
                    "Received %lu byte%s from session %lu.\n",
                    count, count == 1 ? "" : "s", sid
                );

                sockets.write(sid, buffer.data(), count);

                continue;
            }
            default: continue;
        }
    }
}
