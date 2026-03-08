#include "shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader() : program_(0), vertexShader_(0), fragmentShader_(0) {}

Shader::~Shader() {
    if (program_) {
        glDeleteProgram(program_);
    }
    if (vertexShader_) {
        glDeleteShader(vertexShader_);
    }
    if (fragmentShader_) {
        glDeleteShader(fragmentShader_);
    }
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::ifstream vertexFile(vertexPath);
    std::ifstream fragmentFile(fragmentPath);
    
    if (!vertexFile.is_open() || !fragmentFile.is_open()) {
        std::cerr << "Failed to open shader files" << std::endl;
        return false;
    }

    std::stringstream vertexStream, fragmentStream;
    vertexStream << vertexFile.rdbuf();
    fragmentStream << fragmentFile.rdbuf();

    return loadFromSource(vertexStream.str(), fragmentStream.str());
}

bool Shader::loadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    // Compile shaders
    vertexShader_ = compileShader(vertexSource, GL_VERTEX_SHADER);
    fragmentShader_ = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    if (vertexShader_ == 0 || fragmentShader_ == 0) {
        return false;
    }

    // Link program
    return linkProgram();
}

void Shader::use() {
    if (program_) {
        glUseProgram(program_);
    }
}

void Shader::setUniform1f(const std::string& name, float value) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) {
        glUniform1f(loc, value);
    }
}

void Shader::setUniform2f(const std::string& name, float x, float y) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) {
        glUniform2f(loc, x, y);
    }
}

void Shader::setUniform3f(const std::string& name, float x, float y, float z) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) {
        glUniform3f(loc, x, y, z);
    }
}

void Shader::setUniform1i(const std::string& name, int value) {
    GLint loc = getUniformLocation(name);
    if (loc != -1) {
        glUniform1i(loc, value);
    }
}

GLuint Shader::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::linkProgram() {
    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader_);
    glAttachShader(program_, fragmentShader_);
    glLinkProgram(program_);

    GLint success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program_, 512, nullptr, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
        return false;
    }

    // Shaders can be deleted after linking
    glDeleteShader(vertexShader_);
    glDeleteShader(fragmentShader_);
    vertexShader_ = 0;
    fragmentShader_ = 0;

    return true;
}

GLint Shader::getUniformLocation(const std::string& name) {
    if (!program_) return -1;
    return glGetUniformLocation(program_, name.c_str());
}
