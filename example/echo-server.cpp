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
 
