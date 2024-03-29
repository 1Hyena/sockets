// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

void signal_handler(int sig) {
    static const char *text = "A signal has been caught!\n";
    write(STDOUT_FILENO, text, strlen(text));
}

int main(int argc, char **argv) {
    std::signal(SIGALRM, signal_handler);
    alarm(3);

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

    printf("%s\n", "Sleeping indefinitely.");

    SOCKETS::ERROR error;

    if ((error = sockets.next_error()) != SOCKETS::NO_ERROR) {
        printf("Error serving the sockets (%s).\n", sockets.to_string(error));
    }

    printf("%s\n", "Deinitializing networking.");
    sockets.deinit();

    return EXIT_SUCCESS;
}
