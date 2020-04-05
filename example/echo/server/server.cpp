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
*/
#include <iostream>
#include <regex>
#include <memory>
#include "simple-sockets.h"

auto main(int _argc, char* _argv[]) -> int {

    const auto listen_addr = _argc < 2? "*"  : (strcmp("all", _argv[1]) == 0? "*" : _argv[1]);
    const auto listen_port = _argc < 3? 8080 : std::atoi(_argv[2]);;
    const auto buffer_size = 16;

    if (auto ret = qcstudio::net::init(); !ret) {
        std::cout << "Error: could not initialize network system" << std::endl;
        return 1;
    }

    if (auto server = qcstudio::net::simple_socket(buffer_size).server(listen_addr, listen_port)) {

        std::cout << "listening for connections on " << listen_addr << "::" << listen_port << std::endl;
        auto buffer = std::make_unique<std::uint8_t[]>(buffer_size);
        auto alive = true;
        while (alive) {

            if (auto [client, caddr, cport] = server.accept(); client) {
                const auto prompt = "\n" + caddr + "::" + std::to_string(cport) + " --> ";
                auto client_ok = true;
                std::cout << "\n[New connection]" << prompt << std::flush;

                while (client_ok) {
                    if (auto [ line_size, status ] = client.read<uint32_t>(); status > 0) {
                        while (line_size) {
                            if (auto nbytes = client.read(buffer.get(), std::min<int>(buffer_size, line_size), true); nbytes > 0) {
                                auto str = std::string((const char*)buffer.get(), nbytes);
                                std::cout << str.c_str() << std::flush;
                                if (auto pos = str.find("exit server"); pos == 0) {
                                    std::cout << "\n\n[Server closed by client command]" << std::endl;
                                    client_ok = alive = false;
                                    break;
                                }
                                line_size -= nbytes;
                            } else if (auto err = client.get_last_result(); err.is_io_recoverable()) {
                                std::cout << "[Error reading \"" << (const char*)client.get_last_result() << "\"]" << std::flush;
                                break;
                            } else {
                                std::cout << "\n\n[Error reading \"" << (const char*)client.get_last_result() << "\": connection aborted!]\n" << std::flush;
                                client_ok = false;
                                break;
                            }
                        }
                        if (alive) {
                            std::cout << prompt;
                        }
                    } else if (auto err_id = client.get_last_result().type; err_id != qcstudio::net::result_id::ATOMIC_IO_FAILED) {
                        client_ok = false;
                        std::cout << "[Connection " << ((err_id == qcstudio::net::result_id::CONNECTION_CLOSED) ? "closed" : "reset") << "!]\n" << std::flush;
                        break;
                    }
                }
            } else {
                std::cout << "Error: [" << (const char*)client.get_last_result() << "] Client socket creation failed!" << std::endl;
            }
        }
    } else {
        std::cout << "Error: [" << (const char*)server.get_last_result() << "] Server socket creation failed!" << std::endl;
    }

    qcstudio::net::shutdown();

    return 0;
}
 
