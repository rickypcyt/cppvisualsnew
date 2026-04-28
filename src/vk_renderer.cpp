#include "vk_renderer.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <cstring>

// ============================================================
// VKRenderer Implementation
// ============================================================

VKRenderer::VKRenderer()
    : instance_(VK_NULL_HANDLE)
    , debugMessenger_(VK_NULL_HANDLE)
    , physicalDevice_(VK_NULL_HANDLE)
    , device_(VK_NULL_HANDLE)
    , graphicsQueue_(VK_NULL_HANDLE)
    , presentQueue_(VK_NULL_HANDLE)
    , surface_(VK_NULL_HANDLE)
    , swapChain_(VK_NULL_HANDLE)
    , renderPass_(VK_NULL_HANDLE)
    , pipelineLayout_(VK_NULL_HANDLE)
    , graphicsPipeline_(VK_NULL_HANDLE)
    , commandPool_(VK_NULL_HANDLE)
    , window_(nullptr)
    , width_(0)
    , height_(0)
    , shaderHotReload_(false)
    , currentFrame_(0)
    , framebufferResized_(false) {
}

VKRenderer::~VKRenderer() {
    cleanup();
}

bool VKRenderer::initialize(int width, int height, const std::string& windowTitle) {
    width_ = width;
    height_ = height;
    windowTitle_ = windowTitle;

    if (!glfwInit()) {
        std::cerr << "[VKRenderer] Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // No OpenGL
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window_ = glfwCreateWindow(width, height, windowTitle.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[VKRenderer] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
        auto renderer = static_cast<VKRenderer*>(glfwGetWindowUserPointer(window));
        renderer->framebufferResized_ = true;
    });

    if (!createInstance()) {
        return false;
    }

    if (enableValidationLayers_ && !setupDebugMessenger()) {
        return false;
    }

    if (!createSurface()) {
        return false;
    }

    if (!pickPhysicalDevice()) {
        return false;
    }

    if (!createLogicalDevice()) {
        return false;
    }

    if (!createSwapChain()) {
        return false;
    }

    if (!createImageViews()) {
        return false;
    }

    if (!createRenderPass()) {
        return false;
    }

    if (!createGraphicsPipeline()) {
        return false;
    }

    if (!createFramebuffers()) {
        return false;
    }

    if (!createCommandPool()) {
        return false;
    }

    if (!createCommandBuffers()) {
        return false;
    }

    if (!createSyncObjects()) {
        return false;
    }

    std::cout << "[VKRenderer] Initialized: " << getVersionString() << std::endl;
    return true;
}

void VKRenderer::shutdown() {
    cleanup();
}

bool VKRenderer::shouldClose() {
    return window_ && glfwWindowShouldClose(window_);
}

void VKRenderer::beginFrame() {
    // Frame begin placeholder
}

void VKRenderer::endFrame() {
    // Frame end placeholder
}

void VKRenderer::swapBuffers() {
    // Vulkan handles swap differently than OpenGL
}

// Placeholder implementations
void VKRenderer::renderBackground(const AudioFeatures& features) {
    std::cerr << "[VKRenderer] renderBackground not implemented" << std::endl;
}

void VKRenderer::renderProceduralLayer(const AudioFeatures& features, int mode, float opacity) {
    std::cerr << "[VKRenderer] renderProceduralLayer not implemented" << std::endl;
}

void VKRenderer::renderPostProcessing(const std::vector<int>& effects) {
    std::cerr << "[VKRenderer] renderPostProcessing not implemented" << std::endl;
}

void VKRenderer::renderUI() {
    std::cerr << "[VKRenderer] renderUI not implemented" << std::endl;
}

std::unique_ptr<IShader> VKRenderer::createShader(ShaderType type) {
    std::cerr << "[VKRenderer] createShader not implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<ITexture> VKRenderer::createTexture(int width, int height, TextureFormat format) {
    std::cerr << "[VKRenderer] createTexture not implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<IFramebuffer> VKRenderer::createFramebuffer(int width, int height) {
    std::cerr << "[VKRenderer] createFramebuffer not implemented" << std::endl;
    return nullptr;
}

std::unique_ptr<IRenderLayer> VKRenderer::createProceduralLayer() {
    std::cerr << "[VKRenderer] createProceduralLayer not implemented" << std::endl;
    return nullptr;
}

void VKRenderer::setViewport(int x, int y, int width, int height) {
    std::cerr << "[VKRenderer] setViewport not implemented" << std::endl;
}

void VKRenderer::enableBlending(bool enable) {
    std::cerr << "[VKRenderer] enableBlending not implemented" << std::endl;
}

void VKRenderer::enableDepthTest(bool enable) {
    std::cerr << "[VKRenderer] enableDepthTest not implemented" << std::endl;
}

void VKRenderer::reloadShaders() {
    std::cerr << "[VKRenderer] reloadShaders not implemented" << std::endl;
}

void VKRenderer::setShaderHotReload(bool enabled) {
    shaderHotReload_ = enabled;
}

std::string VKRenderer::getVersionString() const {
    uint32_t apiVersion;
    vkEnumerateInstanceVersion(&apiVersion);
    return "Vulkan " + std::to_string(VK_VERSION_MAJOR(apiVersion)) + "." +
           std::to_string(VK_VERSION_MINOR(apiVersion)) + "." +
           std::to_string(VK_VERSION_PATCH(apiVersion));
}

// ============================================================
// Vulkan Initialization Functions
// ============================================================

bool VKRenderer::createInstance() {
    if (enableValidationLayers_ && !checkValidationLayerSupport()) {
        std::cerr << "[VKRenderer] Validation layers requested but not available" << std::endl;
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Audio Visualizer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create Vulkan instance" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::setupDebugMessenger() {
    if (!enableValidationLayers_) return true;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                     VkDebugUtilsMessageTypeFlagsEXT messageType,
                                     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                     void* pUserData) -> VkBool32 {
        std::cerr << "[VK Validation] " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    };

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance_, "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr || func(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to set up debug messenger" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::createSurface() {
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create window surface" << std::endl;
        return false;
    }
    return true;
}

bool VKRenderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

    if (deviceCount == 0) {
        std::cerr << "[VKRenderer] Failed to find GPUs with Vulkan support" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            physicalDevice_ = device;
            break;
        }
    }

    if (physicalDevice_ == VK_NULL_HANDLE) {
        std::cerr << "[VKRenderer] Failed to find a suitable GPU" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::isDeviceSuitable(VkPhysicalDevice device) {
    uint32_t queueFamily = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    return queueFamily != UINT32_MAX && extensionsSupported;
}

uint32_t VKRenderer::findQueueFamilies(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport) {
            return i;
        }
    }

    return UINT32_MAX;
}

bool VKRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions_.begin(), deviceExtensions_.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool VKRenderer::createLogicalDevice() {
    uint32_t queueFamilyIndex = findQueueFamilies(physicalDevice_);

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions_.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions_.data();

    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers_.size());
        createInfo.ppEnabledLayerNames = validationLayers_.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create logical device" << std::endl;
        return false;
    }

    vkGetDeviceQueue(device_, queueFamilyIndex, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queueFamilyIndex, 0, &presentQueue_);

    return true;
}

bool VKRenderer::createSwapChain() {
    // Simplified swap chain creation - would need proper query in full implementation
    VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    VkExtent2D extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)};

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create swap chain" << std::endl;
        return false;
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
    swapChainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

    swapChainImageFormat_ = surfaceFormat.format;
    swapChainExtent_ = extent;

    return true;
}

bool VKRenderer::createImageViews() {
    swapChainImageViews_.resize(swapChainImages_.size());

    for (size_t i = 0; i < swapChainImages_.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages_[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat_;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
            std::cerr << "[VKRenderer] Failed to create image views" << std::endl;
            return false;
        }
    }

    return true;
}

bool VKRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create render pass" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::createGraphicsPipeline() {
    // Placeholder - would need actual shaders in SPIR-V format
    // For now, create a minimal pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create pipeline layout" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::createFramebuffers() {
    swapChainFramebuffers_.resize(swapChainImageViews_.size());

    for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
        VkImageView attachments[] = {swapChainImageViews_[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent_.width;
        framebufferInfo.height = swapChainExtent_.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS) {
            std::cerr << "[VKRenderer] Failed to create framebuffer" << std::endl;
            return false;
        }
    }

    return true;
}

bool VKRenderer::createCommandPool() {
    uint32_t queueFamilyIndex = findQueueFamilies(physicalDevice_);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to create command pool" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::createCommandBuffers() {
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        std::cerr << "[VKRenderer] Failed to allocate command buffers" << std::endl;
        return false;
    }

    return true;
}

bool VKRenderer::createSyncObjects() {
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            std::cerr << "[VKRenderer] Failed to create synchronization objects" << std::endl;
            return false;
        }
    }

    return true;
}

bool VKRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers_) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void VKRenderer::cleanup() {
    if (device_) {
        vkDeviceWaitIdle(device_);
    }

    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (inFlightFences_[i]) vkDestroyFence(device_, inFlightFences_[i], nullptr);
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
    }

    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);

    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (debugMessenger_) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(instance_, debugMessenger_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void VKRenderer::cleanupSwapChain() {
    for (auto framebuffer : swapChainFramebuffers_) {
        if (framebuffer) vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    for (auto imageView : swapChainImageViews_) {
        if (imageView) vkDestroyImageView(device_, imageView, nullptr);
    }
    if (swapChain_) vkDestroySwapchainKHR(device_, swapChain_, nullptr);
}
