#include "Logger.hpp"
#include "Taskmaster.hpp"
#include <memory>

int main() {
    std::unique_ptr<Logger> logger;
    
    Logger::Config logger_config {.log_dir = "./logs/", .log_file = "app.log"};
    logger = std::make_unique<Logger>(logger_config);
    
    Taskmaster::Config taskmaster_config {.config_file = ""};
    Taskmaster taskmaster(taskmaster_config, *logger);
    taskmaster.init();
    
    return 0;
}