#ifndef IPC_STATE_BUFFER_H
#define IPC_STATE_BUFFER_H

#include "ipc/visual_state.h"

#include <atomic>

class StateBuffer {
public:
    void update(const VisualState& s) {
        int next = 1 - currentIndex.load(std::memory_order_relaxed);
        buffers[next] = s;
        std::atomic_thread_fence(std::memory_order_release);
        currentIndex.store(next, std::memory_order_release);
    }

    VisualState get() const {
        int idx = currentIndex.load(std::memory_order_acquire);
        return buffers[idx];
    }

private:
    VisualState buffers[2];
    std::atomic<int> currentIndex{0};
};

#endif // IPC_STATE_BUFFER_H
