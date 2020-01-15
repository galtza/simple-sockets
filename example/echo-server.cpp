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
#include <iostream>
#include <regex>
#include <memory>
#include "simple-sockets.h"

auto main() -> int {

    const auto saddr = "*";
    const auto sport = 8080;
    const auto buffer_size = 42;
    const auto cr_re = std::regex("\n");

    if (auto ret = qcstudio::net::init(); !ret) {
        std::cout << "Error: could not initialize network system" << std::endl;
        return 1;
    }

    if (auto server = qcstudio::net::simple_socket().server(saddr, sport)) {

        std::cout << "listening for connections on " << saddr << "::" << sport << std::endl;
        auto buffer = std::make_unique<std::uint8_t[]>(buffer_size);

        while (true) {

            if (auto [client, caddr, cport] = server.accept(); client) {
                const auto prompt = "\n" + caddr + "::" + std::to_string(cport) + " --> ";
                std::cout << "\n[New connection]" << prompt << std::flush;

                while (true) {

                    if (auto read_data = client.read(buffer.get(), buffer_size); read_data > 0) {
                        auto str = std::string((const char*)buffer.get(), read_data);
                        std::regex_replace(std::ostreambuf_iterator<char>(std::cout), str.begin(), str.end(), cr_re, prompt);
                        std::cout << std::flush;
                    } else {
                        std::cout << "[Connection " << (read_data == 0 ? "closed" : "failed") << "!]\n";
                        break;
                    }
                }
            }
        }
    } else {
        std::cout << "Error: [" << (int)server.get_last_result().type << "] Server socket creation failed!" << std::endl;
    }

    qcstudio::net::shutdown();

    return 0;
}
 
