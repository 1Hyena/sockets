// SPDX-License-Identifier: MIT
#include "../../sockets.h"

void signal_handler(int sig) {
    static const char *text = "A signal has been caught!\n";
    write(STDOUT_FILENO, text, strlen(text));
}

int main(int argc, char **argv) {
    signal(SIGALRM, signal_handler);
    alarm(3);

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

    printf("%s\n", "Sleeping indefinitely.");

    if (!sockets.serve()) {
        printf("%s\n", "Error serving the sockets.");
    }

    printf("%s\n", "Deinitializing networking.");
    sockets.deinit();

    return EXIT_SUCCESS;
}
