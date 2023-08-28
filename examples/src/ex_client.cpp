// SPDX-License-Identifier: MIT
#include "../../sockets.h"

static void handle(SOCKETS &sockets);

int main(int argc, char **argv) {
    static constexpr const char *SERVER_HOST = "localhost";
    static constexpr const char *SERVER_PORT = "4000";

    SOCKETS sockets;

    printf("Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION);

    bool success{
        sockets.init(
            [&](const char *txt) {
                printf("Sockets: %s\n", txt);
            }
        )
    };

    if (!success) {
        printf("%s\n", "Failed to initialize networking.");
        return EXIT_FAILURE;
    }

    printf("Connecting to %s:%s.\n", SERVER_HOST, SERVER_PORT);

    if (sockets.connect(SERVER_HOST, SERVER_PORT)) {
        constexpr int timeout_milliseconds = 3000;
        bool connected = false;
        bool success;
        int d;

        printf("%s\n", "Waiting for socket events.");

        while (true == (success = sockets.serve(timeout_milliseconds))) {
            while ((d = sockets.next_connection()) != SOCKETS::NO_DESCRIPTOR) {
                printf(
                    "Connected to %s:%s.\n",
                    sockets.get_host(d), sockets.get_port(d)
                );

                connected = true;
                sockets.write(d, "Ahoy!\n");
            }

            if ((d = sockets.next_disconnection()) != SOCKETS::NO_DESCRIPTOR) {
                if (connected) {
                    printf(
                        "Disconnected from %s:%s.\n",
                        sockets.get_host(d), sockets.get_port(d)
                    );

                    goto TheEnd;
                }
                else break;
            }

            if (sockets.idle()) {
                printf(
                    "Nothing happened in the last %d seconds.\n",
                    timeout_milliseconds / 1000
                );
            }
            else handle(sockets);
        }

        if (!success) {
            printf("%s\n", "Error serving the sockets.");
            goto TheEnd;
        }
    }

    printf("Failed to connect to %s:%s.\n", SERVER_HOST, SERVER_PORT);

    TheEnd:

    printf("Deinitializing networking.\n");
    sockets.deinit();

    return EXIT_SUCCESS;
}

static void handle(SOCKETS &sockets) {
    int d = SOCKETS::NO_DESCRIPTOR;

    std::vector<uint8_t> buf;

    while ((d = sockets.next_incoming()) != SOCKETS::NO_DESCRIPTOR) {
        sockets.swap_incoming(d, buf);

        while (!buf.empty() && (buf.back() == '\n' || buf.back() == '\r')) {
            buf.pop_back();
        }

        buf.push_back(0);

        printf(
            "%s:%s> %s\n",
            sockets.get_host(d), sockets.get_port(d), (const char *) buf.data()
        );

        buf.clear();
    }
}
