// SPDX-License-Identifier: MIT
#include "../../sockets.h"

static void handle(SOCKETS &sockets);

int main(int argc, char **argv) {
    static constexpr const char *SERVER_PORT = "4000";
    SOCKETS sockets;

    printf("Initializing networking using SOCKETS v%s.\n", SOCKETS::VERSION);

    bool success{
        sockets.init(
            [&](const char *txt) {
                printf("Sockets: %s\n", txt);
            }
        )
    };

    if (!success) {
        return EXIT_FAILURE;
    }

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
    int d = SOCKETS::NO_DESCRIPTOR;

    while ((d = sockets.next_disconnection()) != SOCKETS::NO_DESCRIPTOR) {
        printf(
            "Disconnected %s:%s (descriptor %d).\n",
            sockets.get_host(d), sockets.get_port(d), d
        );
    }

    while ((d = sockets.next_connection()) != SOCKETS::NO_DESCRIPTOR) {
        printf(
            "New connection from %s:%s (descriptor %d).\n",
            sockets.get_host(d), sockets.get_port(d), d
        );

        sockets.writef(
            d, "Hello, %s:%s!\n\r", sockets.get_host(d), sockets.get_port(d)
        );
    }

    std::vector<uint8_t> buffer;

    while ((d = sockets.next_incoming()) != SOCKETS::NO_DESCRIPTOR) {
        sockets.swap_incoming(d, buffer);

        printf(
            "Received %lu byte%s from descriptor %d.\n",
            buffer.size(), buffer.size() == 1 ? "" : "s", d
        );

        sockets.append_outgoing(d, buffer);

        buffer.clear();
    }
}
