#ifndef RENDER_COMMAND_H
#define RENDER_COMMAND_H

#include <string>
#include <vector>
#include <array>
#include "shader_handle.h"

// Backend-agnostic render commands
// This allows the same render logic to execute on OpenGL, Vulkan, or other backends
struct RenderCommand {
    enum class Type {
        // Shader commands
        BindShader,
        SetUniform1f,
        SetUniform2f,
        SetUniform3f,
        SetUniform4f,
        SetUniform1i,
        SetUniformMatrix4f,

        // Texture commands
        BindTexture,
        UnbindTexture,

        // Buffer commands
        BindVertexArray,
        UnbindVertexArray,
        BindBuffer,
        UnbindBuffer,

        // Drawing commands
        DrawArrays,
        DrawElements,
        DrawIndexed,

        // Framebuffer commands
        BindFramebuffer,
        UnbindFramebuffer,
        ClearFramebuffer,

        // State commands
        EnableBlend,
        DisableBlend,
        SetBlendFunc,
        EnableDepthTest,
        DisableDepthTest,
        SetViewport,

        // Other
        Custom
    };

    Type type;

    union {
        // Shader data
        struct {
            ShaderHandle handle;
        } bindShader;

        struct {
            ShaderHandle handle;
            int location;
            float value1;
        } setUniform1f;

        struct {
            ShaderHandle handle;
            int location;
            float value1;
            float value2;
        } setUniform2f;

        struct {
            ShaderHandle handle;
            int location;
            float value1;
            float value2;
            float value3;
        } setUniform3f;

        struct {
            ShaderHandle handle;
            int location;
            float value1;
            float value2;
            float value3;
            float value4;
        } setUniform4f;

        struct {
            ShaderHandle handle;
            int location;
            int value;
        } setUniform1i;

        // Texture data
        struct {
            unsigned int textureId;
            int slot;
        } bindTexture;

        // Buffer data
        struct {
            unsigned int vaoId;
        } bindVertexArray;

        struct {
            unsigned int bufferId;
            int target;
        } bindBuffer;

        // Drawing data
        struct {
            int mode;
            int first;
            int count;
        } drawArrays;

        // Framebuffer data
        struct {
            unsigned int fboId;
        } bindFramebuffer;

        struct {
            float r, g, b, a;
        } clearFramebuffer;

        // State data
        struct {
            int x, y, width, height;
        } setViewport;

        struct {
            int sfactor, dfactor;
        } setBlendFunc;
    };

    // For string data (uniform names, etc.)
    std::string stringData;

    // For matrix data
    std::array<float, 16> matrixData;
};

#endif // RENDER_COMMAND_H
