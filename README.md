## Simple-Sockets v0.1

Header-only library that wraps the intricacies of creating sockets on Windows, OSX and Linux platforms. 
Currently it supports IPv4, TCP and blocking sockets (more to come in the near future).

## Usage

First, we need to initialize it:

```c++
if (auto ret = qcstudio::net::init(); !ret) {
    ... // print error or return or do whatever you need to do
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
            client.write("hi there!");
        }
    }
}
```

### Client socket example

```c++
if (auto client = qcstudio::net::simple_socket().client("localhost", 8080)) {
    auto hello = "hello world";
    client.write("hello world");
    ...
}
```

### Build the echo server example

Please, find a full echo client and server [here](https://github.com/galtza/simple-sockets/blob/master/example/). In dorder to build it, follow the next bash commands:

```bash
example$ mkdir .build
example$ cd .build
.build$ cmake ..
.build$ make
```

Execute the **_'server'_** first and then the _**'client'**_ and play with it.

## Backlog
- Finish generic error handling (work in progress)
- Deal with incomplete data while writing / reading (work in progress)
- IPv6 
- Non blocking sockets
- UDP
- Broadcast address support
- More platforms
