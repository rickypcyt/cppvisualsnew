#include "gl_command_executor.h"
#include <iostream>
#include <algorithm>
#include <vector>

GLCommandExecutor::GLCommandExecutor() {
    for (int i = 0; i < 16; i++) {
        current_texture[i] = 0;
    }
}

GLCommandExecutor::~GLCommandExecutor() {}

void GLCommandExecutor::execute(const RenderQueue& queue) {
    const auto& commands = queue.getCommands();
    for (const auto& cmd : commands) {
        executeCommand(cmd);
    }
}

void GLCommandExecutor::resetMetrics() {
    metrics.reset();
    current_shader = 0;
    current_vao = 0;
    for (int i = 0; i < 16; i++) {
        current_texture[i] = 0;
    }
    current_fbo = 0;
}

const GLPerformanceMetrics& GLCommandExecutor::getMetrics() const {
    return metrics;
}

void GLCommandExecutor::printMetrics() const {
    std::cout << "=== GL Performance Metrics ===" << std::endl;
    std::cout << "Total Commands: " << metrics.total_commands << std::endl;
    std::cout << "Draw Calls: " << metrics.draw_call_count << std::endl;
    std::cout << "Shader Binds: " << metrics.shader_bind_count << std::endl;
    std::cout << "VAO Binds: " << metrics.vao_bind_count << std::endl;
    std::cout << "Texture Binds: " << metrics.texture_bind_count << std::endl;
    std::cout << "Framebuffer Binds: " << metrics.framebuffer_bind_count << std::endl;
    std::cout << "Buffer Binds: " << metrics.buffer_bind_count << std::endl;
    std::cout << "Uniform Sets: " << metrics.uniform_set_count << std::endl;
    std::cout << "\nState Redundancy:" << std::endl;
    std::cout << "Consecutive Shader Rebinds: " << metrics.consecutive_shader_rebinds << std::endl;
    std::cout << "Consecutive VAO Rebinds: " << metrics.consecutive_vao_rebinds << std::endl;
    std::cout << "Consecutive Texture Rebinds: " << metrics.consecutive_texture_rebinds << std::endl;
    
    if (metrics.shader_bind_count > 0) {
        float shader_redundancy = (float)metrics.consecutive_shader_rebinds / metrics.shader_bind_count * 100.0f;
        std::cout << "Shader Redundancy: " << shader_redundancy << "%" << std::endl;
    }
    if (metrics.vao_bind_count > 0) {
        float vao_redundancy = (float)metrics.consecutive_vao_rebinds / metrics.vao_bind_count * 100.0f;
        std::cout << "VAO Redundancy: " << vao_redundancy << "%" << std::endl;
    }
    if (metrics.texture_bind_count > 0) {
        float texture_redundancy = (float)metrics.consecutive_texture_rebinds / metrics.texture_bind_count * 100.0f;
        std::cout << "Texture Redundancy: " << texture_redundancy << "%" << std::endl;
    }
    
    std::cout << "\nTop Command Types:" << std::endl;
    std::vector<std::pair<std::string, int>> sorted_types(metrics.command_type_counts.begin(), metrics.command_type_counts.end());
    std::sort(sorted_types.begin(), sorted_types.end(), 
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    int count = 0;
    for (const auto& [type, count_val] : sorted_types) {
        if (count++ >= 10) break;
        std::cout << "  " << type << ": " << count_val << std::endl;
    }
    std::cout << "=============================" << std::endl;
}

void GLCommandExecutor::trackCommandType(const std::string& type) {
    metrics.command_type_counts[type]++;
    metrics.total_commands++;
}

void GLCommandExecutor::executeCommand(const RenderCommand& cmd) {
    switch (cmd.type) {
        case RenderCommand::Type::BindShader:
            handleBindShader(cmd);
            break;
        case RenderCommand::Type::SetUniform1f:
            handleSetUniform1f(cmd);
            break;
        case RenderCommand::Type::SetUniform2f:
            handleSetUniform2f(cmd);
            break;
        case RenderCommand::Type::SetUniform3f:
            handleSetUniform3f(cmd);
            break;
        case RenderCommand::Type::SetUniform4f:
            handleSetUniform4f(cmd);
            break;
        case RenderCommand::Type::SetUniform1i:
            handleSetUniform1i(cmd);
            break;
        case RenderCommand::Type::BindTexture:
            handleBindTexture(cmd);
            break;
        case RenderCommand::Type::UnbindTexture:
            handleUnbindTexture(cmd);
            break;
        case RenderCommand::Type::BindVertexArray:
            handleBindVertexArray(cmd);
            break;
        case RenderCommand::Type::UnbindVertexArray:
            handleUnbindVertexArray(cmd);
            break;
        case RenderCommand::Type::BindBuffer:
            handleBindBuffer(cmd);
            break;
        case RenderCommand::Type::UnbindBuffer:
            handleUnbindBuffer(cmd);
            break;
        case RenderCommand::Type::DrawArrays:
            handleDrawArrays(cmd);
            break;
        case RenderCommand::Type::BindFramebuffer:
            handleBindFramebuffer(cmd);
            break;
        case RenderCommand::Type::UnbindFramebuffer:
            handleUnbindFramebuffer(cmd);
            break;
        case RenderCommand::Type::ClearFramebuffer:
            handleClearFramebuffer(cmd);
            break;
        case RenderCommand::Type::EnableBlend:
            handleEnableBlend(cmd);
            break;
        case RenderCommand::Type::DisableBlend:
            handleDisableBlend(cmd);
            break;
        case RenderCommand::Type::SetBlendFunc:
            handleSetBlendFunc(cmd);
            break;
        case RenderCommand::Type::EnableDepthTest:
            handleEnableDepthTest(cmd);
            break;
        case RenderCommand::Type::DisableDepthTest:
            handleDisableDepthTest(cmd);
            break;
        case RenderCommand::Type::SetViewport:
            handleSetViewport(cmd);
            break;
        default:
            std::cerr << "[GLCommandExecutor] Unknown command type" << std::endl;
            break;
    }
}

void GLCommandExecutor::handleBindShader(const RenderCommand& cmd) {
    trackCommandType("BindShader");
    GLuint program = static_cast<GLuint>(cmd.bindShader.handle);
    
    if (current_shader == program) {
        metrics.consecutive_shader_rebinds++;
    }
    current_shader = program;
    metrics.shader_bind_count++;
    
    glUseProgram(program);
}

void GLCommandExecutor::handleSetUniform1f(const RenderCommand& cmd) {
    trackCommandType("SetUniform1f");
    metrics.uniform_set_count++;
    GLuint program = static_cast<GLuint>(cmd.setUniform1f.handle);
    GLint loc = glGetUniformLocation(program, cmd.stringData.c_str());
    if (loc != -1) {
        glUniform1f(loc, cmd.setUniform1f.value1);
    }
}

void GLCommandExecutor::handleSetUniform2f(const RenderCommand& cmd) {
    trackCommandType("SetUniform2f");
    metrics.uniform_set_count++;
    GLuint program = static_cast<GLuint>(cmd.setUniform2f.handle);
    GLint loc = glGetUniformLocation(program, cmd.stringData.c_str());
    if (loc != -1) {
        glUniform2f(loc, cmd.setUniform2f.value1, cmd.setUniform2f.value2);
    }
}

void GLCommandExecutor::handleSetUniform3f(const RenderCommand& cmd) {
    trackCommandType("SetUniform3f");
    metrics.uniform_set_count++;
    GLuint program = static_cast<GLuint>(cmd.setUniform3f.handle);
    GLint loc = glGetUniformLocation(program, cmd.stringData.c_str());
    if (loc != -1) {
        glUniform3f(loc, cmd.setUniform3f.value1, cmd.setUniform3f.value2, cmd.setUniform3f.value3);
    }
}

void GLCommandExecutor::handleSetUniform4f(const RenderCommand& cmd) {
    trackCommandType("SetUniform4f");
    metrics.uniform_set_count++;
    GLuint program = static_cast<GLuint>(cmd.setUniform4f.handle);
    GLint loc = glGetUniformLocation(program, cmd.stringData.c_str());
    if (loc != -1) {
        glUniform4f(loc, cmd.setUniform4f.value1, cmd.setUniform4f.value2, cmd.setUniform4f.value3, cmd.setUniform4f.value4);
    }
}

void GLCommandExecutor::handleSetUniform1i(const RenderCommand& cmd) {
    trackCommandType("SetUniform1i");
    metrics.uniform_set_count++;
    GLuint program = static_cast<GLuint>(cmd.setUniform1i.handle);
    GLint loc = glGetUniformLocation(program, cmd.stringData.c_str());
    if (loc != -1) {
        glUniform1i(loc, cmd.setUniform1i.value);
    }
}

void GLCommandExecutor::handleBindTexture(const RenderCommand& cmd) {
    trackCommandType("BindTexture");
    int slot = cmd.bindTexture.slot;
    GLuint textureId = cmd.bindTexture.textureId;
    
    if (slot >= 0 && slot < 16 && current_texture[slot] == textureId) {
        metrics.consecutive_texture_rebinds++;
    }
    if (slot >= 0 && slot < 16) {
        current_texture[slot] = textureId;
    }
    metrics.texture_bind_count++;
    
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, textureId);
}

void GLCommandExecutor::handleUnbindTexture(const RenderCommand& cmd) {
    trackCommandType("UnbindTexture");
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLCommandExecutor::handleBindVertexArray(const RenderCommand& cmd) {
    trackCommandType("BindVertexArray");
    GLuint vaoId = cmd.bindVertexArray.vaoId;
    
    if (current_vao == vaoId) {
        metrics.consecutive_vao_rebinds++;
    }
    current_vao = vaoId;
    metrics.vao_bind_count++;
    
    glBindVertexArray(vaoId);
}

void GLCommandExecutor::handleUnbindVertexArray(const RenderCommand& cmd) {
    trackCommandType("UnbindVertexArray");
    glBindVertexArray(0);
}

void GLCommandExecutor::handleBindBuffer(const RenderCommand& cmd) {
    trackCommandType("BindBuffer");
    metrics.buffer_bind_count++;
    glBindBuffer(cmd.bindBuffer.target, cmd.bindBuffer.bufferId);
}

void GLCommandExecutor::handleUnbindBuffer(const RenderCommand& cmd) {
    trackCommandType("UnbindBuffer");
    glBindBuffer(cmd.bindBuffer.target, 0);
}

void GLCommandExecutor::handleDrawArrays(const RenderCommand& cmd) {
    trackCommandType("DrawArrays");
    metrics.draw_call_count++;
    glDrawArrays(cmd.drawArrays.mode, cmd.drawArrays.first, cmd.drawArrays.count);
}

void GLCommandExecutor::handleBindFramebuffer(const RenderCommand& cmd) {
    trackCommandType("BindFramebuffer");
    GLuint fboId = cmd.bindFramebuffer.fboId;
    
    current_fbo = fboId;
    metrics.framebuffer_bind_count++;
    
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);
}

void GLCommandExecutor::handleUnbindFramebuffer(const RenderCommand& cmd) {
    trackCommandType("UnbindFramebuffer");
    current_fbo = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLCommandExecutor::handleClearFramebuffer(const RenderCommand& cmd) {
    trackCommandType("ClearFramebuffer");
    glClearColor(cmd.clearFramebuffer.r, cmd.clearFramebuffer.g, cmd.clearFramebuffer.b, cmd.clearFramebuffer.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLCommandExecutor::handleEnableBlend(const RenderCommand& cmd) {
    trackCommandType("EnableBlend");
    glEnable(GL_BLEND);
}

void GLCommandExecutor::handleDisableBlend(const RenderCommand& cmd) {
    trackCommandType("DisableBlend");
    glDisable(GL_BLEND);
}

void GLCommandExecutor::handleSetBlendFunc(const RenderCommand& cmd) {
    trackCommandType("SetBlendFunc");
    glBlendFunc(cmd.setBlendFunc.sfactor, cmd.setBlendFunc.dfactor);
}

void GLCommandExecutor::handleEnableDepthTest(const RenderCommand& cmd) {
    trackCommandType("EnableDepthTest");
    glEnable(GL_DEPTH_TEST);
}

void GLCommandExecutor::handleDisableDepthTest(const RenderCommand& cmd) {
    trackCommandType("DisableDepthTest");
    glDisable(GL_DEPTH_TEST);
}

void GLCommandExecutor::handleSetViewport(const RenderCommand& cmd) {
    trackCommandType("SetViewport");
    glViewport(cmd.setViewport.x, cmd.setViewport.y, cmd.setViewport.width, cmd.setViewport.height);
}
