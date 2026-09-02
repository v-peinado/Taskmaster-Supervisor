#include "Client.hpp"
#include <iostream>

int main() {
    try {
        Client client("/tmp/taskmaster.sock");
        client.run();
    }
    catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }
}