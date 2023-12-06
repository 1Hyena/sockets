// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

int main(int argc, char **argv) {
    static constexpr const char *SERVER_HOST = "localhost";
    static constexpr const char *SERVER_PORT = "4000";

    SOCKETS sockets;

    printf("Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION);

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize sockets.");
        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    printf("Connecting to %s:%s.\n", SERVER_HOST, SERVER_PORT);

    if (sockets.connect(SERVER_HOST, SERVER_PORT).valid) {
        constexpr int timeout_ms = 3000;
        bool connected = false;
        SOCKETS::ALERT alert;

        printf("%s\n", "Waiting for socket events.");

        while (!sockets.next_error(timeout_ms)) {
            if (sockets.idle()) {
                printf(
                    "Nothing happened in the last %d seconds.\n",
                    timeout_ms / 1000
                );

                continue;
            }

            while ((alert = sockets.next_alert()).valid) {
                size_t sid = alert.session;

                switch (alert.event) {
                    case SOCKETS::EV_CONNECTION: {
                        printf(
                            "Connected to %s:%s.\n",
                            sockets.get_host(sid), sockets.get_port(sid)
                        );

                        connected = true;
                        sockets.write(sid, "Ahoy!\n");

                        continue;
                    }
                    case SOCKETS::EV_DISCONNECTION: {
                        if (connected) {
                            printf(
                                "Disconnected from %s:%s.\n",
                                sockets.get_host(sid), sockets.get_port(sid)
                            );

                            goto TheEnd;
                        }

                        break;
                    }
                    case SOCKETS::EV_INCOMING: {
                        printf(
                            "%s:%s> %s\n",
                            sockets.get_host(sid), sockets.get_port(sid),
                            sockets.read(sid)
                        );

                        continue;
                    }
                    default: continue;
                }

                break;
            }

            if (alert.event == SOCKETS::EV_DISCONNECTION) break;
        }

        if (sockets.last_error() != SOCKETS::ERR_NONE) {
            printf(
                "Error serving the sockets (%s).\n",
                sockets.get_code(sockets.last_error())
            );

            goto TheEnd;
        }
    }

    printf("Failed to connect to %s:%s.\n", SERVER_HOST, SERVER_PORT);

    TheEnd:

    printf("Deinitializing networking.\n");
    sockets.deinit();

    return EXIT_SUCCESS;
}
