// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

int main(int argc, char **argv) {
    constexpr int timeout_milliseconds = 10000;
    SOCKETS sockets;

    printf("Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION);

    if (!sockets.init()) {
        printf("%s\n", "Failed to initialize sockets.");
        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](SOCKETS::SESSION, const char *txt) noexcept {
            printf("Sockets: %s\n", txt);
        }
    );

    printf(
        "Sleeping for %d millisecond%s.\n",
        timeout_milliseconds, timeout_milliseconds == 1 ? "" : "s"
    );

    sockets.next_error(timeout_milliseconds);

    printf("%s\n", "Deinitializing networking.");
    sockets.deinit();

    return EXIT_SUCCESS;
}
