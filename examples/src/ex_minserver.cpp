// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

int main(int argc, char **argv) {
    SOCKETS sockets;
    sockets.init();
    sockets.listen("4000");

    while (!sockets.next_error()) {
        SOCKETS::ALERT alert{ sockets.next_alert() };

        if (alert.event == SOCKETS::CONNECTION) {
            sockets.writef(
                alert.session, "Hello, %s:%s!\n",
                sockets.get_host(alert.session),
                sockets.get_port(alert.session)
            );

            sockets.disconnect(alert.session);
        }
    }

    return EXIT_SUCCESS;
}
