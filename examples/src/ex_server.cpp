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
        [](const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize networking.");
        return EXIT_FAILURE;
    }

    int tcp_listener = sockets.listen(SERVER_PORT);

    if (tcp_listener != SOCKETS::NO_DESCRIPTOR) {
        constexpr int timeout_ms = 3000;

        printf(
            "Listening for TCP connections on %s:%s.\n",
            sockets.get_host(tcp_listener), sockets.get_port(tcp_listener)
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

        if (sockets.last_error() != SOCKETS::ERR_NONE) {
            printf(
                "Error serving the sockets (%s).\n",
                sockets.get_code(sockets.last_error())
            );
        }

        sockets.disconnect(tcp_listener);
    }

    sockets.deinit();

    return EXIT_SUCCESS;
}

static void handle(SOCKETS &sockets) {
    SOCKETS::EVENT ev;
    std::vector<uint8_t> buffer;

    while ((ev = sockets.next_event()).valid) {
        int d = ev.descriptor;

        switch (ev.type) {
            case SOCKETS::EV_CONNECTION: {
                printf(
                    "New connection from %s:%s (descriptor %d).\n",
                    sockets.get_host(d), sockets.get_port(d), d
                );

                sockets.writef(
                    d, "Hello, %s:%s!\n",
                    sockets.get_host(d), sockets.get_port(d)
                );

                continue;
            }
            case SOCKETS::EV_DISCONNECTION: {
                printf(
                    "Disconnected %s:%s (descriptor %d).\n",
                    sockets.get_host(d), sockets.get_port(d), d
                );

                continue;
            }
            case SOCKETS::EV_INCOMING: {
                buffer.resize(std::max(buffer.capacity(), sockets.incoming(d)));

                size_t count = sockets.read(d, buffer.data(), buffer.size());

                printf(
                    "Received %lu byte%s from descriptor %d.\n",
                    count, count == 1 ? "" : "s", d
                );

                sockets.write(d, buffer.data(), count);

                continue;
            }
            default: continue;
        }
    }
}
