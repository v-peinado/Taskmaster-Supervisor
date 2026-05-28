#include "Logger.hpp"
#include "Taskmaster.hpp"
#include <memory>

int main() {
    std::unique_ptr<Logger> logger;
    Logger::Config logger_config {};
    logger = std::make_unique<Logger>(logger_config);

    
    return 0;
}