#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include "ipc/visual_state.h"

#include <string>

class IPCClient {
public:
    IPCClient() = default;
    ~IPCClient();

    bool sendState(const VisualState& state);
    bool connectTo(const std::string& path);
    void disconnect();
    bool isConnected() const { return socket_ >= 0; }

private:
    int socket_ = -1;
    std::string path_;
};

#endif // IPC_CLIENT_H
