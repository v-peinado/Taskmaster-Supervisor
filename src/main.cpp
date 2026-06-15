#include "Logger.hpp"
#include "Taskmaster.hpp"
#include <memory>
#include <string>
#include <unistd.h>

int main(int argc, char **argv) {
    if(argc != 2) {
        std::cout << "./taskmaster [valid config file]" << std::endl;
        return 1;
    }
    std::unique_ptr<Logger> logger;
    
    Logger::Config logger_config {.log_dir = "./logs/", .log_file = "app.log"};
    logger = std::make_unique<Logger>(logger_config);
    
    Taskmaster::Config taskmaster_config {.config_file = argv[1]};
    Taskmaster taskmaster(taskmaster_config, *logger);
    taskmaster.init();

    pause(); // para que no se cierre

    return 0;
}