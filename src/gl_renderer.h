#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#include "renderer_interface.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <memory>
#include <unordered_map>
#include <string>

// ============================================================
// GLShader - OpenGL implementation of IShader
// ============================================================
class GLShader : public IShader {
public:
    GLShader();
    ~GLShader() override;

    bool loadFromSource(const std::string& vertexSource, const std::string& fragmentSource);
    bool loadFromFile(const std::string& vertexPath, const std::string& fragmentPath);
    bool reload();

    // IShader implementation
    void bind() override;
    void unbind() override;

    void setUniform1i(const std::string& name, int value) override;
    void setUniform1f(const std::string& name, float value) override;
    void setUniform2f(const std::string& name, float x, float y) override;
    void setUniform3f(const std::string& name, float x, float y, float z) override;
    void setUniform4f(const std::string& name, float x, float y, float z, float w) override;
    void setUniformMatrix4f(const std::string& name, const float* matrix) override;

    GLuint getProgram() const { return program_; }

private:
    GLint getUniformLocation(const std::string& name);
    GLuint compileShader(const std::string& source, GLenum type);
    bool linkProgram();

    GLuint program_;
    GLuint vertexShader_;
    GLuint fragmentShader_;
    std::unordered_map<std::string, GLint> uniformLocations_;
};

// ============================================================
// GLTexture - OpenGL implementation of ITexture
// ============================================================
class GLTexture : public ITexture {
public:
    GLTexture(int width, int height, TextureFormat format);
    ~GLTexture() override;

    // ITexture implementation
    void bind(int slot = 0) override;
    void unbind() override;

    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    TextureFormat getFormat() const override { return format_; }

    void resize(int width, int height) override;

    // OpenGL-specific
    GLuint getTextureID() const { return textureId_; }
    void setFilter(GLint minFilter, GLint magFilter);
    void setWrap(GLint wrapS, GLint wrapT);
    void setData(const void* data);

private:
    GLuint textureId_;
    int width_;
    int height_;
    TextureFormat format_;
};

// ============================================================
// GLFramebuffer - OpenGL implementation of IFramebuffer
// ============================================================
class GLFramebuffer : public IFramebuffer {
public:
    GLFramebuffer(int width, int height);
    ~GLFramebuffer() override;

    // IFramebuffer implementation
    void bind() override;
    void unbind() override;

    void attachColorTexture(ITexture* texture, int attachment = 0) override;
    void attachDepthTexture(ITexture* texture) override;

    void clear(float r, float g, float b, float a) override;

    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }

    // OpenGL-specific
    GLuint getFramebufferID() const { return fbo_; }
    bool isComplete() const;

private:
    GLuint fbo_;
    int width_;
    int height_;
    std::vector<GLuint> colorAttachments_;
};

// ============================================================
// GLRenderer - OpenGL implementation of IRenderer
// ============================================================
class GLRenderer : public IRenderer {
public:
    GLRenderer();
    ~GLRenderer() override;

    // IRenderer lifecycle
    bool initialize(int width, int height, const std::string& windowTitle) override;
    void shutdown() override;
    bool shouldClose() override;

    // Frame management
    void beginFrame() override;
    void endFrame() override;
    void swapBuffers() override;

    // Layer rendering (placeholder - actual implementation in Visualizer)
    void renderBackground(const AudioFeatures& features) override;
    void renderProceduralLayer(const AudioFeatures& features, int mode, float opacity) override;
    void renderPostProcessing(const std::vector<int>& effects) override;
    void renderUI() override;

    // Resource creation
    std::unique_ptr<IShader> createShader(ShaderType type) override;
    std::unique_ptr<ITexture> createTexture(int width, int height, TextureFormat format) override;
    std::unique_ptr<IFramebuffer> createFramebuffer(int width, int height) override;
    std::unique_ptr<IRenderLayer> createProceduralLayer() override;

    // State management
    void setViewport(int x, int y, int width, int height) override;
    void enableBlending(bool enable) override;
    void enableDepthTest(bool enable) override;

    // Hot reload
    void reloadShaders() override;
    void setShaderHotReload(bool enabled) override;

    // Info
    std::string getBackendName() const override { return "OpenGL"; }
    std::string getVersionString() const override;
    bool supportsComputeShaders() const override;

    // OpenGL-specific access (for migration period)
    SDL_Window* getWindow() const { return window_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    SDL_Window* window_;
    SDL_GLContext glContext_;
    int width_;
    int height_;
    bool shaderHotReload_;
    std::string windowTitle_;

    bool initializeGL();
    bool initializeWindow(int width, int height, const std::string& title);
};

#endif // GL_RENDERER_H
