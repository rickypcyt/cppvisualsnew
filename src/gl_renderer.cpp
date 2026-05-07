#include "gl_renderer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vulkan/vulkan.h>

// ============================================================
// GLRenderer Implementation
// ============================================================

GLRenderer::GLRenderer() : context_(nullptr), width_(0), height_(0), shaderHotReload_(false) {}

// ============================================================
// GLShader Implementation
// ============================================================

GLShader::GLShader() : program_(0), vertexShader_(0), fragmentShader_(0) {}

GLShader::~GLShader() {
    if (program_) {
        glDeleteProgram(program_);
    }
}

bool GLShader::loadFromSource(const std::string& vertexSource, const std::string& fragmentSource) {
    vertexShader_ = compileShader(vertexSource, GL_VERTEX_SHADER);
    fragmentShader_ = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    if (vertexShader_ == 0 || fragmentShader_ == 0) {
        return false;
    }

    return linkProgram();
}

bool GLShader::loadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    auto readFile = [](const std::string& path) -> std::string {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << path << std::endl;
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    };

    std::string vertexSource = readFile(vertexPath);
    std::string fragmentSource = readFile(fragmentPath);

    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }

    return loadFromSource(vertexSource, fragmentSource);
}

bool GLShader::reload() {
    // TODO: Store original source/file paths for reloading
    return false;
}

void GLShader::bind() {
    if (program_) {
        glUseProgram(program_);
    }
}

void GLShader::unbind() {
    glUseProgram(0);
}

GLint GLShader::getUniformLocation(const std::string& name) {
    auto it = uniformLocations_.find(name);
    if (it != uniformLocations_.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(program_, name.c_str());
    uniformLocations_[name] = location;
    return location;
}

void GLShader::setUniform1i(const std::string& name, int value) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void GLShader::setUniform1f(const std::string& name, float value) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void GLShader::setUniform2f(const std::string& name, float x, float y) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform2f(loc, x, y);
    }
}

void GLShader::setUniform3f(const std::string& name, float x, float y, float z) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform3f(loc, x, y, z);
    }
}

void GLShader::setUniform4f(const std::string& name, float x, float y, float z, float w) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform4f(loc, x, y, z, w);
    }
}

void GLShader::setUniformMatrix4f(const std::string& name, const float* matrix) {
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
    }
}

GLuint GLShader::compileShader(const std::string& source, GLenum type) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "[GLShader] Compilation error: " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool GLShader::linkProgram() {
    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader_);
    glAttachShader(program_, fragmentShader_);
    glLinkProgram(program_);

    GLint success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program_, 512, nullptr, infoLog);
        std::cerr << "[GLShader] Linking error: " << infoLog << std::endl;
        return false;
    }

    glDeleteShader(vertexShader_);
    glDeleteShader(fragmentShader_);
    vertexShader_ = 0;
    fragmentShader_ = 0;

    return true;
}

// ============================================================
// GLTexture Implementation
// ============================================================

GLTexture::GLTexture(int width, int height, TextureFormat format)
    : textureId_(0), width_(width), height_(height), format_(format) {
    glGenTextures(1, &textureId_);
    bind();

    GLenum internalFormat = GL_RGBA8;
    GLenum dataFormat = GL_RGBA;
    GLenum dataType = GL_UNSIGNED_BYTE;

    switch (format) {
        case TextureFormat::RGBA8:
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            dataType = GL_UNSIGNED_BYTE;
            break;
        case TextureFormat::RGBA16F:
            internalFormat = GL_RGBA16F;
            dataFormat = GL_RGBA;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::RGBA32F:
            internalFormat = GL_RGBA32F;
            dataFormat = GL_RGBA;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::DEPTH24_STENCIL8:
            internalFormat = GL_DEPTH24_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_UNSIGNED_INT_24_8;
            break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, dataType, nullptr);
    setFilter(GL_LINEAR, GL_LINEAR);
    setWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    unbind();
}

GLTexture::~GLTexture() {
    if (textureId_) {
        glDeleteTextures(1, &textureId_);
    }
}

void GLTexture::bind(int slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, textureId_);
}

void GLTexture::unbind() {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::resize(int width, int height) {
    width_ = width;
    height_ = height;
    bind();

    GLenum internalFormat = GL_RGBA8;
    GLenum dataFormat = GL_RGBA;
    GLenum dataType = GL_UNSIGNED_BYTE;

    switch (format_) {
        case TextureFormat::RGBA8:
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            dataType = GL_UNSIGNED_BYTE;
            break;
        case TextureFormat::RGBA16F:
            internalFormat = GL_RGBA16F;
            dataFormat = GL_RGBA;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::RGBA32F:
            internalFormat = GL_RGBA32F;
            dataFormat = GL_RGBA;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::DEPTH24_STENCIL8:
            internalFormat = GL_DEPTH24_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_UNSIGNED_INT_24_8;
            break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, dataType, nullptr);
    unbind();
}

void GLTexture::setFilter(GLint minFilter, GLint magFilter) {
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    unbind();
}

void GLTexture::setWrap(GLint wrapS, GLint wrapT) {
    bind();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    unbind();
}

void GLTexture::setData(const void* data) {
    bind();

    GLenum dataFormat = GL_RGBA;
    GLenum dataType = GL_UNSIGNED_BYTE;

    switch (format_) {
        case TextureFormat::RGBA8:
            dataFormat = GL_RGBA;
            dataType = GL_UNSIGNED_BYTE;
            break;
        case TextureFormat::RGBA16F:
            dataFormat = GL_RGBA;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::RGBA32F:
            dataFormat = GL_RGBA;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::DEPTH24_STENCIL8:
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_UNSIGNED_INT_24_8;
            break;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, dataFormat, dataType, data);
    unbind();
}

// ============================================================
// GLFramebuffer Implementation
// ============================================================

GLFramebuffer::GLFramebuffer(int width, int height)
    : fbo_(0), width_(width), height_(height) {
    glGenFramebuffers(1, &fbo_);
}

GLFramebuffer::~GLFramebuffer() {
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
    }
}

void GLFramebuffer::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

void GLFramebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFramebuffer::attachColorTexture(ITexture* texture, int attachment) {
    bind();
    GLTexture* glTexture = static_cast<GLTexture*>(texture);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment, GL_TEXTURE_2D, glTexture->getTextureID(), 0);
    
    if (attachment >= colorAttachments_.size()) {
        colorAttachments_.resize(attachment + 1);
    }
    colorAttachments_[attachment] = glTexture->getTextureID();
    unbind();
}

void GLFramebuffer::attachDepthTexture(ITexture* texture) {
    bind();
    GLTexture* glTexture = static_cast<GLTexture*>(texture);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, glTexture->getTextureID(), 0);
    unbind();
}

void GLFramebuffer::clear(float r, float g, float b, float a) {
    bind();
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    unbind();
}

bool GLFramebuffer::isComplete() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

// ============================================================
// GLRenderer Implementation
// ============================================================

GLRenderer::~GLRenderer() {
    shutdown();
}

bool GLRenderer::initialize(int width, int height, const std::string& windowTitle) {
    width_ = width;
    height_ = height;
    windowTitle_ = windowTitle;

    // Initialize GLFW globally (only once)
    if (!GLContext::initializeGLFW()) {
        return false;
    }

    // Create GL context with window
    context_ = std::make_unique<GLContext>();
    if (!context_->createWindow(width, height, windowTitle)) {
        std::cerr << "[GLRenderer] Failed to create GL context" << std::endl;
        return false;
    }

    // Make context current and initialize GLEW
    context_->makeCurrent();

    if (!initializeGL()) {
        return false;
    }

    // Disable VSync for uncapped FPS
    context_->setVSync(false);

    std::cout << "[GLRenderer] Initialized: " << getVersionString() << std::endl;
    return true;
}

bool GLRenderer::initializeGL() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void GLRenderer::shutdown() {
    if (context_) {
        context_->destroyWindow();
        context_.reset();
    }
    GLContext::shutdownGLFW();
}

bool GLRenderer::shouldClose() {
    return context_ && context_->shouldClose();
}

void GLRenderer::beginFrame() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::endFrame() {
    // End frame placeholder
}

void GLRenderer::swapBuffers() {
    if (context_) {
        context_->swapBuffers();
    }
}

// Layer rendering stubs - actual implementation will be migrated from Visualizer
void GLRenderer::renderBackground(const AudioFeatures& features) {
    // TODO: Migrate from Visualizer
}

void GLRenderer::renderProceduralLayer(const AudioFeatures& features, int mode, float opacity) {
    // TODO: Migrate from Visualizer
}

void GLRenderer::renderPostProcessing(const std::vector<int>& effects) {
    // TODO: Migrate from Visualizer
}

void GLRenderer::renderUI() {
    // TODO: Migrate from Visualizer (ImGui)
}

std::unique_ptr<IShader> GLRenderer::createShader(ShaderType type) {
    return std::make_unique<GLShader>();
}

std::unique_ptr<ITexture> GLRenderer::createTexture(int width, int height, TextureFormat format) {
    return std::make_unique<GLTexture>(width, height, format);
}

std::unique_ptr<IFramebuffer> GLRenderer::createFramebuffer(int width, int height) {
    return std::make_unique<GLFramebuffer>(width, height);
}

std::unique_ptr<IRenderLayer> GLRenderer::createProceduralLayer() {
    // TODO: Implement when migrating ModularLayer
    return nullptr;
}

void GLRenderer::setViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GLRenderer::enableBlending(bool enable) {
    if (enable) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

void GLRenderer::enableDepthTest(bool enable) {
    if (enable) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void GLRenderer::reloadShaders() {
    // TODO: Implement shader hot reload
}

void GLRenderer::setShaderHotReload(bool enabled) {
    shaderHotReload_ = enabled;
}

std::string GLRenderer::getVersionString() const {
    if (context_) {
        return std::string("OpenGL ") + context_->getGLVersion() + " - " + context_->getGLRenderer();
    }
    return "OpenGL - Unknown";
}

bool GLRenderer::supportsComputeShaders() const {
    // OpenGL 4.3+ supports compute shaders
    if (context_) {
        std::string version = context_->getGLVersion();
        int major = 0, minor = 0;
        sscanf(version.c_str(), "%d.%d", &major, &minor);
        return major > 4 || (major == 4 && minor >= 3);
    }
    return false;
}

// ============================================================
// RendererFactory Implementation
// ============================================================

std::unique_ptr<IRenderer> RendererFactory::create(Backend backend) {
    switch (backend) {
        case Backend::OPENGL:
            return std::make_unique<GLRenderer>();
        case Backend::VULKAN:
            // VKRenderer not yet fully implemented - fall back to OpenGL
            std::cerr << "[RendererFactory] Vulkan renderer not yet fully implemented" << std::endl;
            std::cerr << "[RendererFactory] Falling back to OpenGL" << std::endl;
            return std::make_unique<GLRenderer>();
        default:
            return std::make_unique<GLRenderer>();
    }
}

bool RendererFactory::isBackendAvailable(Backend backend) {
    switch (backend) {
        case Backend::OPENGL:
            return true; // OpenGL is always available
        case Backend::VULKAN: {
            // Check if Vulkan is available by trying to get the instance version
            // This is a simple check - in production you'd want more robust detection
            uint32_t apiVersion = 0;
            VkResult result = vkEnumerateInstanceVersion(&apiVersion);
            return result == VK_SUCCESS;
        }
        default:
            return false;
    }
}

RendererFactory::Backend RendererFactory::getDefaultBackend() {
    // Vulkan is preferred if available, otherwise OpenGL
    if (isBackendAvailable(Backend::VULKAN)) {
        std::cout << "[RendererFactory] Vulkan available, defaulting to Vulkan" << std::endl;
        return Backend::VULKAN;
    }
    std::cout << "[RendererFactory] Vulkan not available, defaulting to OpenGL" << std::endl;
    return Backend::OPENGL;
}

RendererFactory::Backend RendererFactory::parseBackend(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);

    if (lowerStr == "vulkan" || lowerStr == "vk") {
        return Backend::VULKAN;
    } else if (lowerStr == "opengl" || lowerStr == "gl" || lowerStr == "ogl") {
        return Backend::OPENGL;
    } else {
        std::cerr << "[RendererFactory] Unknown backend: " << str << ", defaulting to OpenGL" << std::endl;
        return Backend::OPENGL;
    }
}

std::string RendererFactory::getBackendName(Backend backend) {
    switch (backend) {
        case Backend::OPENGL:
            return "OpenGL";
        case Backend::VULKAN:
            return "Vulkan";
        default:
            return "Unknown";
    }
}
