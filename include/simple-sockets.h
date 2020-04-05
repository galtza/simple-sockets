/*
    MIT License

    Copyright (c) 2016-2020 Raúl Ramos

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

    == Overview ========

    Header-only, platform-independent, simple IPv4-blocking-TCP socket wrapper

    == CLIENT example ========

    struct msg {
        ...
    };

    if (auto client = qcstudio::net::simple_socket().client("example.com", 321)) {
        if (auto num = client.write<msg>(); num > 0) {
            ...
        }
    }

    == SERVER example ========

    if (auto server = qcstudio::net::simple_socket().server("*", 80)) {
        while(true) {
            if (auto [client, addr, port] = server.accept(); client) {
                if (auto [ msg, status ] = client.read<msg>(); status == 1) {
                    ...
                }
            }
        }
    }

    == Features ========

    ● Client / Server socket wrappers
    ● Buffered write / reads
    ● Atomic write / reads support

    == Notes ========

    ● simple_sockets are NOT thread safe
    ● If the internal buffer's size is too small it might not be able to send POD types atomically
*/

#pragma once

#include <type_traits>
#include <functional>
#include <utility>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <tuple>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <array>

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

    // We assume that there must be a 'socket' function but we do not know how to include it. 
    // Detect if user has done it for you, otherwise fail and tell the user what to do.
    static inline constexpr char socket(...) { return '0'; }
    static_assert(!std::is_same<decltype(socket(1, 2, 3)), char>::value, "You need to include your platform socket API headers yourself. Please, check your platform documentation.");

#endif

// == Platform specific helpers  ========

#if defined(_WIN32)
    #define IS_INVALID_SOCKET(_val) ((_val) == INVALID_SOCKET)
    #define DID_SEND_RECV_CLOSE_FAILED(_val) ((_val) == SOCKET_ERROR)
    #define CLOSE_SOCKET(_socket) closesocket(_socket)
#elif defined(__linux__) || defined(__APPLE__) || defined(unix) || defined(__unix__) || defined(__unix)
    #define IS_INVALID_SOCKET(_val) ((_val) < 0)
    #define DID_SEND_RECV_CLOSE_FAILED(_val) ((_val) < 0)
    #define CLOSE_SOCKET(_socket) ::close(_socket)
#endif

namespace qcstudio {
namespace net {

    enum class result_id : uint8_t {
        OK = 0,

        NOT_INITIALIZED,
        UNSUPPORTED_PLATFORM,

        QUOTA_EXCEEDED,
        MEMORY_ERROR,
        PERMISSION_DENIED,

        ADDRESS_ALREADY_IN_USE,
        INVALID_ADDRESS,
        INVALID_HOSTNAME,
        HOSTNAME_RESOLVE_ERROR,

        SOCKET_NOT_VALID,
        SOCKET_ALREADY_BOUND,
        SOCKET_ALREADY_CONNECTED,
        SOCKET_NOT_BOUND,
        SOCKET_NOT_CONNECTED,
        SOCKET_CLOSED,
        LISTEN_SOCKET,
        NO_LISTEN_SOCKET,

        CONNECTION_CLOSED,
        CONNECTION_RESET,
        CONNECTION_REFUSED,

        UNREACHABLE,
        TIMEOUT,
        TOO_MANY_FILES,

        EMPTY_BUFFER,
        ATOMIC_IO_FAILED,
        ATOMIC_IO_IMPOSSIBLE,

        NET_DOWN,
        INTERNAL_ERROR,
        UNKNOWN,

        COUNT
    };

    struct result_t {
        result_t(result_id _type);
        result_t() = default;
        explicit operator bool() const;
        explicit operator const char*() const;
        auto is_io_recoverable() const -> bool; // assumes that the creation of the socket is correct
        result_id type = result_id::UNKNOWN;
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

    class simple_socket {

    public:
        /*
            Construct an unspecialized socket

            After construction, we can 'specialize' it by via 'client' or 'server' methods.

            (1) @param _buffer_capacity internal buffer size in bytes
            (2) @param _other the other socket to be moved from
        */
        simple_socket(uint32_t _buffer_capacity = 16384); // (1)
        simple_socket(simple_socket&& _other);            // (2)

        /*
            Destructor will close the socket file descriptors
        */
        ~simple_socket();

        /*
            Retrieve the error of the last operation
        */
        auto get_last_result() const -> result_t;

        /*
            'bool' operator used to check the validity of the socket
            @returns true if the socket is valid or false otherwise
        */
        explicit operator bool() const;

        // ==========================================
        // == Some socket creation helpers ==========
        // ==========================================

        /*
            Wrap server socket creation in a simple call

            It creates the socket, 'bind', 'listen', etc.
            ONLY callable from rvalues (like in "simple_socket{...}.server(...)")

            @param _addr listening server address
            @param _port listening server port
            @returns rvalue socket valid if it succeeded or invalid on any error (error stored inside the last error)
        */
        auto server(const std::string& _addr, uint16_t _port) && -> simple_socket&&;

        /*
            Wrap client socket creation in a simple call

            It creates the socket, 'connect', etc.
            ONLY callable from rvalues (like in "simple_socket{...}.client(...)")

            @param _addr address to connect to
            @param _port port to connect to
            @returns rvalue socket valid if it succeeded or invalid on any error (error stored inside the last error)
        */
        auto client(const std::string& _addr, uint16_t _port) && -> simple_socket&&;

        // ============================================
        // == 'Manual' setting-up of sockets ==========
        // ============================================

        /*
            Bind an address to a socket

            @param _addr std::string representing the address to bind the socket to 
                         (can be xxx.xxx.xxx.xxx or '*')
            @param _port port on that address

            Note that this should not be necessary to be called manually if we
            use client / server wrappers.
        */
        auto bind(const std::string& _addr, uint16_t _port) -> result_t;

        /*
            Listen for connections

            @param _backlog max length of connections queue waiting to be processed

            Note that this should not be necessary to be called manually if we
            use client / server wrappers.
        */
        auto listen(int _backlog) -> result_t;

        /*
            Accept a connection from the connection queue

            @returns a tuple with:
                    (i)   the client socket object (invalid on error),
                    (ii)  the client IP (std::string)
                    (iii) the client port
        */
        auto accept() -> std::tuple<simple_socket, std::string, uint16_t>;

        /*
            Connect the socket to a server

            Make the socket a server socket that listens on passed ip/port. Incompatible with 'listen'

            @param _hostname std::string representing the address to connect the socket to (localhost or xxx.xxx.xxx.xxx)
            @param _port port on that address
            @return true if succeeded or false otherwise (TODO: create platform independent errors)
        */
        auto connect(const std::string& _hostname, uint16_t _port) -> result_t;

        /*
            Write a memory buffer into the socket

            As we use internal buffers we need to call flush in order to make sure it is indeed
            sent through the socket. When flush manages to sent it all it returns true

            @param _addr       address of the memory buffer to write
            @param _length     length of the buffer
            @param _atomically if true, It will be FULLY WRITTEN or not at all
            @return < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
                    >= 0 indicates the number of bytes written

            Notice that if _atomically is specified and the internal buffer size is < the _length this will never be written
            Notice that on a preexistent error that prevents IO the error won't change but -1 will be returned
        */
        auto write(const uint8_t* _addr, std::size_t _length, bool _atomically = false) -> int;

        /*
            Atomically write a pod item

            It will be FULLY WRITTEN or not at all. 
            
            As we use internal buffers we need to call flush in order to make sure it is indeed
            sent through the socket. When flush manages to sent it all it returns true

            @param _item reference to the item to be written
            @param _length length to be sent
            @return  < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
                    >= 0 indicates the number of items written (0 or 1)

            Notice that if the internal buffer size is < the length of T this will never be written
            Notice that on a preexistent error that prevents IO the error won't change but -1 will be returned
        */
        template<typename T>
        auto write(const T& _item) -> int;

        /*
            Read data from the socket

            @param _addr       pointer to the memory address that will receive the data
            @param _length     length of the data we want to receive
            @param _atomically if true, It will read the data completely or not at all 
                               (in that case we return -1 and get_last_result returns ATOMIC_IO_FAILED)
            @return < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
                    > 0 indicates the number of bytes received

            Notice that if the internal buffer size is < _length this will never be written
            Notice that on a preexistent error that prevents IO the error won't change but -1 will be returned
        */
        auto read(uint8_t* _addr, std::size_t _length, bool _atomically = false) -> int;

        /*
            fully Read a pod type

            Return a pair with the data as the first field and the read status as the second one.
            Reads of items are never partial; either they work or they do not.

            @return pair with the value and an integer with the following possible values:
                     < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
                    == 1 indicates that the whole data was loaded

            If it cannot be read atomically it will return -1 and the last result will be ATOMIC_IO_FAILED. 

            Notice that if the internal buffer size is < sizeof(T) this will never be written.
            Notice that on a preexistent error that prevents IO the error won't change but -1 will be returned
        */
        template<typename T>
        auto read() -> std::pair<T, int>;

        /*
            Flush the internal write buffer

            @return  < 0 indicates that an error occurred (call get_last_result to retrieve the specific error)
                    == 0 indicates that there is nothing to flush
                     > 0 indicates the number of pending to flush bytes

            Notice that on a preexistent error that prevents IO the error won't change but -1 will be returned
        */
        auto flush() -> int;

        /*
            Close the socket

            Usually it is closed via destructor

            @returns true if it was closed properly or false in other case
        */
        auto close() -> result_t;

    private:

        enum class command : uint8_t {
            cmd_startup, cmd_socket, cmd_all, // common
            cmd_bind, cmd_listen, cmd_accept, // server
            cmd_connect,                      // client
            cmd_send, cmd_recv,               // all sockets
            cmd_closesocket
        };

        static auto translate_platform_error(command _cmd, int64_t _err) -> result_id;
        static auto translate_platform_error(command _cmd) -> result_id;
        friend auto init() -> result_t;

#ifdef _WIN32
        using underlying_socket_t = SOCKET;
        using underlying_socket_len_t = int;
#else
        using underlying_socket_t = int;
        using underlying_socket_len_t = socklen_t;
#endif

        // private construction

        simple_socket(underlying_socket_t _fd, uint32_t _buffer_capacity);

        // utility

        auto binarize_ipv4(const char* _str, void* _dest) -> bool;
        auto stringify_ipv4(const void* _src) -> std::string;
        auto resolve(const std::string& _hostname, int _port) -> std::vector<std::string>;
        auto set_result(result_t _result) && -> simple_socket& { last_result_ = _result; return *this; }

        // buffering related 

        auto alloc_buffers() -> bool;
        auto write_buffer(int _nbuff, const uint8_t* _src, size_t _size, bool _atomically = false) -> uint32_t;
        auto read_buffer(int _nbuff, uint8_t* _dest, size_t _size, bool _atomically = false) -> uint32_t;

        // data

        uint32_t buffer_capacity_;
        underlying_socket_t fd_;
        std::array<std::unique_ptr<uint8_t[]>, 2> buffers_; // 0:wr / 1:rd 
        std::array<uint32_t, 2> buffer_start_, buffer_size_;
        result_t last_result_;
        bool valid_ = false;
    };

// == Implementation ========

#if defined(_WIN32)
    using send_addr_t = const char*;
    using recv_addr_t = char*;
    using send_recv_size_t = int;
#elif defined(__linux__) || defined(__APPLE__) || defined(unix) || defined(__unix__) || defined(__unix)
    using send_addr_t = const void*;
    using recv_addr_t = void*;
    using send_recv_size_t = size_t;
#endif

forceinline auto init() -> result_t {
#ifdef _WIN32
    auto data = WSADATA();
    if (auto err = WSAStartup(MAKEWORD(2, 2), &data)) {
        return simple_socket::translate_platform_error(simple_socket::command::cmd_startup, err);
    }
#endif

    // Check that we can create IP sockets
    auto tmp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_INVALID_SOCKET(tmp_socket)) {
        shutdown();
        return result_id::UNSUPPORTED_PLATFORM;
    } else {
        CLOSE_SOCKET(tmp_socket);
    }

    return result_id::OK;
}

forceinline void shutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

forceinline auto simple_socket::alloc_buffers() -> bool {

    for (auto& buff : buffers_) {
        if (!(buff = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_capacity_]))) {
            for (auto& tmp : buffers_) {
                tmp = nullptr;
            }
            return false;
        }
    }

    for (auto& start : buffer_start_) { start = 0u; }
    for (auto& size  : buffer_size_)  { size  = 0u; }

    return true;
}

forceinline auto simple_socket::write_buffer(int _nbuff, const uint8_t* _src, size_t _size, bool _atomically) -> uint32_t {
    auto& bsize = buffer_size_[_nbuff];
    auto  bstart = buffer_start_[_nbuff];
    auto  b = buffers_[_nbuff].get();
    auto  available = buffer_capacity_ - bsize;

    if (_atomically && _size > available) {
        return 0;
    }

    if (auto wsize = std::min((uint32_t)_size, available)) {
        if (bstart + bsize + wsize <= buffer_capacity_) {
            memcpy(b + bstart, _src, wsize);
        } else {
            auto first_chunk_size = buffer_capacity_ - (bstart + bsize);
            memcpy(b + bstart + bsize, _src, first_chunk_size);
            memcpy(b, _src + first_chunk_size, wsize - first_chunk_size);
        }
        bsize += wsize;
        return wsize;
    }

    return 0;
}

forceinline auto simple_socket::read_buffer(int _nbuff, uint8_t* _dest, size_t _size, bool _atomically) -> uint32_t {
    auto& bsize = buffer_size_[_nbuff];
    if (_atomically && _size > bsize) {
        return 0;
    }

    auto  b = buffers_[_nbuff].get();
    auto& bstart = buffer_start_[_nbuff];
    if (auto rsize = std::min((uint32_t)_size, bsize)) {
        if (bstart + rsize <= buffer_capacity_) {
            memcpy(_dest, reinterpret_cast<void*>(b + bstart), rsize);
        } else {
            auto first_chunk_size = buffer_capacity_ - bstart;
            memcpy(_dest, reinterpret_cast<void*>(b + bstart), first_chunk_size);
            memcpy(_dest + first_chunk_size, reinterpret_cast<void*>(b), rsize - first_chunk_size);
        }
        bstart = (bstart + rsize) % buffer_capacity_;
        bsize -= rsize;
        return rsize;
    }

    return 0;
}

forceinline simple_socket::simple_socket(uint32_t _buffer_capacity) : buffer_capacity_(_buffer_capacity), last_result_(result_id::OK) {

    if (!alloc_buffers()) {
        last_result_ = { result_id::MEMORY_ERROR };
        return;
    }

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (IS_INVALID_SOCKET(fd_)) {
        last_result_ = translate_platform_error(command::cmd_socket);
        return;
    }
    valid_ = true;

#ifndef _WIN32
    auto opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#endif
#if defined(__APPLE__)
    setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, (const char*)&opt, sizeof(opt));
#endif

    last_result_ = result_id::OK;
}

forceinline simple_socket::simple_socket(underlying_socket_t _fd, uint32_t _buffer_capacity) : buffer_capacity_(_buffer_capacity), fd_(_fd), last_result_(result_id::OK) {

    if (!alloc_buffers()) {
        last_result_ = { result_id::MEMORY_ERROR };
    }
}

forceinline simple_socket::simple_socket(simple_socket&& _other) : 
    buffer_capacity_(_other.buffer_capacity_), fd_(_other.fd_),
    buffers_(std::move(_other.buffers_)), buffer_start_(_other.buffer_start_), buffer_size_(_other.buffer_size_),
    last_result_(_other.last_result_) , valid_(true) {
    _other.valid_ = false;
}

forceinline simple_socket::~simple_socket() {
    close();
}

forceinline auto simple_socket::close() -> result_t {
    if (valid_) {
        while (flush() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (last_result_) {
            if (DID_SEND_RECV_CLOSE_FAILED(CLOSE_SOCKET(fd_))) {
                return last_result_ = translate_platform_error(command::cmd_closesocket);
            }
        } else {
            return last_result_;
        }
    }
    valid_ = false;
    return result_id::OK;
}

forceinline simple_socket::operator bool() const {
    return (bool)last_result_;
}

forceinline auto simple_socket::bind(const std::string& _addr, uint16_t _port) -> result_t {

    sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(_port);
    if (_addr == "*") {
        servaddr.sin_addr.s_addr = INADDR_ANY;
    } else if (_addr == "localhost") {
        if (!binarize_ipv4("127.0.0.1", &servaddr.sin_addr)) {
            return last_result_ = result_id::INVALID_ADDRESS;
        }
    } else if (!binarize_ipv4(_addr.c_str(), &servaddr.sin_addr)) {
        return last_result_ = result_id::INVALID_ADDRESS;
    }

    last_result_ = result_id::OK;

    if (::bind(fd_, (sockaddr*)&servaddr, (int)sizeof(servaddr)) != 0) {
        last_result_ = translate_platform_error(command::cmd_bind);
    }

    return last_result_;
}

forceinline auto simple_socket::accept() -> std::tuple<simple_socket, std::string, uint16_t> {

    sockaddr_in client_addr;
    auto client_len = underlying_socket_len_t { sizeof(client_addr) };
    auto client_underlying_socket = ::accept(fd_, (sockaddr*)&client_addr, &client_len);
    if (IS_INVALID_SOCKET(client_underlying_socket)) {
        last_result_ = translate_platform_error(command::cmd_accept);
        return std::make_tuple<simple_socket, std::string, uint16_t>(std::move(simple_socket { }.set_result(last_result_)), { }, { });
    }

    return std::make_tuple(
        simple_socket(client_underlying_socket, buffer_capacity_), // use the same buffer size for clients
        stringify_ipv4(&client_addr.sin_addr),
        client_addr.sin_port
    );
}

forceinline auto simple_socket::connect(const std::string& _hostname, uint16_t _port) -> result_t {

    sockaddr_in remoteaddr;
    memset(&remoteaddr, 0, sizeof(remoteaddr));

    remoteaddr.sin_family = AF_INET;
    remoteaddr.sin_port = htons(_port);

    auto hostnames = resolve(_hostname, _port);
    if (hostnames.size()) {
        last_result_ = result_t{}; // assume failure
        for (auto& addr : hostnames) {
            if (binarize_ipv4(addr.c_str(), &remoteaddr.sin_addr)) {
                auto connect_result = ::connect(fd_, (sockaddr*)&remoteaddr, (underlying_socket_len_t)sizeof(remoteaddr));
                if (connect_result == 0) {
                    last_result_ = result_id::OK;
                } else {
                    last_result_ = translate_platform_error(command::cmd_connect);
                }
                break;
            }
        }
    } else {
        last_result_ = result_id::HOSTNAME_RESOLVE_ERROR;
    }

    return last_result_;
}

forceinline auto simple_socket::listen(int _backlog) -> result_t {

    if (::listen(fd_, _backlog) != 0) {
        last_result_ = translate_platform_error(command::cmd_listen);
    }
    return last_result_;
}

template<typename T>
forceinline auto simple_socket::write(const T& _item) -> int {
    static_assert(std::is_pod<T>::value, "This method requires a POD type");
    auto n = write((const uint8_t*)&_item, sizeof(_item), true);
    return n <= 0 ? n : 1;
}

forceinline auto simple_socket::write(const uint8_t* _addr, std::size_t _length, bool _atomically) -> int {

    if (!last_result_.is_io_recoverable()) {
        return -1;
    }

    if (!_addr || _length == 0) {
        last_result_ = result_id::EMPTY_BUFFER;
        return -1;
    }

    last_result_ = result_id::OK;
    if (!flush() && !last_result_) {
        return -1;
    }

    if (_atomically) {
        if (buffer_capacity_ < _length) {
            last_result_ = result_id::ATOMIC_IO_IMPOSSIBLE;
            return -1;
        } else if ((buffer_capacity_ - buffer_size_[0]) < _length) {
            last_result_ = result_id::ATOMIC_IO_FAILED;
            return -1;
        }
    }

    if (buffer_size_[0] > 0) {
        return write_buffer(0, _addr, _length);
    } else {
        auto nbytes = ::send(fd_, (send_addr_t)_addr, (send_recv_size_t)_length, 0);
        if (DID_SEND_RECV_CLOSE_FAILED(nbytes)) {
            last_result_ = translate_platform_error(command::cmd_send);
            return -1;
        }

        if ((size_t)nbytes < _length) {
            nbytes += write_buffer(0, _addr + nbytes, _length - nbytes);
        }
        return nbytes;
    }
}

template<typename T>
forceinline auto simple_socket::read() -> std::pair<T, int> {
    static_assert(std::is_pod<T>::value, "POD type required");

    std::pair<T, int> ret{ {}, -1 };
    if (read((uint8_t*)&ret.first, sizeof(T), true) > 0) {
        ret.second = 1;
    }

    return ret;
}

forceinline auto simple_socket::read(uint8_t* _addr, std::size_t _length, bool _atomically) -> int {

    if (!last_result_.is_io_recoverable()) {
        return -1;
    }

    auto needed = (int64_t)_length - buffer_size_[1];
    if (needed <= 0) {
        read_buffer(1, _addr, _length);
        return (int)_length;
    }

    auto available = buffer_capacity_ - buffer_size_[1];
    if (_atomically) {
        if (buffer_capacity_ < _length) {
            last_result_ = result_id::ATOMIC_IO_IMPOSSIBLE;
            return -1;
        } else if (available < needed) {
            last_result_ = result_id::ATOMIC_IO_FAILED;
            return -1;
        }
    }

    auto rq_size = std::min<int64_t>(available, needed);
    auto nbytes = ::recv(fd_, (recv_addr_t)(_addr + buffer_size_[1]), (send_recv_size_t)rq_size, _atomically? MSG_WAITALL : 0);
    if (DID_SEND_RECV_CLOSE_FAILED(nbytes)) {
        last_result_ = translate_platform_error(command::cmd_recv);
        return -1;
    } else if (nbytes == 0) {
        last_result_ = result_id::CONNECTION_CLOSED;
        return -1;
    }
    if (_atomically && nbytes != rq_size) {
        write_buffer(1, _addr + buffer_size_[1], nbytes); // at this point it is guaranteed that the buffer has room enough for the data we just read
        last_result_ = result_id::ATOMIC_IO_FAILED;
        return -1;
    }

    nbytes += buffer_size_[1];
    read_buffer(1, _addr, buffer_size_[1]);
    return nbytes;
}

forceinline auto simple_socket::flush() -> int {

    if (!last_result_.is_io_recoverable()) {
        return -1;
    }

    auto& bsize = buffer_size_[0];
    if (bsize) {
        auto& bstart = buffer_start_[0];
        auto  b = buffers_[0].get();
        auto  sent = 0;

        if (bstart + bsize <= buffer_capacity_) {
            sent = ::send(fd_, (send_addr_t)(b + bstart), (send_recv_size_t)bsize, 0);
            if (DID_SEND_RECV_CLOSE_FAILED(sent)) {
                last_result_ = translate_platform_error(command::cmd_send);
                return -1;
            }
        } else {
            auto first_chunk_size = buffer_capacity_ - bstart;
            sent = ::send(fd_, (send_addr_t)(b + bstart), (send_recv_size_t)first_chunk_size, 0);
            if (DID_SEND_RECV_CLOSE_FAILED(sent)) {
                last_result_ = translate_platform_error(command::cmd_send);
                return -1;
            }

            if (auto second_chung_size = (bsize - first_chunk_size)) {
                auto tmp = ::send(fd_, (send_addr_t)b, (send_recv_size_t)second_chung_size, 0);
                if (DID_SEND_RECV_CLOSE_FAILED(tmp)) {
                    bstart = (bstart + sent) % buffer_capacity_;
                    bsize -= sent;
                    last_result_ = translate_platform_error(command::cmd_send);
                    return -1;
                }
                sent += tmp;
            }
        }
        bstart = (bstart + sent) % buffer_capacity_;
        bsize -= sent;
    }

    return bsize;
}

forceinline auto simple_socket::get_last_result() const -> result_t {
    return last_result_;
}

forceinline auto simple_socket::binarize_ipv4(const char* _str, void* _dest) -> bool {
    if (!_str) return false;
    uint8_t buff[4];
    auto pc = _str - 1;
    auto quad = 0, accum = 0;
    while (*++pc != '\0') {
        if (*pc >= '0' && *pc <= '9') {
            accum = accum * 10 + (*pc - '0');
        } else if (*pc == '.') {
            if (quad == 4 || accum > 0xFF) return false;
            else {
                buff[quad++] = (uint8_t)accum;
                accum = 0;
            }
        } else return false;
    }
    if (accum > 0xff) return false;
    buff[quad] = (uint8_t)accum;
    memcpy(_dest, buff, 4);
    return true;
}

forceinline auto simple_socket::stringify_ipv4(const void* _src) -> std::string {
    if (!_src)
        return { "" };

    std::ostringstream ret;
    auto src = (uint8_t*)_src;
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

forceinline auto simple_socket::resolve(const std::string& _hostname, int _port) -> std::vector<std::string> {

    addrinfo  hints;
    addrinfo* result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    auto errcode = getaddrinfo(_hostname.c_str(), std::to_string(_port).c_str(), &hints, &result);
    if (errcode != 0) {
        return { };
    }

    auto buffer = std::array<char, 32> { };
    auto ret = std::vector<std::string> { };
    while (result) {
        if (inet_ntop(result->ai_family, &((struct sockaddr_in *)result->ai_addr)->sin_addr, &buffer[0], buffer.size())) {
            ret.push_back(std::string{ &buffer[0] });
        }
        result = result->ai_next;
    }

    freeaddrinfo(result);
    return ret;
}

forceinline auto simple_socket::server(const std::string& _addr, uint16_t _port) && -> simple_socket&& {

    if (*this) {
        last_result_ = bind(_addr, _port);
        if (last_result_) {
            last_result_ = listen(5);
        }
    }
    return std::move(*this);
}

forceinline auto simple_socket::client(const std::string& _addr, uint16_t _port) && -> simple_socket&& {

    if (*this) {
        last_result_ = connect(_addr, _port);
    }
    return std::move(*this);
}

forceinline auto simple_socket::translate_platform_error(command _cmd) -> result_id {
#ifdef _WIN32
    int64_t err = WSAGetLastError();
#else
    int64_t err = errno;
#endif
    return translate_platform_error(_cmd, err);
}

forceinline auto simple_socket::translate_platform_error(command _cmd, int64_t _err) -> result_id {
#pragma push_macro("_")
#undef _
#define _(_name, _id) { command::cmd_##_name, result_id::_id }

    static auto ret = std::unordered_map<int64_t, std::unordered_map<command, result_id>> {
#ifdef _WIN32
        { WSASYSNOTREADY,     { _(startup, INTERNAL_ERROR)          }},
        { WSAVERNOTSUPPORTED, { _(startup, UNSUPPORTED_PLATFORM)    }},
        { WSAEPROCLIM,        { _(startup, QUOTA_EXCEEDED)          }},

        { WSAEMFILE,          { _(socket, TOO_MANY_FILES)           }},

        { WSAEADDRNOTAVAIL,   { _(bind, INVALID_ADDRESS)            }},
        { WSAEINVAL,          { _(bind, SOCKET_ALREADY_BOUND)       }},

        { WSAEINVAL,          { _(listen, SOCKET_NOT_BOUND)         }},
        { WSAEMFILE,          { _(listen, TOO_MANY_FILES)           }},
        { WSAEOPNOTSUPP,      { _(listen, NO_LISTEN_SOCKET)         }},
        
        { WSAEINVAL,          { _(accept, NO_LISTEN_SOCKET)         }},
        { WSAEMFILE,          { _(accept, TOO_MANY_FILES)           }},
        { WSAECONNRESET,      { _(accept, CONNECTION_RESET)         }},

        { WSAECONNREFUSED,    { _(connect, CONNECTION_REFUSED)      }},
        { WSAENETUNREACH,     { _(connect, NET_DOWN)                }},
        { WSAEADDRNOTAVAIL,   { _(connect, INVALID_ADDRESS)         }},
        { WSAEHOSTUNREACH,    { _(connect, UNREACHABLE)             }},
        { WSAETIMEDOUT,       { _(connect, TIMEOUT)                 }},
        { WSAEINVAL,          { _(connect, LISTEN_SOCKET)           }},
        { WSAEISCONN,         { _(connect, SOCKET_ALREADY_CONNECTED)}},

        { WSAEHOSTUNREACH,    { _(send, UNREACHABLE)                }},
        { WSAETIMEDOUT,       { _(send, TIMEOUT)                    }},
        { WSAEINVAL,          { _(send, SOCKET_NOT_BOUND)           }},
        { WSAENETRESET,       { _(send, CONNECTION_RESET)           }},
        { WSAENOTCONN,        { _(send, SOCKET_NOT_CONNECTED)       }},
        { WSAESHUTDOWN,       { _(send, SOCKET_CLOSED)              }},
        { WSAECONNABORTED,    { _(send, CONNECTION_RESET)           }},
        { WSAECONNRESET,      { _(send, CONNECTION_RESET)           }},

        { WSAETIMEDOUT,       { _(recv, TIMEOUT)                    }},
        { WSAEINVAL,          { _(recv, SOCKET_NOT_BOUND)           }},
        { WSAENETRESET,       { _(recv, CONNECTION_RESET)           }},
        { WSAENOTCONN,        { _(recv, SOCKET_NOT_CONNECTED)       }},
        { WSAESHUTDOWN,       { _(recv, SOCKET_CLOSED)              }},
        { WSAECONNABORTED,    { _(recv, CONNECTION_RESET)           }},
        { WSAECONNRESET,      { _(recv, CONNECTION_RESET)           }},

        { WSANOTINITIALISED,  { _(all, NOT_INITIALIZED)             }},
        { WSAENETDOWN,        { _(all, NET_DOWN)                    }},
        { WSAENOBUFS,         { _(all, MEMORY_ERROR)                }},
        { WSAENOTSOCK,        { _(all, SOCKET_NOT_VALID)            }},
        { WSAEADDRINUSE,      { _(all, ADDRESS_ALREADY_IN_USE)      }},
#else
        { EADDRINUSE,    { _(bind, ADDRESS_ALREADY_IN_USE),     } },
        { EADDRNOTAVAIL, { _(bind, INVALID_ADDRESS),            } },
        { ELOOP,         { _(bind, INVALID_ADDRESS),            } },
        { ENAMETOOLONG,  { _(bind, INVALID_ADDRESS),            } },
        { EINVAL,        { _(bind, SOCKET_ALREADY_BOUND),       } },
        { EISDIR,        { _(bind, INVALID_ADDRESS),            } },
        { EDESTADDRREQ,  { _(bind, SOCKET_NOT_VALID),           } },

        { EINVAL,        { _(listen, SOCKET_ALREADY_CONNECTED), } },
        { EADDRINUSE,    { _(listen, SOCKET_ALREADY_CONNECTED), } },
        { EDESTADDRREQ,  { _(listen, SOCKET_NOT_BOUND),         } },
        { EOPNOTSUPP,    { _(listen, NO_LISTEN_SOCKET),         } },

        { ECONNABORTED,  { _(accept, CONNECTION_RESET),         } },
        { EINTR,         { _(accept, CONNECTION_RESET),         } },
        { EINVAL,        { _(accept, NO_LISTEN_SOCKET),         } },

        { EADDRINUSE,    { _(connect, ADDRESS_ALREADY_IN_USE),  } },
        { ECONNREFUSED,  { _(connect, CONNECTION_REFUSED),      } },
        { EISCONN,       { _(connect, SOCKET_ALREADY_CONNECTED),} },
        { ETIMEDOUT,     { _(connect, TIMEOUT),                 } },
        { EADDRNOTAVAIL, { _(connect, INVALID_ADDRESS),         } },
        { EHOSTUNREACH,  { _(connect, UNREACHABLE),             } },
        { ENAMETOOLONG,  { _(connect, INVALID_ADDRESS),         } },
        { EAFNOSUPPORT,  { _(connect, INVALID_ADDRESS),         } },
        { EOPNOTSUPP,    { _(connect, LISTEN_SOCKET),           } },
        { EPROTOTYPE,    { _(connect, INVALID_ADDRESS),         } },

        { EDESTADDRREQ,  { _(send, SOCKET_NOT_VALID),           } },
        { ENOTCONN,      { _(send, SOCKET_NOT_CONNECTED),       } },
        { EPIPE,         { _(send, CONNECTION_RESET),           } },
        { EHOSTUNREACH,  { _(send, UNREACHABLE),                } },
        { ECONNREFUSED,  { _(send, CONNECTION_REFUSED),         } },
        { EHOSTDOWN,     { _(send, UNREACHABLE),                } },
        { ENETDOWN,      { _(send, NET_DOWN),                   } },

        { ENOTCONN,      { _(recv, SOCKET_NOT_CONNECTED),       } },
        { ECONNREFUSED,  { _(recv, CONNECTION_REFUSED),         } },
        { ETIMEDOUT,     { _(recv, TIMEOUT),                    } },

        { EACCES,        { _(all, PERMISSION_DENIED),           } },
        { EPERM,         { _(all, PERMISSION_DENIED),           } },
        { ENOBUFS,       { _(all, MEMORY_ERROR),                } },
        { ENOMEM,        { _(all, MEMORY_ERROR),                } },
        { EMFILE,        { _(all, TOO_MANY_FILES),              } },
        { ENFILE,        { _(all, TOO_MANY_FILES),              } },
        { EBADF,         { _(all, SOCKET_NOT_VALID),            } },
        { ENOTSOCK,      { _(all, SOCKET_NOT_VALID),            } },
        { EROFS,         { _(all, PERMISSION_DENIED),           } }, // note: fd_ is read-only
        { ECONNRESET,    { _(all, CONNECTION_RESET),            } },
        { EINTR,         { _(all, INTERNAL_ERROR),              } },
        { EIO,           { _(all, INTERNAL_ERROR),              } },
        { ENOSPC,        { _(all, INTERNAL_ERROR),              } },
        { EDQUOT,        { _(all, INTERNAL_ERROR),              } },
        { ENOENT,        { _(all, SOCKET_NOT_VALID),            } },
        { ENOTDIR,       { _(all, SOCKET_NOT_VALID),            } },
        { ENETUNREACH,   { _(all, NET_DOWN),                    } },
#endif
};

    auto it_err = ret.find(_err);
    if (it_err != ret.end()) {
        auto it_cmd = it_err->second.find(_cmd);
        if (it_cmd == it_err->second.end()) {
            it_cmd = it_err->second.find(command::cmd_all);
        }
        if (it_cmd != it_err->second.end()) {
            return it_cmd->second;
        }
    }
#pragma pop_macro("_")
    return result_id::UNKNOWN;
}

forceinline result_t::result_t(result_id _type) : type(_type) { 
}

forceinline result_t::operator bool() const {
    return type == result_id::OK;
}

forceinline result_t::operator const char*() const {
    static_assert((int)result_id::COUNT == 30, "Enum changed. Please, update this function accordingly.");
    switch (type) {
        case result_id::OK:                         return "OK";
        case result_id::NOT_INITIALIZED:            return "NOT_INITIALIZED";
        case result_id::UNSUPPORTED_PLATFORM:       return "UNSUPPORTED_PLATFORM";
        case result_id::QUOTA_EXCEEDED:             return "QUOTA_EXCEEDED";
        case result_id::MEMORY_ERROR:               return "MEMORY_ERROR";
        case result_id::PERMISSION_DENIED:          return "PERMISSION_DENIED";
        case result_id::ADDRESS_ALREADY_IN_USE:     return "ADDRESS_ALREADY_IN_USE";
        case result_id::INVALID_ADDRESS:            return "INVALID_ADDRESS";
        case result_id::INVALID_HOSTNAME:           return "INVALID_HOSTNAME";
        case result_id::HOSTNAME_RESOLVE_ERROR:     return "HOSTNAME_RESOLVE_ERROR";
        case result_id::SOCKET_NOT_VALID:           return "SOCKET_NOT_VALID";
        case result_id::SOCKET_ALREADY_BOUND:       return "SOCKET_ALREADY_BOUND";
        case result_id::SOCKET_ALREADY_CONNECTED:   return "SOCKET_ALREADY_CONNECTED";
        case result_id::SOCKET_NOT_BOUND:           return "SOCKET_NOT_BOUND";
        case result_id::SOCKET_NOT_CONNECTED:       return "SOCKET_NOT_CONNECTED";
        case result_id::SOCKET_CLOSED:              return "SOCKET_CLOSED";
        case result_id::LISTEN_SOCKET:              return "LISTEN_SOCKET";
        case result_id::NO_LISTEN_SOCKET:           return "NO_LISTEN_SOCKET";
        case result_id::CONNECTION_CLOSED:          return "CONNECTION_CLOSED";
        case result_id::CONNECTION_RESET:           return "CONNECTION_RESET";
        case result_id::CONNECTION_REFUSED:         return "CONNECTION_REFUSED";
        case result_id::UNREACHABLE:                return "UNREACHABLE";
        case result_id::TIMEOUT:                    return "TIMEOUT";
        case result_id::EMPTY_BUFFER:               return "EMPTY_BUFFER";
        case result_id::ATOMIC_IO_FAILED:           return "ATOMIC_IO_FAILED";
        case result_id::ATOMIC_IO_IMPOSSIBLE:       return "ATOMIC_IO_IMPOSSIBLE";
        case result_id::TOO_MANY_FILES:             return "TOO_MANY_FILES";
        case result_id::INTERNAL_ERROR:             return "INTERNAL_ERROR";
        case result_id::NET_DOWN:                   return "NET_DOWN";
        case result_id::UNKNOWN:                    return "UNKNOWN";
        default:
            return "not defined";
    }
}

forceinline auto result_t::is_io_recoverable () const -> bool {
    static_assert((int)result_id::COUNT == 30, "Enum changed. Please, update this function accordingly.");
    switch (type) {
        case result_id::MEMORY_ERROR:
        case result_id::SOCKET_NOT_CONNECTED:
        case result_id::SOCKET_CLOSED:
        case result_id::CONNECTION_CLOSED:
        case result_id::CONNECTION_RESET:
        case result_id::UNREACHABLE:
        case result_id::TIMEOUT:
        case result_id::ATOMIC_IO_IMPOSSIBLE:
        case result_id::NET_DOWN:
        case result_id::UNKNOWN:
            return false;
        default:
            return true;
    }
}

} // namespace net
} // namespace qcstudio

// == Restore macros ========

#pragma pop_macro("forceinline")
#pragma pop_macro("NOMINMAX")
#pragma pop_macro("WIN32_LEAN_AND_MEAN")
#pragma pop_macro("TEXT")

