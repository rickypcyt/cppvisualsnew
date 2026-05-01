#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

class IPCServer {
public:
    ~IPCServer() { shutdown(); }

    bool init(const std::string& path) {
        shutdown();
        path_ = path;

        sock_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock_ < 0) {
            return false;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);
        unlink(path_.c_str());

        if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            shutdown();
            return false;
        }

        if (listen(sock_, 1) < 0) {
            shutdown();
            return false;
        }

        return true;
    }

    int acceptClient() const {
        if (sock_ < 0) {
            return -1;
        }
        return accept(sock_, nullptr, nullptr);
    }

    void shutdown() {
        if (sock_ >= 0) {
            close(sock_);
            sock_ = -1;
        }
        if (!path_.empty()) {
            unlink(path_.c_str());
        }
    }

private:
    int sock_ = -1;
    std::string path_;
};

#endif // IPC_SERVER_H
