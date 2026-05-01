#include "ipc/ipc_reader.h"
#include "ipc/ipc_server.h"
#include "ipc/visual_state.h"

#include <cerrno>
#include <iostream>
#include <sys/socket.h>

namespace {
constexpr size_t kBufferSize = 8192;
}

IPCReader::IPCReader(StateBuffer& buffer)
    : stateBuffer_(buffer) {
}

IPCReader::~IPCReader() {
    stop();
}

bool IPCReader::start(const std::string& socketPath) {
    stop();

    server_ = std::make_unique<IPCServer>();
    if (!server_->init(socketPath)) {
        std::cerr << "[IPC] Failed to initialize server on " << socketPath << std::endl;
        server_.reset();
        return false;
    }

    socketPath_ = socketPath;
    running_.store(true, std::memory_order_release);
    workerThread_ = std::thread(&IPCReader::run, this);
    return true;
}

void IPCReader::stop() {
    running_.store(false, std::memory_order_release);
    if (server_) {
        server_->shutdown();
    }
    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void IPCReader::run() {
    while (running_.load(std::memory_order_acquire)) {
        int clientSocket = server_ ? server_->acceptClient() : -1;
        if (clientSocket < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            break;
        }

        std::string pending;
        char buffer[kBufferSize];

        while (running_.load(std::memory_order_acquire)) {
            int received = recv(clientSocket, buffer, static_cast<int>(sizeof(buffer)), 0);
            if (received <= 0) {
                break;
            }

            pending.append(buffer, received);
            size_t newline;
            while ((newline = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (!line.empty()) {
                    VisualState state = parseVisualState(line);
                    stateBuffer_.update(state);
                }
            }
        }

        close(clientSocket);
    }
}
