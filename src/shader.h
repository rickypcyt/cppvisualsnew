#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <GL/glew.h>

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

private:
    GLuint program_;
    GLuint vertexShader_;
    GLuint fragmentShader_;

    GLuint compileShader(const std::string& source, GLenum type);
    bool linkProgram();
    GLint getUniformLocation(const std::string& name);
};

#endif // SHADER_H
