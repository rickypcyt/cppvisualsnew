#ifndef RENDERER_INTERFACE_H
#define RENDERER_INTERFACE_H

#include <vector>
#include <string>
#include <memory>
#include <functional>

// Forward declarations
struct AudioFeatures;

// Render layer types
enum class RenderLayer {
    BACKGROUND,
    PROCEDURAL_BASE,
    PROCEDURAL_OVERLAY,
    POST_PROCESSING,
    UI_OVERLAY
};

// Shader type enumeration
enum class ShaderType {
    PROCEDURAL,
    POST_PROCESSING,
    UI,
    CORE
};

// Texture format
enum class TextureFormat {
    RGBA8,
    RGBA16F,
    RGBA32F,
    DEPTH24_STENCIL8
};

// ============================================================
// IShader - Abstract shader interface
// ============================================================
class IShader {
public:
    virtual ~IShader() = default;

    // Activation
    virtual void bind() = 0;
    virtual void unbind() = 0;

    // Uniforms
    virtual void setUniform1i(const std::string& name, int value) = 0;
    virtual void setUniform1f(const std::string& name, float value) = 0;
    virtual void setUniform2f(const std::string& name, float x, float y) = 0;
    virtual void setUniform3f(const std::string& name, float x, float y, float z) = 0;
    virtual void setUniform4f(const std::string& name, float x, float y, float z, float w) = 0;
    virtual void setUniformMatrix4f(const std::string& name, const float* matrix) = 0;

    // Audio-specific uniforms (convenience)
    void setAudioFeatures(const AudioFeatures& features);
};

// ============================================================
// ITexture - Abstract texture interface
// ============================================================
class ITexture {
public:
    virtual ~ITexture() = default;

    virtual void bind(int slot = 0) = 0;
    virtual void unbind() = 0;
    
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual TextureFormat getFormat() const = 0;
    
    // For render-to-texture
    virtual void resize(int width, int height) = 0;
};

// ============================================================
// IFramebuffer - Abstract framebuffer/render target
// ============================================================
class IFramebuffer {
public:
    virtual ~IFramebuffer() = default;

    virtual void bind() = 0;
    virtual void unbind() = 0;
    
    virtual void attachColorTexture(ITexture* texture, int attachment = 0) = 0;
    virtual void attachDepthTexture(ITexture* texture) = 0;
    
    virtual void clear(float r, float g, float b, float a) = 0;
    
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};

// ============================================================
// IRenderLayer - Procedural/post-processing layer abstraction
// ============================================================
class IRenderLayer {
public:
    virtual ~IRenderLayer() = default;

    // Layer configuration
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
    
    virtual void setOpacity(float opacity) = 0;
    virtual float getOpacity() const = 0;
    
    // Shader mode for procedural layers
    virtual void setMode(int mode) = 0;
    virtual int getMode() const = 0;

    // Rendering
    virtual void render(const AudioFeatures& features, float time, float tempo) = 0;
    virtual void composite(IFramebuffer* target) = 0;
};

// ============================================================
// IRenderer - Main renderer interface
// Future implementations:
//   - GLRenderer (OpenGL - current implementation)
//   - VKRenderer (Vulkan - future implementation)
// ============================================================
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ========== Lifecycle ==========
    virtual bool initialize(int width, int height, const std::string& windowTitle) = 0;
    virtual void shutdown() = 0;
    virtual bool shouldClose() = 0;

    // ========== Frame Management ==========
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void swapBuffers() = 0;

    // ========== Layer Rendering ==========
    virtual void renderBackground(const AudioFeatures& features) = 0;
    virtual void renderProceduralLayer(const AudioFeatures& features, int mode, float opacity) = 0;
    virtual void renderPostProcessing(const std::vector<int>& effects) = 0;
    virtual void renderUI() = 0;

    // ========== Resource Creation ==========
    virtual std::unique_ptr<IShader> createShader(ShaderType type) = 0;
    virtual std::unique_ptr<ITexture> createTexture(int width, int height, TextureFormat format) = 0;
    virtual std::unique_ptr<IFramebuffer> createFramebuffer(int width, int height) = 0;
    virtual std::unique_ptr<IRenderLayer> createProceduralLayer() = 0;

    // ========== State Management ==========
    virtual void setViewport(int x, int y, int width, int height) = 0;
    virtual void enableBlending(bool enable) = 0;
    virtual void enableDepthTest(bool enable) = 0;
    
    // ========== Hot Reload ==========
    virtual void reloadShaders() = 0;
    virtual void setShaderHotReload(bool enabled) = 0;

    // ========== Info ==========
    virtual std::string getBackendName() const = 0;
    virtual std::string getVersionString() const = 0;
    virtual bool supportsComputeShaders() const = 0;
};

// ============================================================
// Renderer Factory
// Creates appropriate renderer based on configuration
// ============================================================
class RendererFactory {
public:
    enum class Backend {
        OPENGL,     // Current stable backend
        VULKAN      // Future high-performance backend
    };

    // Create renderer instance
    static std::unique_ptr<IRenderer> create(Backend backend = Backend::OPENGL);

    // Check if backend is available
    static bool isBackendAvailable(Backend backend);

    // Get default backend for platform
    static Backend getDefaultBackend();

    // Parse backend from string (for command line args)
    static Backend parseBackend(const std::string& str);

    // Get backend name as string
    static std::string getBackendName(Backend backend);
};

// ============================================================
// Convenience typedefs for current OpenGL implementation
// These can be removed once GLRenderer fully implements IRenderer
// ============================================================
using ShaderPtr = std::unique_ptr<IShader>;
using TexturePtr = std::unique_ptr<ITexture>;
using FramebufferPtr = std::unique_ptr<IFramebuffer>;
using RenderLayerPtr = std::unique_ptr<IRenderLayer>;
using RendererPtr = std::unique_ptr<IRenderer>;

#endif // RENDERER_INTERFACE_H
