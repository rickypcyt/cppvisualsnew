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
// VKRenderer - Vulkan implementation of IRenderer
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
    // Vulkan initialization
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

    // Helper functions
    bool isDeviceSuitable(VkPhysicalDevice device);
    uint32_t findQueueFamilies(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkValidationLayerSupport();

    // Cleanup helpers
    void cleanupSwapChain();
    void cleanup();

    // Vulkan objects
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debugMessenger_;
    VkPhysicalDevice physicalDevice_;
    VkDevice device_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapChain_;
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    std::vector<VkImageView> swapChainImageViews_;
    VkRenderPass renderPass_;
    VkPipelineLayout pipelineLayout_;
    VkPipeline graphicsPipeline_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;

    GLFWwindow* window_;
    int width_;
    int height_;
    std::string windowTitle_;
    bool shaderHotReload_;

    uint32_t currentFrame_;
    bool framebufferResized_;

    // Constants
    const int MAX_FRAMES_IN_FLIGHT = 2;
    const std::vector<const char*> validationLayers_ = {
        "VK_LAYER_KHRONOS_validation"
    };
    const std::vector<const char*> deviceExtensions_ = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool enableValidationLayers_ = false;
#else
    const bool enableValidationLayers_ = true;
#endif
};

#endif // VK_RENDERER_H
