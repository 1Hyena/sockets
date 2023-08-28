# About ########################################################################

SOCKETS is a single-threaded and signal-compatible header-only C++ library for
the creation and acceptance of TCP connections. The library makes use of the
*epoll* Linux kernel system call to achieve a scalable I/O event notification
mechanism.

# Usage ########################################################################

In the following subsection various code snippets are provided in order to
exemplify the most basic uses of this library. Then, a set of practical example
programs is provided for the creation of production grade software.


## Minimalist Examples #########################################################

The following piece of code is the most minimalist example of a TCP server
using the SOCKETS library.

```
SOCKETS sockets;
sockets.init();

int d, tcp_listener = sockets.listen(4000);

while (sockets.serve()) {
    while ((d = sockets.next_connection()) != SOCKETS::NO_DESCRIPTOR) {
        sockets.writef(
            d, "Hello, %s:%s!\n\r", sockets.get_host(d), sockets.get_port(d)
        );
    }
}
```

## Practical Examples ##########################################################

Most common use cases for this library are documented with the following code
examples.

* [ex_server](examples/src/ex_server.cpp) —
  This example server listens for incoming TCP connections on port 4000 and
  greets them with a short text message. After accepting a new connection, it
  just echoes anything it receives back to the client.

* [ex_client](examples/src/ex_client.cpp) —
  This code example shows how to establish a single outgoing TCP connection and
  exchange some data with the target server. The client connects to _localhost_
  on port 4000 and sends them some text message upon a successful connection.

In the following subsections the most common build instructions and the target
platform system requirements are given.


## Build Instructions ##########################################################

SOCKETS is written in C++ and should be trivial to compile on most Linux based
systems. In order to build the examples, just go to the _examples_ directory and
type _make_. If compilation fails, then most likely you are missing some of the
required dependencies listed in the following section.


## Requirements ################################################################

The only requirements are a C++17 compliant compiler and Linux kernel headers.


# License ######################################################################

The SOCKETS library has been authored by Erich Erstu and is released under the
[MIT](LICENSE) license.
