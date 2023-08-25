// SPDX-License-Identifier: MIT
#include "../../sockets.h"

static void handle(SOCKETS &sockets);

int main(int argc, char **argv) {
    static constexpr const char *SERVER_HOST = "localhost";
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

    if (!sockets.connect(SERVER_HOST, SERVER_PORT)) {
        printf("Failed to connect to %s:%s.\n", SERVER_HOST, SERVER_PORT);
    }
    else {
        constexpr int timeout_milliseconds = 3000;

        printf("%s\n", "Waiting for TCP connection events.");

        while (sockets.serve(/*timeout_milliseconds*/)) {
            // TODO: Add a possibility to learn about refused connections.
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

        printf("%s\n", "Error serving the sockets.");
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
            "New connection to %s:%s (descriptor %d).\n",
            sockets.get_host(d), sockets.get_port(d), d
        );
    }

    std::vector<uint8_t> buf;

    while ((d = sockets.next_incoming()) != SOCKETS::NO_DESCRIPTOR) {
        sockets.swap_incoming(d, buf);

        while (!buf.empty() && (buf.back() == '\n' || buf.back() == '\r')) {
            buf.pop_back();
        }

        buf.push_back(0);

        printf("Descriptor %d says: '%s'\n", d, (const char *) buf.data());

        buf.clear();
    }
}
