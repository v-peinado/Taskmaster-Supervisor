#include "Client.hpp"
#include <iostream>

// Constructor

Client::Client(const std::string& path)
    : m_conn(path)
    , m_running(true)
    {}

// Lifecycle

void Client::run() {
    while (m_running) {

        std::optional<std::string> line = m_shell.readLine();

        if (!line) {                      // EOF (Ctrl-D)
            m_running = false;
            break;
        }

        if (line->empty())
            continue;

        if (isLocalCommand(*line))
            continue;

        m_conn.sendCommand(*line);
        m_shell.showResponse(m_conn.readResponse());

        if (*line == "shutdown")
            m_running = false;

        if (m_conn.isClosed()) {
            std::cerr << "connection closed by server" << std::endl;
            m_running = false;
        }
    }
}

// Command handling

bool Client::isLocalCommand(const std::string& line) {
    if (line == "exit") {
        m_running = false;
        return true;
    }
    return false;
}