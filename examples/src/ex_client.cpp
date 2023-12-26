// SPDX-License-Identifier: MIT
#include "../../sockets.h"
#include <cstdlib>

static void log(const char *line) noexcept;
static void log(SOCKETS::SESSION, const char *line) noexcept;

int main(int argc, char **argv) {
    char logbuf[1024];
    const char *SERVER_HOST = "localhost";
    const char *SERVER_PORT = "4000";

    if (argc >= 3) {
        SERVER_HOST = argv[1];
        SERVER_PORT = argv[2];
    }
    else {
        printf("Usage: %s HOST PORT\n", argv[0]);
        return EXIT_FAILURE;
    }

    SOCKETS sockets;

    snprintf(
        logbuf, sizeof(logbuf),
        "Initializing networking (SOCKETS v%s).\n", SOCKETS::VERSION
    );
    log(logbuf);

    if (!sockets.init()) {
        snprintf(
            logbuf, sizeof(logbuf), "%s\n", "Failed to initialize sockets."
        );
        log(logbuf);

        return EXIT_FAILURE;
    }

    sockets.set_logger(
        [](SOCKETS::SESSION session, const char *txt) noexcept {
            log(session, txt);
        }
    );

    snprintf(
        logbuf, sizeof(logbuf),
        "Connecting to %s:%s.\n", SERVER_HOST, SERVER_PORT
    );
    log(logbuf);

    SOCKETS::SESSION session = sockets.connect(SERVER_HOST, SERVER_PORT);

    if (session.valid) {
        constexpr int timeout_ms = 30000;
        bool connected = false;
        SOCKETS::ALERT alert;

        snprintf(
            logbuf, sizeof(logbuf), "%s\n", "Waiting for socket events."
        );
        log(logbuf);

        while (!sockets.next_error(timeout_ms)) {
            if (sockets.idle()) {
                snprintf(
                    logbuf, sizeof(logbuf),
                    "Nothing happened in the last %d seconds.\n",
                    timeout_ms / 1000
                );
                log(logbuf);

                continue;
            }

            while ((alert = sockets.next_alert()).valid) {
                size_t sid = alert.session;

                switch (alert.event) {
                    case SOCKETS::CONNECTION: {
                        snprintf(
                            logbuf, sizeof(logbuf),
                            "Connected to %s:%s.\n",
                            sockets.get_host(sid), sockets.get_port(sid)
                        );
                        log(logbuf);

                        connected = true;
                        sockets.write(sid, "Ahoy!\n");

                        continue;
                    }
                    case SOCKETS::DISCONNECTION: {
                        if (connected) {
                            snprintf(
                                logbuf, sizeof(logbuf),
                                "Disconnected from %s:%s.\n",
                                sockets.get_host(sid), sockets.get_port(sid)
                            );
                            log(logbuf);

                            goto TheEnd;
                        }

                        break;
                    }
                    case SOCKETS::INCOMING: {
                        printf(
                            "%s:%s> %s",
                            sockets.get_host(sid), sockets.get_port(sid),
                            sockets.read(sid)
                        );

                        continue;
                    }
                    default: continue;
                }

                break;
            }

            if (alert.event == SOCKETS::DISCONNECTION) break;
        }

        if (sockets.last_error() != SOCKETS::NO_ERROR) {
            snprintf(
                logbuf, sizeof(logbuf),
                "Error serving the sockets (%s).\n",
                sockets.to_string(sockets.last_error())
            );
            log(logbuf);

            goto TheEnd;
        }
    }

    snprintf(
        logbuf, sizeof(logbuf),
        "Failed to connect to %s:%s.\n", SERVER_HOST, SERVER_PORT
    );
    log(logbuf);

    TheEnd:

    snprintf(
        logbuf, sizeof(logbuf), "Deinitializing networking.\n"
    );
    log(logbuf);

    sockets.deinit();

    return EXIT_SUCCESS;
}

static void log(const char *line) noexcept {
    const char *segments[2]{
        line, "\x1B[0m\n\n"
    };

    for (const char *segment : segments) {
        size_t len = strlen(segment);

        write(
            STDERR_FILENO, segment, len && segment[len-1] == '\n' ? len-1 : len
        );
    }
}

static void log(SOCKETS::SESSION session, const char *txt) noexcept {
    const char *esc = "\x1B[0;31m";

    switch (session.error) {
        case SOCKETS::BAD_TIMING:    esc = "\x1B[1;33m"; break;
        case SOCKETS::LIBRARY_ERROR: esc = "\x1B[1;31m"; break;
        case SOCKETS::NO_ERROR:      esc = "\x1B[0;32m"; break;
        default: break;
    }

    char stackbuf[1024];

    if (!session) {
        snprintf(stackbuf, sizeof(stackbuf), "Sockets: %s%s", esc, txt);
    }
    else {
        snprintf(
            stackbuf, sizeof(stackbuf), "#%06lx: %s%s", session.id, esc, txt
        );
    }

    log(stackbuf);
}
