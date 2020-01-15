#include <iostream>
#include "simple-sockets.h"

auto main() -> int {

    if (auto ret = qcstudio::net::init(); !ret) {
        return 1;
    }

    const auto server = "localhost";
    const auto port   = 8080;
    const auto prompt = std::string(server) + "::" + std::to_string(port) + " <-- ";

    if (auto client = qcstudio::net::simple_socket().client(server, port)) {
        auto alive = true;

        while (alive) {
            std::cout << prompt;
            for (std::string line; std::getline(std::cin, line); ) {
                if (line == "exit") {
                    std::cout << "Closing local connection.\n" << std::flush;
                    alive = false;
                    break;
                } else if (client.write(line + "\n") < 0)  {
                    std::cout << "Remote connection reset.\n" << std::flush;
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
