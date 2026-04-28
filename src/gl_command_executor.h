#ifndef GL_COMMAND_EXECUTOR_H
#define GL_COMMAND_EXECUTOR_H

#include <GL/glew.h>
#include "render_queue.h"
#include <unordered_map>
#include <string>

// Performance metrics for GLCommandExecutor
struct GLPerformanceMetrics {
    int shader_bind_count = 0;
    int vao_bind_count = 0;
    int texture_bind_count = 0;
    int draw_call_count = 0;
    int framebuffer_bind_count = 0;
    int buffer_bind_count = 0;
    int uniform_set_count = 0;
    int total_commands = 0;
    
    // Command type frequency
    std::unordered_map<std::string, int> command_type_counts;
    
    // State repetition tracking
    int consecutive_shader_rebinds = 0;
    int consecutive_vao_rebinds = 0;
    int consecutive_texture_rebinds = 0;
    
    void reset() {
        shader_bind_count = 0;
        vao_bind_count = 0;
        texture_bind_count = 0;
        draw_call_count = 0;
        framebuffer_bind_count = 0;
        buffer_bind_count = 0;
        uniform_set_count = 0;
        total_commands = 0;
        consecutive_shader_rebinds = 0;
        consecutive_vao_rebinds = 0;
        consecutive_texture_rebinds = 0;
        command_type_counts.clear();
    }
};

// OpenGL-specific command executor
// Translates backend-agnostic RenderCommands into OpenGL calls
class GLCommandExecutor {
public:
    GLCommandExecutor();
    ~GLCommandExecutor();

    // Execute all commands in a queue
    void execute(const RenderQueue& queue);

    // Execute single command
    void executeCommand(const RenderCommand& cmd);

    // Performance metrics
    void resetMetrics();
    const GLPerformanceMetrics& getMetrics() const;
    void printMetrics() const;

private:
    // Command type handlers
    void handleBindShader(const RenderCommand& cmd);
    void handleSetUniform1f(const RenderCommand& cmd);
    void handleSetUniform2f(const RenderCommand& cmd);
    void handleSetUniform3f(const RenderCommand& cmd);
    void handleSetUniform4f(const RenderCommand& cmd);
    void handleSetUniform1i(const RenderCommand& cmd);
    void handleBindTexture(const RenderCommand& cmd);
    void handleUnbindTexture(const RenderCommand& cmd);
    void handleBindVertexArray(const RenderCommand& cmd);
    void handleUnbindVertexArray(const RenderCommand& cmd);
    void handleBindBuffer(const RenderCommand& cmd);
    void handleUnbindBuffer(const RenderCommand& cmd);
    void handleDrawArrays(const RenderCommand& cmd);
    void handleBindFramebuffer(const RenderCommand& cmd);
    void handleUnbindFramebuffer(const RenderCommand& cmd);
    void handleClearFramebuffer(const RenderCommand& cmd);
    void handleEnableBlend(const RenderCommand& cmd);
    void handleDisableBlend(const RenderCommand& cmd);
    void handleSetBlendFunc(const RenderCommand& cmd);
    void handleEnableDepthTest(const RenderCommand& cmd);
    void handleDisableDepthTest(const RenderCommand& cmd);
    void handleSetViewport(const RenderCommand& cmd);

    // Performance tracking helpers
    void trackCommandType(const std::string& type);
    
    GLPerformanceMetrics metrics;
    
    // Track current state for redundancy detection
    GLuint current_shader = 0;
    GLuint current_vao = 0;
    GLuint current_texture[16] = {0}; // Support up to 16 texture units
    GLuint current_fbo = 0;
};

#endif // GL_COMMAND_EXECUTOR_H
