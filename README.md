## Simple-Sockets v0.1

Header-only library that wraps the intricacies of creating sockets on Windows, OSX and Linux platforms. 
Currently it supports IPv4, TCP and blocking sockets (more to come in the near future).

## Usage

First, we need to initialize it:

```c++
if (auto ret = qcstudio::net::init(); !ret) {
    ... // handle the error (str error '(const char*)ret')
}
```

Later, when we do not need it anymore, we should shut it down like this:

```c++
qcstudio::net::shutdown();
```

### Listening Server example

```c++
if (auto server = qcstudio::net::simple_socket().server("*", 8080)) {
    while (true) {
        if (auto [client, caddr, cport] = server.accept(); client) {
            // read/write bytes ...
        }
    }
}
```

### Client socket example

```c++
if (auto client = qcstudio::net::simple_socket().client("localhost", 8080)) {
    // read/write bytes ...
}
```

### Build the echo server example

Please, find a full echo client and server [here](https://github.com/galtza/simple-sockets/blob/master/example/). In order to build it, follow the next bash commands:

```bash
example$ mkdir .build
example$ cd .build
.build$ cmake ..
.build$ make
```

Execute the **_'server'_** first and then the _**'client'**_ and play with it.

## Backlog
- IPv6 
- More platforms
