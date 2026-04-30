#include "shader.h"
#include "gl_shader_compiler.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader() : handle_(INVALID_SHADER_HANDLE) {}

Shader::~Shader() {
    if (handle_ != INVALID_SHADER_HANDLE) {
        GLShaderCompiler::destroyProgram(handle_);
    }
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::ifstream vertexFile(vertexPath);
    std::ifstream fragmentFile(fragmentPath);

    if (!vertexFile.is_open() || !fragmentFile.is_open()) {
        std::cerr << "Failed to open shader files" << std::endl;
        lastError_ = "Failed to open shader files";
        return false;
    }

    std::stringstream vertexStream, fragmentStream;
    vertexStream << vertexFile.rdbuf();
    fragmentStream << fragmentFile.rdbuf();

    return loadFromSource(vertexStream.str(), fragmentStream.str());
}

bool Shader::loadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    // Delegate compilation to backend-specific compiler
    handle_ = GLShaderCompiler::compileProgram(vertexSource, fragmentSource, lastError_);

    if (handle_ == INVALID_SHADER_HANDLE) {
        std::cerr << "[SHADER ERROR]" << std::endl;
        std::cerr << "----- FRAGMENT SHADER COMPILATION FAILED -----" << std::endl;
        std::cerr << lastError_ << std::endl;
        std::cerr << "----- Debug dump written to: debug_shader.frag -----" << std::endl;
        return false;
    }

    return true;
}

void Shader::use() {
    if (handle_ != INVALID_SHADER_HANDLE) {
        GLShaderCompiler::useProgram(handle_);
    }
}

void Shader::setUniform1f(const std::string& name, float value) {
    GLShaderCompiler::setUniform1f(handle_, name, value);
}

void Shader::setUniform2f(const std::string& name, float x, float y) {
    GLShaderCompiler::setUniform2f(handle_, name, x, y);
}

void Shader::setUniform3f(const std::string& name, float x, float y, float z) {
    GLShaderCompiler::setUniform3f(handle_, name, x, y, z);
}

void Shader::setUniform4f(const std::string& name, float x, float y, float z, float w) {
    GLShaderCompiler::setUniform4f(handle_, name, x, y, z, w);
}

void Shader::setUniform1i(const std::string& name, int value) {
    GLShaderCompiler::setUniform1i(handle_, name, value);
}
