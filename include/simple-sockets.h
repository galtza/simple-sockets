/*
    MIT License

    Copyright (c) 2016-2020 Raul Ramos

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

/*
    == Overview ========

    Header-only, simplified sockets class that wraps Windows/OSX/Linux intricacies. 
    At the moment it supports TCP blocking sockets.

    == CLIENT example ========

    if (auto client = qcstudio::net::simple_socket().client("example.com", 80))
    {
        if (auto num = client.write(http_request); num > 0)
        {
            ...
        }
    }

    == SERVER example ========

    auto buffer = std::make_unique<std::uint8_t[]>(BUFFER_SIZE);
    if (auto server = qcstudio::net::simple_socket().server("*", 80))
    {
        while(true)
        {
            if (auto[client, caddr, cport] = server.accept(); client)
            {
                if (auto num = client.read(buffer, BUFFER_SIZE); num > 0)
                {
                    ...
                }
            }
        }
    }
*/

#pragma once

#include <type_traits>
#include <functional>
#include <utility>
#include <map>
#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <tuple>
#include <sstream>

// == Preserve macros ========

#pragma push_macro("TEXT")
#pragma push_macro("WIN32_LEAN_AND_MEAN")
#pragma push_macro("NOMINMAX")
#pragma push_macro("forceinline")

// == 'forceinline' macro ========

#undef forceinline
#if defined(_WIN32)
#   if defined(NDEBUG)
#       define forceinline __forceinline
#       pragma warning(disable : 4714)
#   else
#       define forceinline inline
#   endif
#elif defined (__clang__) || defined(__GNUC__)
#   define forceinline __attribute__((always_inline)) inline
#else
#   define forceinline inline
#endif

// == Platform specific includes  ========

#if defined(_WIN32)

    #pragma warning(disable : 4668)

    #if !defined(WIN32_LEAN_AND_MEAN) && defined(_WINDOWS_)
    #error "<Windows.h> was included before without WIN32_LEAN_AND_MEAN been defined. This is required for <WinSock2.h> to be included and compile."
    #endif

    #undef TEXT
    #undef WIN32_LEAN_AND_MEAN
    #undef NOMINMAX

    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX

    #include <Windows.h>
    #include <WinSock2.h>
    #include <Ws2tcpip.h>
    #pragma comment(lib,"Ws2_32.lib")

#elif defined(__linux__) || defined(__APPLE__) || defined(unix) || defined(__unix__) || defined(__unix)

    #include <unistd.h>
    #include <limits.h>
    #include <fcntl.h>

    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <netdb.h>

#else

    // We assume that there must be a 'socket' function but we do not know how to include it. Detect if user has done it for you, otherwise fail and tell the user what to do.
static inline constexpr char socket(...) { return '0'; }
static_assert(!std::is_same<decltype(socket(1, 2, 3)), char>::value, "You need to include your platform networking headers yourself (Those related to Berkeley sockets API). Please, check your platform documentation.");

#endif

namespace qcstudio {
namespace net {

    /*
        Platform-independent errors
    */
    enum class result_id : std::uint8_t {
        OK = 0,

        UNSUPPORTED_PLATFORM,
        NOT_INITIALIZED,
        DOUBLE_INITIALIZATION,

        PERMISSION_DENIED,
        QUOTA_EXCEEDED,
        MEMORY_ERROR,

        ADDRESS_ALREADY_IN_USE,
        INVALID_ADDRESS,
        INVALID_HOSTNAME,
        INVALID_PARAMETERS,
        INVALID_OPERATION,

        SOCKET_NOT_VALID,
        SOCKET_ALREADY_BOUND,
        SOCKET_ALREADY_CONNECTED,
        SOCKET_NOT_BOUND,
        SOCKET_NOT_CONNECTED,
        SOCKET_CLOSED,
        LISTEN_SOCKET,

        CONNECTION_RESET,
        CONNECTION_REFUSED,

        UNREACHABLE,
        TIMEOUT,
        WOULD_BLOCK,
        TOO_LONG,

        INTERNAL_ERROR,
        UNKNOWN,
    };

    struct result_t {

        result_t(result_id _type) : type(_type) { }
        result_t() : result_t(result_id::UNKNOWN) { }
        explicit operator bool() const {
            return type == result_id::OK;
        }

        result_id type;
    };

    /*
        Initialize network library

        @returns result_t result (OK if succeeded)
    */
    auto init() -> result_t;

    /*
        Shut down network library
    */
    void shutdown();

    /*
        Platform independent socket representation
    */
#ifdef _WIN32
    using underlying_socket_t = SOCKET;
    using underlying_socket_len_t = int;
#else
    using underlying_socket_t = int;
    using underlying_socket_len_t = socklen_t;
#endif

    class simple_socket {

    public:

        /*
            Construct an unspecialised socket, blocking/non-blocking

            This DOES NOT create an underlying socket. It just declares intentions.
            Straightaway after this, Use 'client', 'server' and 'generic' methods

            @param _tcp true for SOCK_STREAM or false for SOCK_DGRAM (defaults to true)
            @param _blocking true to allow to block upon certain operations or false to never block (defaults to false)
        */
        simple_socket(bool _tcp = true, bool _blocking = true);

        /*
            move Constructor

            It transfers the underlying representation
        */
        simple_socket(simple_socket&& _other);

        /*
            Destructor will close the socket file descriptors
        */
        ~simple_socket();

        /*
            'bool' operator used to check the validity of the socket
            @returns true if the socket is valid or false otherwise
        */
        explicit operator bool() const;

        /*
            Wrap server socket creation in a simple call

            It creates the socket, configure it, 'bind', 'listen', etc.
            ONLY callable from rvalues (like in "simple_socket{...}.server(...)")

            @param _addr listening server address
            @param _port listening server port
            @returns rvalue socket valid if it succeeded or invalid on any error (error stored inside the last error)
        */
        auto server(const std::string& _addr, std::uint16_t _port) -> simple_socket&&;

        /*
            Wrap client socket creation in a simple call

            It creates the socket, configure it, 'connect', etc.
            ONLY callable from rvalues (like in "simple_socket{...}.client(...)")

            @param _addr address to connect to
            @param _port port to connect to
            @returns rvalue socket valid if it succeeded or invalid on any error (error stored inside the last error)
        */
        auto client(const std::string& _addr, std::uint16_t _port) && -> simple_socket&&;

        /*
            Retrieve the error of the last operation
        */
        auto get_last_result() const -> result_t;

        /*
            [SERVER] Bind an address to a socket

            @param _addr std::string representing the address to bind the socket to (can be xxx.xxx.xxx.xxx or '*')
            @param _port port on that address
        */
        auto bind(const std::string& _addr, std::uint16_t _port) -> result_t;

        /*
            [SERVER] Listen for connections

            @param _backlog max length of connections queue waiting to be processed
        */
        auto listen(int _backlog) -> result_t;

        /*
            [SERVER] Accept a connection from the connection queue

            @returns a tuple with:
                    (i)   the client socket object (invalid on error),
                    (ii)  the client IP (std::string)
                    (iii) the client port
        */
        auto accept() -> std::tuple<simple_socket, std::string, std::uint16_t>;

        /*
            [CLIENT] Connect the socket to a server

            Make the socket a server socket that listens on passed ip/port. Incompatible with 'listen'

            @param _hostname std::string representing the address to connect the socket to (localhost or xxx.xxx.xxx.xxx)
            @param _port port on that address
            @return true if succeeded or false otherwise (TODO: create platform independent errors)
        */
        auto connect(const std::string& _hostname, std::uint16_t _port) -> result_t;

        /*
            Write data into the socket

            @param _buffer pointer to the buffer with the bytes to send
            @param _length length to be sent
            @return number that if
                    >= 0 indicates the number of bytes sent
                     < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
        */
        template<typename T>
        auto write(const T* _buffer, std::size_t _length) const -> int;

        /*
            Write a string into the socket

            @param _str std-string or c-string to write
            @return number that if
                    >= 0 indicates the number of bytes sent
                     < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
        */
        auto write(const char* _str) const -> int;
        auto write(const std::string& _str) const -> int;

        /*
            Read data from the socket

            @param _buffer pointer to the buffer that will receive the data
            @param _length length we want to receive
            @return number that if
                    == 0 indicates that the connection has been gracefully closed
                     > 0 indicates the number of bytes received
                     < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
        */
        template<typename T>
        auto read(T* _buffer, std::size_t _length) const -> int;

        /*
            Close the socket

            Usually it is closed via destructor

            @returns true if it was closed properly or false in other case
        */
        auto close() -> result_t;

    private:

        simple_socket(bool _tcp, bool _blocking, underlying_socket_t _fd);
        auto binarize_ipv4(const char* _str, void* _dest) -> bool;
        auto stringify_ipv4(const void* _src) -> std::string;
        auto resolve(const std::string& _hostname, int _port, bool _tcp = true, bool _include_ipv6 = false) -> std::vector<std::string>;
        auto set_result(result_t _result) -> simple_socket& { last_result_ = _result; return *this; }

        underlying_socket_t fd_;
        result_t last_result_;
        bool tcp_ : 1;
        bool blocking_ : 1;
    };

// == Implementation ========

forceinline auto init() -> result_t {
#ifdef _WIN32
    auto data = WSADATA();
    if (auto result = WSAStartup(MAKEWORD(2, 2), &data)) {
        if (result == WSAEPROCLIM) {
            return result_t { result_id::QUOTA_EXCEEDED };
        }
        return result_t { result_id::INTERNAL_ERROR };
    }
#endif

    /*
        Check minimal requirements
        1. IPv4 stream / datagram  sockets
    */

    // Check that we can create IP
    {
        for (auto address_family : { AF_INET, AF_INET6 }) {
            for (auto socket_type : { SOCK_STREAM, SOCK_DGRAM }) {
                auto tmp_socket = socket(address_family, socket_type, 0);
    #ifdef _WIN32
                if (tmp_socket == INVALID_SOCKET) {
                    switch (WSAGetLastError()) {
                        case WSAESOCKTNOSUPPORT:
                        case WSAEPROTOTYPE:
                        case WSAEPROTONOSUPPORT:
                        case WSAEINVALIDPROCTABLE:
                        case WSAEINVALIDPROVIDER:
                        case WSAEINVAL:
                        case WSAEAFNOSUPPORT: {
                            shutdown();
                            return result_t { result_id::UNSUPPORTED_PLATFORM };
                        }
                    }
                }
    #else
                if (tmp_socket < 0) {
                    switch (errno) {
                        case EAFNOSUPPORT:
                        case EINVAL :
                        case EPROTONOSUPPORT: {
                            shutdown();
                            return result_t { result_id::UNSUPPORTED_PLATFORM };
                        }
                    }
                }
    #endif
                else {
    #ifdef _WIN32
                    closesocket(tmp_socket);
    #else
                    close(tmp_socket);
    #endif
                }
            }
        }
    }

    return result_t { result_id::OK };
}

forceinline void shutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

forceinline simple_socket::simple_socket(bool /*_tcp*/, bool /*_blocking*/) : last_result_(result_t { result_id::OK }), tcp_(true/*_tcp*/), blocking_(true/*_blocking*/) {

    fd_ = socket(AF_INET, tcp_? SOCK_STREAM : SOCK_DGRAM, 0);
    #ifdef _WIN32
    if (fd_ == INVALID_SOCKET) {
    #else
    if (fd_ < 0) {
    #endif
        switch (fd_) {
            #ifdef _WIN32
                case EACCES: { last_result_ = result_t { result_id::PERMISSION_DENIED }; break; }
            #else
                case EACCES: { last_result_ = result_t { result_id::PERMISSION_DENIED }; break; }
            #endif
        }
        return;
    }

    auto enable = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable));
    #if defined(__APPLE__)
        setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&enable, sizeof(enable));
    #endif

    if (!blocking_) {
        #ifdef _WIN32
            auto mode = 1ul;
            if (ioctlsocket(fd_, FIONBIO, &mode) != 0) {
                return;
            }
        #else
            auto flags = fcntl(fd_, F_GETFL, 0);
            if (flags == -1) {
                //last_error_ = ...
                return;
            }
            auto err = fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
            if (err != 0) {
                //last_error_ = ...
                return;
            }
        #endif
    }

    last_result_ = result_t { result_id::OK };
}

forceinline simple_socket::simple_socket(bool _tcp, bool _blocking, underlying_socket_t _fd) : fd_(_fd), last_result_(result_t { result_id::OK }), tcp_(_tcp), blocking_(_blocking) {
}

forceinline simple_socket::simple_socket(simple_socket&& _other) : fd_(_other.fd_), last_result_(_other.last_result_), tcp_(_other.tcp_), blocking_(_other.blocking_) {
    _other.last_result_ = result_t { result_id::UNKNOWN };
}

forceinline simple_socket::~simple_socket() {
    close();
}

forceinline auto simple_socket::close() -> result_t {
    if (*this) {
    #ifdef _WIN32
        if (closesocket(fd_)) {
            return result_t { result_id::INTERNAL_ERROR };
        }
    #else
        if (::close(fd_)) {
            return result_t { result_id::INTERNAL_ERROR };
        }
    #endif
    }
    return result_t { result_id::OK };
}

forceinline simple_socket::operator bool() const {
    return (bool)last_result_;
}

forceinline auto simple_socket::bind(const std::string& _addr, std::uint16_t _port) -> result_t {

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(_port);
    if (_addr == "*") {
        servaddr.sin_addr.s_addr = INADDR_ANY;
    } else if (!binarize_ipv4(_addr.c_str(), &servaddr.sin_addr)) {
        return last_result_ = result_t { result_id::INVALID_ADDRESS };
    }

    auto result = ::bind(fd_, (sockaddr*)&servaddr, (int)sizeof(servaddr));
    if (result != 0) {
        return result_t { };
    }
    return result_t { result_id::OK };
}

forceinline auto simple_socket::accept() -> std::tuple<simple_socket, std::string, std::uint16_t> {

    sockaddr_in client_addr;
    auto client_len = underlying_socket_len_t { sizeof(client_addr) };
    auto client_underlying_socket = ::accept(fd_, (sockaddr*)&client_addr, &client_len);

    if (
    #ifdef _WIN32
        client_underlying_socket != INVALID_SOCKET
    #else
        client_underlying_socket >= 0
    #endif
        ) {
        return std::make_tuple(simple_socket(tcp_, blocking_, client_underlying_socket), stringify_ipv4(&client_addr.sin_addr), client_addr.sin_port);
    }

    // TODO: Handle errors

    return std::make_tuple<simple_socket, std::string, std::uint16_t>(std::move(simple_socket { }.set_result(result_t { result_id::INTERNAL_ERROR })), { }, { });
}

forceinline auto simple_socket::connect(const std::string& _hostname, std::uint16_t _port) -> result_t {

    sockaddr_in remoteaddr;
    memset(&remoteaddr, 0, sizeof(remoteaddr));

    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(_port);

    auto hostnames = resolve(_hostname, _port, tcp_);
    if (hostnames.size()) {
        auto connect_result =
        #ifdef _WIN32
            SOCKET_ERROR
        #else
            - 1
        #endif
            ;
        for (auto& addr : hostnames) {
            if (binarize_ipv4(addr.c_str(), &remoteaddr.sin_addr)) {
                connect_result = ::connect(fd_, (sockaddr*)&remoteaddr, (underlying_socket_len_t)sizeof(remoteaddr));
                if (connect_result == 0) {
                    return result_t { result_id::OK };
                }
            }
        }
    }

    return last_result_ = result_t { result_id::INVALID_HOSTNAME };
}

forceinline auto simple_socket::listen(int _backlog) -> result_t {

    if (::listen(fd_, _backlog) != 0) {
        // TODO: Fill the error
        //auto err = WSAGetLastError();
        return result_t { };
    }
    return result_t { result_id::OK };
}

#include <exception>

template<typename T>
forceinline auto simple_socket::write(const T* _buffer, std::size_t _length) const -> int {
    auto sent = int { };
#ifdef _WIN32
    sent = ::send(fd_, (const char*)_buffer, (int)_length, 0);
    if (sent == SOCKET_ERROR) {
        // TODO: fill the last error
    }
#else
    sent = ::send(fd_, (const void*)_buffer, (int)_length, 0);
    if (sent < 0) {
        // TODO: fill the last error
    }
    // TODO: fill the last error
#endif
    return sent;
}

forceinline auto simple_socket::write(const char* _str) const -> int {
    return write(_str, strlen(_str));
}

forceinline auto simple_socket::write(const std::string& _str) const -> int {
    return write(_str.c_str(), _str.length());
}

template<typename T>
forceinline auto simple_socket::read(T* _buffer, std::size_t _length) const -> int {
    auto received = int { };
#ifdef _WIN32
    received = ::recv(fd_, (char*)_buffer, (int)_length, 0);
    if (received == SOCKET_ERROR) {
        // TODO: fill the last error
        //auto err = WSAGetLastError();
    }
#else
    received = ::recv(fd_, (void*)_buffer, (int)_length, 0);
    if (received < 0) {
        // TODO: fill the last error
    }
#endif
    return received;
}

forceinline auto simple_socket::get_last_result() const -> result_t {
    // TODO: call the platform function and retrieve all known errors for each platform
    // TODO: create a kindof hash table or something where we associate at compile time the two values (or static)
    return result_t { result_id::OK };
}

auto simple_socket::binarize_ipv4(const char* _str, void* _dest) -> bool {
    if (!_str) return false;
    std::uint8_t buff[4];
    auto pc = _str - 1;
    auto quad = 0, accum = 0;
    while (*++pc != '\0') {
        if (*pc >= '0' && *pc <= '9') {
            accum = accum * 10 + (*pc - '0');
        } else if (*pc == '.') {
            if (quad == 4 || accum > 0xFF) return false;
            else {
                buff[quad++] = (std::uint8_t)accum;
                accum = 0;
            }
        } else return false;
    }
    if (accum > 0xff) return false;
    buff[quad] = (std::uint8_t)accum;
    memcpy(_dest, buff, 4);
    return true;
}

auto simple_socket::stringify_ipv4(const void* _src) -> std::string {
    if (!_src)
        return { "" };

    std::ostringstream ret;
    auto src = (std::uint8_t*)_src;
    for (auto quad = 0; quad < 4; ++quad, src++) {
        auto divider = 100, value = (int)(*src);
        auto tmp = ret.tellp();
        while (divider >= 1) {
            if (auto digit = value / divider) {
                value -= digit * divider;
                ret << char('0' + (char)digit);
            }
            divider /= 10;
        }
        if (ret.tellp() == tmp) {
            ret << '0';
        }
        if (quad < 3) {
            ret << '.';
        }
    }

    return { ret.str() };
}

auto simple_socket::resolve(const std::string& _hostname, int _port, bool _tcp, bool _include_ipv6) -> std::vector<std::string> {

    addrinfo  hints;
    addrinfo* result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = _include_ipv6 ? AF_UNSPEC : AF_INET;
    hints.ai_socktype = _tcp ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;

    auto errcode = getaddrinfo(_hostname.c_str(), std::to_string(_port).c_str(), &hints, &result);
    if (errcode != 0) {
        return { };
    }

    char buffer[256];
    auto ret = std::vector<std::string> { };
    while (result) {
        void* ptr = nullptr;
        switch (result->ai_family) {
            case AF_INET:
            {
                ptr = &((struct sockaddr_in *) result->ai_addr) -> sin_addr;
                break;
            }
            case AF_INET6:
            {
                if (!_include_ipv6) {
                    continue;
                }
                ptr = &((struct sockaddr_in6 *) result->ai_addr) -> sin6_addr;
                break;
            }
        }
        if (ptr) {
            inet_ntop(result->ai_family, ptr, buffer, 256);
            ret.push_back(std::string { buffer });
        }
        result = result->ai_next;
    }

    freeaddrinfo(result);
    return ret;
}

auto simple_socket::server(const std::string& _addr, std::uint16_t _port) -> simple_socket&& {

    if (*this) {
        last_result_ = bind(_addr, _port);
        if (last_result_ && tcp_) { // UDP servers do not listen (connectionless)
            last_result_ = listen(5);
        }
    }
    return std::move(*this);
}

auto simple_socket::client(const std::string& _addr, std::uint16_t _port) && -> simple_socket&& {

    if (*this && tcp_) { // UDP clients do not connect (connectionless)
        last_result_ = connect(_addr, _port);
    }
    return std::move(*this);
}

} // namespace net
} // namespace qcstudio

// == Restore macros ========

#pragma pop_macro("forceinline")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("TEXT")

/*
    TODO:
    - Finish error handling
    - Deal with incomplete data transfers
    - IPv6
    - Non-blocking sockets
    - UDP
    - Broadcast addresses support
    - More platforms support
*/
