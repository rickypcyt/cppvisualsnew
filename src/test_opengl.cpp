#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Triangle Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewInit()) << std::endl;
        // Continue anyway to test with basic OpenGL
        std::cout << "Continuing without GLEW..." << std::endl;
    }

    // Clear any GL errors
    glGetError();

    // Vertex data for triangle
    float vertices[] = {
        // positions         // colors
        0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // top vertex (red)
       -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom left (green)
        0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // bottom right (blue)
    };

    // Create VAO and VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    
    // Bind VAO
    glBindVertexArray(VAO);
    
    // Bind and fill VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    std::cout << "OpenGL Triangle Test Started!" << std::endl;
    std::cout << "You should see a colorful triangle in the window." << std::endl;
    std::cout << "Press ESC to exit." << std::endl;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Clear screen
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);  // Dark blue background
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw triangle
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Check for ESC key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
    }

    // Cleanup
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "OpenGL Triangle Test Completed." << std::endl;
    return 0;
}
