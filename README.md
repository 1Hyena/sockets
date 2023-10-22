# About ########################################################################

SOCKETS is a single-threaded, non-throwing and signal-compatible header-only C++
library for the creation and acceptance of TCP connections. The library makes
use of the *epoll* Linux kernel system call to achieve a scalable I/O event
notification mechanism.

* **Single-Threaded** —
  Unlike most networking libraries, this one does not maintain a pool of threads
  in order to concurrently manage multiple connections.

* **Non-Throwing** —
  The SOCKETS class has all of its methods specified as *noexcept*, giving it a
  no-throw exception guarantee. In other words, it's perfectly fine to use this
  library in a program that is compiled with the `-fno-exceptions` flag.

* **Signal-Compatible** —
  This library can safely be used by a program that uses signals. It makes the
  necessary calls to _sigprocmask_ to block all signals before executing tasks
  that must not be interrupted by signals.

* **Header-Only** —
  Everything in this library is provided by a single self-contained header file
  named [sockets.h](sockets.h).

* **Scalable** —
  There is no intrinsic limit on the number of sockets managed by the library.
  In other words, the CPU usage of this library is not significantly affected by
  the number of established connections.


# Usage ########################################################################

In the following subsection various code snippets are provided in order to
exemplify the most basic uses of this library. After that, a set of practical
example programs is provided for the creation of production grade software.


## Minimalistic Examples #######################################################

The following piece of code is the most minimalistic example of a TCP server
using the SOCKETS library.

```C++
sockets.listen("4000");

while (!sockets.next_error()) {
    SOCKETS::ALERT alert{ sockets.next_alert() };

    if (alert.event == SOCKETS::EV_CONNECTION) {
        sockets.writef(
            alert.descriptor, "Hello, %s:%s!\n",
            sockets.get_host(alert.descriptor),
            sockets.get_port(alert.descriptor)
        );

        sockets.disconnect(alert.descriptor);
    }
}
```

A really simple TCP client that doesn't do any error checking would look like
the following code snippet.

```C++
sockets.connect("localhost", "4000");

while (!sockets.next_error()) {
    SOCKETS::ALERT alert{ sockets.next_alert() };

    if (alert.event == SOCKETS::EV_CONNECTION) {
        printf(
            "Connected to %s:%s.\n",
            sockets.get_host(alert.descriptor),
            sockets.get_port(alert.descriptor)
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
  The _SOCKETS::next_error_ method can take an argument specifying the maximum
  number of milliseconds to wait for networking alerts. This feature can be used
  to simulate a sleeping routine where the program stops using any processing
  power for a given amount of time.

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

The library requires a C++17 compliant GNU Compiler and Linux kernel headers.


# License ######################################################################

The SOCKETS library has been authored by Erich Erstu and is released under the
[MIT](LICENSE) license.
