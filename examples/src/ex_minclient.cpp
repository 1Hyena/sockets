// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

int main(int argc, char **argv) {
    SOCKETS sockets;
    sockets.init();
    sockets.connect("localhost", "4000");

    while (!sockets.next_error()) {
        SOCKETS::ALERT alert{ sockets.next_alert() };

        if (alert.event == SOCKETS::CONNECTION) {
            printf(
                "Connected to %s:%s.\n",
                sockets.get_host(alert.session),
                sockets.get_port(alert.session)
            );
        }
    }

    return EXIT_SUCCESS;
}
