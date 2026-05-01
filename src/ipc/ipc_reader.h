#ifndef IPC_READER_H
#define IPC_READER_H

#include "ipc/state_buffer.h"
#include "ipc/ipc_server.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

class IPCReader {
public:
    explicit IPCReader(StateBuffer& buffer);
    ~IPCReader();

    bool start(const std::string& socketPath);
    void stop();

private:
    void run();

    StateBuffer& stateBuffer_;
    std::unique_ptr<IPCServer> server_;
    std::thread workerThread_;
    std::atomic<bool> running_{false};
    std::string socketPath_;
};

#endif // IPC_READER_H
