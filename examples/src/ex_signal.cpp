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

    printf("%s\n", "Sleeping indefinitely.");

    if (!sockets.serve()) {
        printf("%s", "Error serving the sockets.\n");
    }

    printf("%s\n", "Deinitializing networking.");
    sockets.deinit();

    return EXIT_SUCCESS;
}
