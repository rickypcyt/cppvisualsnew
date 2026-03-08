#include "visualizer.h"
#include <iostream>

void Visualizer::renderNoImGuiLoop() {
    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Render legacy visualization (works with software rendering)
    renderLegacyVisualization();
}
