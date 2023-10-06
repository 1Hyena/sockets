// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

int main(int argc, char **argv) {
    static constexpr const char *SERVER_HOST = "localhost";
    static constexpr const char *SERVER_PORT = "4000";

    SOCKETS sockets;

    printf("Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION);

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize networking.");
        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    printf("Connecting to %s:%s.\n", SERVER_HOST, SERVER_PORT);

    if (sockets.connect(SERVER_HOST, SERVER_PORT)) {
        constexpr int timeout_ms = 3000;
        bool connected = false;
        SOCKETS::ERROR error;
        SOCKETS::EVENT ev;

        printf("%s\n", "Waiting for socket events.");

        while ((error = sockets.serve(timeout_ms)) == SOCKETS::ERR_NONE) {
            if (sockets.idle()) {
                printf(
                    "Nothing happened in the last %d seconds.\n",
                    timeout_ms / 1000
                );

                continue;
            }

            while ((ev = sockets.next_event()).valid) {
                int d = ev.descriptor;

                switch (ev.type) {
                    case SOCKETS::EV_CONNECTION: {
                        printf(
                            "Connected to %s:%s.\n",
                            sockets.get_host(d), sockets.get_port(d)
                        );

                        connected = true;
                        sockets.write(d, "Ahoy!\n");

                        continue;
                    }
                    case SOCKETS::EV_DISCONNECTION: {
                        if (connected) {
                            printf(
                                "Disconnected from %s:%s.\n",
                                sockets.get_host(d), sockets.get_port(d)
                            );

                            goto TheEnd;
                        }

                        break;
                    }
                    case SOCKETS::EV_INCOMING: {
                        printf(
                            "%s:%s> %s\n",
                            sockets.get_host(d), sockets.get_port(d),
                            sockets.read(d)
                        );

                        continue;
                    }
                    default: continue;
                }

                break;
            }

            if (ev.type == SOCKETS::EV_DISCONNECTION) break;
        }

        if (error != SOCKETS::ERR_NONE) {
            printf(
                "Error serving the sockets (%s).\n", sockets.get_code(error)
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
