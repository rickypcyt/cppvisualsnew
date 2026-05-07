#include "gl_context.h"
#include <iostream>

// Static member initialization
bool GLContext::glfwInitialized_ = false;

GLContext::GLContext()
    : window_(nullptr)
    , width_(0)
    , height_(0)
    , framebufferSizeCallback_(nullptr)
{
}

GLContext::~GLContext() {
    destroyWindow();
}

bool GLContext::initializeGLFW() {
    if (glfwInitialized_) {
        return true; // Already initialized
    }

    if (!glfwInit()) {
        std::cerr << "[GLContext] Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwInitialized_ = true;
    std::cout << "[GLContext] GLFW initialized successfully" << std::endl;
    return true;
}

void GLContext::shutdownGLFW() {
    if (glfwInitialized_) {
        glfwTerminate();
        glfwInitialized_ = false;
        std::cout << "[GLContext] GLFW terminated" << std::endl;
    }
}

bool GLContext::createWindow(int width, int height, const std::string& title, GLFWmonitor* monitor, GLFWwindow* share) {
    if (!glfwInitialized_) {
        if (!initializeGLFW()) {
            return false;
        }
    }

    width_ = width;
    height_ = height;

    // Configure window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(width, height, title.c_str(), monitor, share);
    if (!window_) {
        std::cerr << "[GLContext] Failed to create GLFW window" << std::endl;
        return false;
    }

    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);

    // Set framebuffer size callback
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallbackStatic);

    std::cout << "[GLContext] Window created: " << width << "x" << height << std::endl;
    return true;
}

void GLContext::destroyWindow() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        width_ = 0;
        height_ = 0;
    }
}

void GLContext::makeCurrent() const {
    if (window_) {
        glfwMakeContextCurrent(window_);

        // Initialize GLEW on first context make
        static bool glewInitialized = false;
        if (!glewInitialized) {
            glewExperimental = GL_TRUE;
            if (glewInit() != GLEW_OK) {
                std::cerr << "[GLContext] Failed to initialize GLEW" << std::endl;
            } else {
                glewInitialized = true;
                std::cout << "[GLContext] GLEW initialized successfully" << std::endl;
                std::cout << "[GLContext] OpenGL Version: " << getGLVersion() << std::endl;
                std::cout << "[GLContext] OpenGL Renderer: " << getGLRenderer() << std::endl;
            }
        }
    }
}

void GLContext::swapBuffers() const {
    if (window_) {
        glfwSwapBuffers(window_);
    }
}

bool GLContext::shouldClose() const {
    return window_ && glfwWindowShouldClose(window_);
}

void GLContext::setWindowTitle(const std::string& title) {
    if (window_) {
        glfwSetWindowTitle(window_, title.c_str());
    }
}

void GLContext::setWindowSize(int width, int height) {
    if (window_) {
        glfwSetWindowSize(window_, width, height);
        width_ = width;
        height_ = height;
    }
}

void GLContext::setWindowPosition(int x, int y) {
    if (window_) {
        glfwSetWindowPos(window_, x, y);
    }
}

void GLContext::setWindowFullscreen(bool fullscreen) {
    if (!window_) return;

    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
    } else {
        glfwSetWindowMonitor(window_, nullptr, 100, 100, width_, height_, GLFW_DONT_CARE);
    }
}

void GLContext::setWindowVisible(bool visible) {
    if (window_) {
        if (visible) {
            glfwShowWindow(window_);
        } else {
            glfwHideWindow(window_);
        }
    }
}

void GLContext::setWindowIconified(bool iconified) {
    if (window_) {
        if (iconified) {
            glfwIconifyWindow(window_);
        } else {
            glfwRestoreWindow(window_);
        }
    }
}

bool GLContext::isWindowVisible() const {
    return window_ && glfwGetWindowAttrib(window_, GLFW_VISIBLE);
}

bool GLContext::isWindowIconified() const {
    return window_ && glfwGetWindowAttrib(window_, GLFW_ICONIFIED);
}

bool GLContext::isWindowFullscreen() const {
    return window_ && (glfwGetWindowMonitor(window_) != nullptr);
}

GLFWmonitor* GLContext::getWindowMonitor() const {
    return window_ ? glfwGetWindowMonitor(window_) : nullptr;
}

void GLContext::setInputMode(int mode, int value) {
    if (window_) {
        glfwSetInputMode(window_, mode, value);
    }
}

int GLContext::getInputMode(int mode) const {
    return window_ ? glfwGetInputMode(window_, mode) : 0;
}

void GLContext::setFramebufferSizeCallback(FramebufferSizeCallback callback) {
    framebufferSizeCallback_ = callback;
}

void GLContext::framebufferSizeCallbackStatic(GLFWwindow* window, int width, int height) {
    GLContext* context = static_cast<GLContext*>(glfwGetWindowUserPointer(window));
    if (context && context->framebufferSizeCallback_) {
        context->framebufferSizeCallback_(window, width, height);
    }
}

void GLContext::setVSync(bool enabled) {
    if (window_) {
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

std::string GLContext::getGLVersion() const {
    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    return version ? version : "Unknown";
}

std::string GLContext::getGLRenderer() const {
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer ? renderer : "Unknown";
}

std::string GLContext::getGLSLVersion() const {
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    return glslVersion ? glslVersion : "Unknown";
}
