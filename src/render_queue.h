#ifndef RENDER_QUEUE_H
#define RENDER_QUEUE_H

#include <vector>
#include <memory>
#include "render_command.h"

// RenderQueue collects render commands from the app/modules
// The backend then executes them in order (with potential batching/sorting)
class RenderQueue {
public:
    RenderQueue();
    ~RenderQueue();

    // Command submission
    void submit(const RenderCommand& command);
    void submitCommand(const RenderCommand& command);

    // Queue management
    void clear();
    size_t size() const { return commands_.size(); }
    bool empty() const { return commands_.empty(); }

    // Command access (for backend executor)
    const std::vector<RenderCommand>& getCommands() const { return commands_; }
    std::vector<RenderCommand>& getCommands() { return commands_; }

    // Batching helpers (optional optimization)
    void sortCommands();
    void mergeStateChanges();

private:
    std::vector<RenderCommand> commands_;
};

#endif // RENDER_QUEUE_H
