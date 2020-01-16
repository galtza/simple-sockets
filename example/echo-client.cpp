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
#include <cstring>
#include "simple-sockets.h"

auto main(int _argc, char* _argv[]) -> int {

    if (auto ret = qcstudio::net::init(); !ret) {
        return 1;
    }

    const auto server = _argc < 2? "localhost" : _argv[1];
    const auto port   = _argc < 3? 8080        : std::atoi(_argv[2]);
    const auto prompt = std::string(server) + "::" + std::to_string(port) + " <-- ";

    if (auto client = qcstudio::net::simple_socket().client(server, port)) {

        auto alive = true;
        while (alive) {
            std::cout << prompt;
            for (std::string line; std::getline(std::cin, line); ) {
                if (line == "exit") {
                    std::cout << "[Closing local connection]\n" << std::flush;
                    alive = false;
                    break;
                } else if (client.write(line + "\n") < 0)  {
                    std::cout << "[Remote connection reset]\n" << std::flush;
                    alive = false;
                    break;
                }
                std::cout << prompt;
            }
            std::cin.clear(); // [*nix] prevent Ctrl-Z + 'fg' from causing trouble
        }

    } else {
        std::cout << "[" << (int)client.get_last_result().type << "] Client socket creation failed!" << std::endl;
    }

    qcstudio::net::shutdown();

    return 0;
}
