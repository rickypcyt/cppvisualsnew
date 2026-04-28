#ifndef SHADER_H
#define SHADER_H

#include <string>
#include "shader_handle.h"

// Forward declaration for backend-specific compiler
class GLShaderCompiler;

// Backend-agnostic shader class
// Handles loading, caching, and logical validation
// Delegates actual compilation to backend-specific compiler
class Shader {
public:
    Shader();
    ~Shader();

    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);
    void use();
    void setUniform1f(const std::string& name, float value);
    void setUniform2f(const std::string& name, float x, float y);
    void setUniform3f(const std::string& name, float x, float y, float z);
    void setUniform4f(const std::string& name, float x, float y, float z, float w);
    void setUniform1i(const std::string& name, int value);

    // Get the opaque handle (for backend operations)
    ShaderHandle getHandle() const { return handle_; }

private:
    ShaderHandle handle_;
    std::string lastError_;
};

#endif // SHADER_H
