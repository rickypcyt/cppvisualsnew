#include "render_queue.h"

RenderQueue::RenderQueue() {}

RenderQueue::~RenderQueue() {}

void RenderQueue::submit(const RenderCommand& command) {
    commands_.push_back(command);
}

void RenderQueue::submitCommand(const RenderCommand& command) {
    commands_.push_back(command);
}

void RenderQueue::clear() {
    commands_.clear();
}

void RenderQueue::sortCommands() {
    // TODO: Implement command sorting for state batching
    // Sort by shader, then by texture, then by other state
    // This minimizes state changes
}

void RenderQueue::mergeStateChanges() {
    // TODO: Implement state change merging
    // Remove redundant state setting commands
}
