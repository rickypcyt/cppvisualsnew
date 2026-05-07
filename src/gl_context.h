#ifndef GL_CONTEXT_H
#define GL_CONTEXT_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional>

// ============================================================
// GLContext - Encapsula GLFW, GLEW y el contexto OpenGL
// 
// Responsabilidades:
// - Inicialización y limpieza de GLFW
// - Creación y gestión de ventanas GLFW
// - Inicialización de GLEW
// - Gestión del contexto OpenGL
// 
// Esta clase separa la gestión del contexto del renderer,
// permitiendo que tanto GLRenderer como ImGui compartan
// el mismo contexto sin duplicar código.
// ============================================================
class GLContext {
public:
    GLContext();
    ~GLContext();

    // Inicialización global de GLFW (llamar una sola vez al inicio)
    static bool initializeGLFW();
    static void shutdownGLFW();

    // Crear ventana y contexto OpenGL
    bool createWindow(int width, int height, const std::string& title, GLFWmonitor* monitor = nullptr, GLFWwindow* share = nullptr);
    
    // Destruir ventana
    void destroyWindow();

    // Hacer el contexto actual
    void makeCurrent() const;

    // Intercambiar buffers
    void swapBuffers() const;

    // Consultas
    bool shouldClose() const;
    GLFWwindow* getWindow() const { return window_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    bool isValid() const { return window_ != nullptr; }

    // Configuración de ventana
    void setWindowTitle(const std::string& title);
    void setWindowSize(int width, int height);
    void setWindowPosition(int x, int y);
    void setWindowFullscreen(bool fullscreen);
    void setWindowVisible(bool visible);
    void setWindowIconified(bool iconified);

    // Consultas de estado
    bool isWindowVisible() const;
    bool isWindowIconified() const;
    bool isWindowFullscreen() const;
    GLFWmonitor* getWindowMonitor() const;

    // Input mode (cursor, etc.)
    void setInputMode(int mode, int value);
    int getInputMode(int mode) const;

    // Callbacks
    using FramebufferSizeCallback = std::function<void(GLFWwindow*, int, int)>;
    void setFramebufferSizeCallback(FramebufferSizeCallback callback);

    // VSync
    void setVSync(bool enabled);

    // Información de OpenGL
    std::string getGLVersion() const;
    std::string getGLRenderer() const;
    std::string getGLSLVersion() const;

private:
    GLFWwindow* window_;
    int width_;
    int height_;
    static bool glfwInitialized_;
    FramebufferSizeCallback framebufferSizeCallback_;

    // Callback estático para GLFW
    static void framebufferSizeCallbackStatic(GLFWwindow* window, int width, int height);
};

#endif // GL_CONTEXT_H
