#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

int main() {
    std::cout << "=== OpenGL Triangle Test ===" << std::endl;
    
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create window with software rendering hint
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);  // Use OpenGL 2.1 for compatibility
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);  // Don't force core profile
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Triangle Test - Press ESC", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
        std::cout << "Continuing without GLEW..." << std::endl;
    } else {
        std::cout << "GLEW initialized successfully!" << std::endl;
    }

    // Print OpenGL info
    const char* version = (const char*)glGetString(GL_VERSION);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    std::cout << "OpenGL Version: " << (version ? version : "Unknown") << std::endl;
    std::cout << "OpenGL Renderer: " << (renderer ? renderer : "Unknown") << std::endl;

    // Clear any GL errors
    glGetError();

    // Simple triangle vertices (using legacy OpenGL for maximum compatibility)
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);  // Red
    glVertex2f(0.0f, 0.5f);        // Top
    
    glColor3f(0.0f, 1.0f, 0.0f);  // Green
    glVertex2f(-0.5f, -0.5f);      // Bottom left
    
    glColor3f(0.0f, 0.0f, 1.0f);  // Blue
    glVertex2f(0.5f, -0.5f);       // Bottom right
    glEnd();

    std::cout << "Triangle rendered!" << std::endl;
    std::cout << "You should see a colorful triangle in the window." << std::endl;
    std::cout << "Press ESC or close window to exit." << std::endl;

    // Render loop - keep window open
    int frameCount = 0;
    while (!glfwWindowShouldClose(window)) {
        // Clear screen with dark blue background
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangle
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f);  // Red
        glVertex2f(0.0f, 0.5f);        // Top
        
        glColor3f(0.0f, 1.0f, 0.0f);  // Green
        glVertex2f(-0.5f, -0.5f);      // Bottom left
        
        glColor3f(0.0f, 0.0f, 1.0f);  // Blue
        glVertex2f(0.5f, -0.5f);       // Bottom right
        glEnd();

        // Add some animation - rotate triangle
        float time = glfwGetTime();
        float rotation = sinf(time) * 0.1f;
        glRotatef(rotation * 57.3f, 0.0f, 0.0f, 1.0f);  // Convert to degrees

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Check for ESC key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            std::cout << "ESC pressed, exiting..." << std::endl;
            break;
        }

        frameCount++;
        if (frameCount % 60 == 0) {  // Print every 60 frames
            std::cout << "Rendering frame " << frameCount << std::endl;
        }
    }

    std::cout << "OpenGL Triangle Test Completed. Rendered " << frameCount << " frames." << std::endl;

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
