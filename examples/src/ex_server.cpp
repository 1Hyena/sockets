// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

static void handle(SOCKETS &sockets);

int main(int argc, char **argv) {
    static constexpr const char *SERVER_PORT = "4000";
    SOCKETS sockets;

    printf("Initializing networking using SOCKETS v%s.\n", SOCKETS::VERSION);

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize networking.");
        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    int tcp_listener = sockets.listen(SERVER_PORT);

    if (tcp_listener != SOCKETS::NO_DESCRIPTOR) {
        constexpr int timeout_milliseconds = 3000;

        printf(
            "Listening for TCP connections on %s:%s.\n",
            sockets.get_host(tcp_listener), sockets.get_port(tcp_listener)
        );

        while (sockets.serve(timeout_milliseconds)) {
            if (sockets.idle()) {
                printf(
                    "Nothing happened in the last %d seconds.\n",
                    timeout_milliseconds / 1000
                );
            }
            else {
                handle(sockets);
            }
        }

        printf("%s", "Error serving the sockets.\n");
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
                sockets.swap_incoming(d, buffer);

                printf(
                    "Received %lu byte%s from descriptor %d.\n",
                    buffer.size(), buffer.size() == 1 ? "" : "s", d
                );

                sockets.append_outgoing(d, buffer.data(), buffer.size());

                buffer.clear();

                continue;
            }
            default: continue;
        }
    }
}
