#ifndef GL_SHADER_COMPILER_H
#define GL_SHADER_COMPILER_H

#include <string>
#include <GL/glew.h>
#include "shader_handle.h"

// OpenGL-specific shader compilation
// This belongs in gl_backend, not shader_system
class GLShaderCompiler {
public:
    // Compile vertex and fragment shaders into a program
    // Returns a ShaderHandle (opaque GLuint wrapper)
    static ShaderHandle compileProgram(
        const std::string& vertexSource,
        const std::string& fragmentSource,
        std::string& outErrorLog
    );

    // Destroy a shader program
    static void destroyProgram(ShaderHandle handle);

    // Bind a shader program for use
    static void useProgram(ShaderHandle handle);

    // Set uniform values
    static void setUniform1f(ShaderHandle handle, const std::string& name, float value);
    static void setUniform2f(ShaderHandle handle, const std::string& name, float x, float y);
    static void setUniform3f(ShaderHandle handle, const std::string& name, float x, float y, float z);
    static void setUniform4f(ShaderHandle handle, const std::string& name, float x, float y, float z, float w);
    static void setUniform1i(ShaderHandle handle, const std::string& name, int value);

private:
    // Convert ShaderHandle to GLuint
    static GLuint toGLuint(ShaderHandle handle);
    static ShaderHandle fromGLuint(GLuint handle);
};

#endif // GL_SHADER_COMPILER_H
