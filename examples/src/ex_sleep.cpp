// SPDX-License-Identifier: MIT
#include "../../sockets.h"

int main(int argc, char **argv) {
    constexpr int timeout_milliseconds = 10000;
    SOCKETS sockets;

    printf("Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION);

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize networking.");
        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](const char *txt) {
            printf("Sockets: %s\n", txt);
        }
    );

    printf(
        "Sleeping for %d millisecond%s.\n",
        timeout_milliseconds, timeout_milliseconds == 1 ? "" : "s"
    );

    sockets.serve(timeout_milliseconds);

    printf("%s\n", "Deinitializing networking.");
    sockets.deinit();

    return EXIT_SUCCESS;
}
