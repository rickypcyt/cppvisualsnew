#ifndef VK_RENDERER_H
#define VK_RENDERER_H

#include "renderer_interface.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>
#include <string>
#include <optional>

// ============================================================
// MVP Vulkan Renderer - Minimal implementation
// Focus: Triangle → Frame Loop → SSBO → ImGui
// ============================================================
class VKRenderer : public IRenderer {
public:
    VKRenderer();
    ~VKRenderer() override;

    // IRenderer lifecycle
    bool initialize(int width, int height, const std::string& windowTitle) override;
    void shutdown() override;
    bool shouldClose() override;

    // Frame management
    void beginFrame() override;
    void endFrame() override;
    void swapBuffers() override;

    // Layer rendering (TODO: implement)
    void renderBackground(const AudioFeatures& features) override;
    void renderProceduralLayer(const AudioFeatures& features, int mode, float opacity) override;
    void renderPostProcessing(const std::vector<int>& effects) override;
    void renderUI() override;

    // Resource creation (TODO: implement)
    std::unique_ptr<IShader> createShader(ShaderType type) override;
    std::unique_ptr<ITexture> createTexture(int width, int height, TextureFormat format) override;
    std::unique_ptr<IFramebuffer> createFramebuffer(int width, int height) override;
    std::unique_ptr<IRenderLayer> createProceduralLayer() override;

    // State management (TODO: implement)
    void setViewport(int x, int y, int width, int height) override;
    void enableBlending(bool enable) override;
    void enableDepthTest(bool enable) override;

    // Hot reload (TODO: implement)
    void reloadShaders() override;
    void setShaderHotReload(bool enabled) override;

    // Info
    std::string getBackendName() const override { return "Vulkan"; }
    std::string getVersionString() const override;
    bool supportsComputeShaders() const override { return true; }

private:
    // MVP: Core Vulkan objects only
    GLFWwindow* window_;
    VkInstance instance_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkSurfaceKHR surface_;
    
    // MVP: Swapchain and frame rendering
    VkSwapchainKHR swapChain_;
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    std::vector<VkImageView> swapChainImageViews_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkRenderPass renderPass_;
    
    // MVP: Pipeline (fullscreen quad)
    VkPipelineLayout pipelineLayout_;
    VkPipeline graphicsPipeline_;
    
    // MVP: Command buffers and sync
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    
    // MVP: SSBO for audio data
    VkBuffer audioSSBO_;
    VkDeviceMemory audioSSBOMemory_;
    
    // MVP: Frame tracking
    uint32_t currentFrame_;
    bool framebufferResized_;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    
    // MVP: Validation layers
    const bool enableValidationLayers_ = true;
    VkDebugUtilsMessengerEXT debugMessenger_;
    
    // MVP: Helper functions
    bool createInstance();
    bool setupDebugMessenger();
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    bool createSurface();
    bool createSwapChain();
    bool createImageViews();
    bool createRenderPass();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createCommandPool();
    bool createCommandBuffers();
    bool createSyncObjects();
    bool createAudioSSBO(); // MVP: SSBO for dummy audio data
    void cleanupSwapChain();
    void cleanup();
    bool checkValidationLayerSupport();
};

#endif // VK_RENDERER_H
