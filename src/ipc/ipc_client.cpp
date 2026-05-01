#include "ipc/ipc_client.h"
#include "ipc/visual_state.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>

bool IPCClient::sendState(const VisualState& state) {
    if (!isConnected() && !connectTo(path_)) {
        return false;
    }

    std::string message = serializeVisualState(state);
    message.push_back('\n');

    const char* ptr = message.data();
    size_t remaining = message.size();

    while (remaining > 0) {
        ssize_t sent = send(socket_, ptr, remaining, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            disconnect();
            return false;
        }
        ptr += sent;
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

bool IPCClient::connectTo(const std::string& path) {
    disconnect();

    path_ = path;
    socket_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_ < 0) {
        return false;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        disconnect();
        return false;
    }

    return true;
}

void IPCClient::disconnect() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
}

IPCClient::~IPCClient() {
    disconnect();
}
