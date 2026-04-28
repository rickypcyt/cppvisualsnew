#include "gl_shader_compiler.h"
#include <GL/glew.h>
#include <iostream>

GLuint GLShaderCompiler::toGLuint(ShaderHandle handle) {
    return static_cast<GLuint>(handle);
}

ShaderHandle GLShaderCompiler::fromGLuint(GLuint handle) {
    return static_cast<ShaderHandle>(handle);
}

ShaderHandle GLShaderCompiler::compileProgram(
    const std::string& vertexSource,
    const std::string& fragmentSource,
    std::string& outErrorLog
) {
    // Compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    const char* vSrc = vertexSource.c_str();
    glShaderSource(vertexShader, 1, &vSrc, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(vertexShader, 1024, nullptr, infoLog);
        outErrorLog = "Vertex shader compilation error: " + std::string(infoLog);
        glDeleteShader(vertexShader);
        return INVALID_SHADER_HANDLE;
    }

    // Compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fSrc = fragmentSource.c_str();
    glShaderSource(fragmentShader, 1, &fSrc, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(fragmentShader, 1024, nullptr, infoLog);
        outErrorLog = "Fragment shader compilation error: " + std::string(infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return INVALID_SHADER_HANDLE;
    }

    // Link program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        outErrorLog = "Program linking error: " + std::string(infoLog);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return INVALID_SHADER_HANDLE;
    }

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return fromGLuint(program);
}

void GLShaderCompiler::destroyProgram(ShaderHandle handle) {
    if (handle != INVALID_SHADER_HANDLE) {
        GLuint program = toGLuint(handle);
        glDeleteProgram(program);
    }
}

void GLShaderCompiler::useProgram(ShaderHandle handle) {
    if (handle != INVALID_SHADER_HANDLE) {
        GLuint program = toGLuint(handle);
        glUseProgram(program);
    }
}

void GLShaderCompiler::setUniform1f(ShaderHandle handle, const std::string& name, float value) {
    if (handle == INVALID_SHADER_HANDLE) return;
    GLuint program = toGLuint(handle);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc != -1) {
        glUniform1f(loc, value);
    }
}

void GLShaderCompiler::setUniform2f(ShaderHandle handle, const std::string& name, float x, float y) {
    if (handle == INVALID_SHADER_HANDLE) return;
    GLuint program = toGLuint(handle);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc != -1) {
        glUniform2f(loc, x, y);
    }
}

void GLShaderCompiler::setUniform3f(ShaderHandle handle, const std::string& name, float x, float y, float z) {
    if (handle == INVALID_SHADER_HANDLE) return;
    GLuint program = toGLuint(handle);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc != -1) {
        glUniform3f(loc, x, y, z);
    }
}

void GLShaderCompiler::setUniform4f(ShaderHandle handle, const std::string& name, float x, float y, float z, float w) {
    if (handle == INVALID_SHADER_HANDLE) return;
    GLuint program = toGLuint(handle);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc != -1) {
        glUniform4f(loc, x, y, z, w);
    }
}

void GLShaderCompiler::setUniform1i(ShaderHandle handle, const std::string& name, int value) {
    if (handle == INVALID_SHADER_HANDLE) return;
    GLuint program = toGLuint(handle);
    GLint loc = glGetUniformLocation(program, name.c_str());
    if (loc != -1) {
        glUniform1i(loc, value);
    }
}
