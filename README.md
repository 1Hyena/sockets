# About ########################################################################

SOCKETS is a single-threaded, non-throwing and signal-compatible header-only C++
library for the creation and acceptance of TCP connections. The library makes
use of the *epoll* Linux kernel system call to achieve a scalable I/O event
notification mechanism.


# Usage ########################################################################

In the following subsection various code snippets are provided in order to
exemplify the most basic uses of this library. After that, a set of practical
example programs is provided for the creation of production grade software.


## Minimalistic Examples #######################################################

The following piece of code is the most minimalistic example of a TCP server
using the SOCKETS library.

```C++
sockets.listen("4000");

while (sockets.serve()) {
    SOCKETS::EVENT ev{ sockets.next_event() };

    if (ev.type == SOCKETS::EV_CONNECTION) {
        sockets.writef(
            ev.descriptor, "Hello, %s:%s!\n",
            sockets.get_host(ev.descriptor), sockets.get_port(ev.descriptor)
        );

        sockets.disconnect(ev.descriptor);
    }
}
```

A really simple TCP client that doesn't do any error checking would look like
the following code snippet.

```C++
sockets.connect("localhost", "4000");

while (sockets.serve()) {
    SOCKETS::EVENT ev{ sockets.next_event() };

    if (ev.type == SOCKETS::EV_CONNECTION) {
        printf(
            "Connected to %s:%s.\n",
            sockets.get_host(ev.descriptor), sockets.get_port(ev.descriptor)
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

* [ex_sleep](examples/src/ex_sleep.cpp) —
  The _SOCKETS::serve_ method can take an argument specifying the maximum number
  of milliseconds to wait for networking events. This feature can be used to
  simulate a sleeping routine where the program stops using any processing power
  for a given amount of time.

* [ex_signal](examples/src/ex_signal.cpp) —
  Since this library is inherently single-threaded, it is trivial to use it in
  combination with custom signal handlers. For example, one might want to
  implement the main loop of their application with the help of _SIGALRM_ to
  make sure that each cycle starts after a fixed time interval.

In the following subsections the regular build instructions and target platform
system requirements are given.


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
