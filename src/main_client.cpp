#include "ServerConnection.hpp"
#include <iostream>
#include <string>

int main() {
    try {
        ServerConnection conn("/tmp/taskmaster.sock");
        std::string line;

        while (true) {
            std::cout << "taskmaster> " << std::flush;

            if (!std::getline(std::cin, line))   // EOF (Ctrl-D)
                break;

            if (line.empty())
                continue;

            conn.sendCommand(line);
            if (conn.isClosed()) {
                std::cerr << "connection lost" << std::endl;
                break;
            }

            std::string response = conn.readResponse();
            std::cout << response;

            if (conn.isClosed()) {
                std::cerr << "connection closed by server" << std::endl;
                break;
            }

            if (line == "quit")
                break;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
}