#include "Taskmaster.hpp"
#include "Logger.hpp"

// Taskmaster - Constructors/Destructors

Taskmaster::Taskmaster(const Config& cfg, Logger& logger)
    : m_config_file(cfg.config_file)
    , m_logger(logger) {}

// Logger - Public meths

void Taskmaster::init(){
    m_logger.log(Logger::LogLevel::Info, "Taskmaster is running");
}