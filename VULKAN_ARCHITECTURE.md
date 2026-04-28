# Vulkan Architecture Design

## Core Design Principles

1. **Ownership Model**: Clear ownership of all Vulkan objects
2. **Frame Flow**: State machine for per-frame resource lifecycle
3. **Abstraction Layers**: Hide Vulkan complexity behind clean interfaces

---

## Ownership Model

### Object Ownership Rules

```
VKRenderer (owns):
  - VkInstance
  - VkPhysicalDevice
  - VkDevice
  - VkQueue
  - VkSurfaceKHR
  - VMA Allocator

VKSwapchain (owned by VKRenderer):
  - VkSwapchainKHR
  - VkImage[] (swapchain images)
  - VkImageView[]
  - VkFramebuffer[]

VKRenderPass (owned by VKRenderer):
  - VkRenderPass

VKPipeline (owned by VKRenderer):
  - VkPipeline
  - VkPipelineLayout
  - VkDescriptorSetLayout

VKShader (owned by VKPipeline):
  - VkShaderModule

VKBuffer (owned by VKRenderer or transient):
  - VkBuffer
  - VmaAllocation
  - void* mappedData

VKTexture (owned by VKRenderer or transient):
  - VkImage
  - VkImageView
  - VmaAllocation
  - VkSampler

VKCommandPool (per-frame, owned by VKRenderer):
  - VkCommandPool
  - VkCommandBuffer[]

VKDescriptorPool (owned by VKRenderer):
  - VkDescriptorPool
  - VkDescriptorSet[]
```

### Destruction Order (Critical)

```cpp
~VKRenderer() {
    // 1. Wait for device to finish
    vkDeviceWaitIdle(device_);

    // 2. Destroy per-frame resources
    for (auto& frame : frames_) {
        frame.destroy(device_);
    }

    // 3. Destroy pipeline objects
    pipelines_.clear();

    // 4. Destroy render pass
    vkDestroyRenderPass(device_, renderPass_, nullptr);

    // 5. Destroy swapchain
    swapchain_.destroy(device_);

    // 6. Destroy descriptor pools
    descriptorPool_.destroy(device_);

    // 7. Destroy command pools
    commandPool_.destroy(device_);

    // 8. Destroy VMA allocator
    vmaDestroyAllocator(allocator_);

    // 9. Destroy device
    vkDestroyDevice(device_, nullptr);

    // 10. Destroy surface
    vkDestroySurfaceKHR(instance_, surface_, nullptr);

    // 11. Destroy debug messenger
    // ...

    // 12. Destroy instance
    vkDestroyInstance(instance_, nullptr);
}
```

---

## Frame Flow State Machine

### Frame States

```
IDLE → ACQUIRE_IMAGE → RECORD_CMDS → SUBMIT → PRESENT → IDLE
```

### Per-Frame State

```cpp
struct VKFrame {
    // Synchronization
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence inFlight;

    // Command buffers
    VkCommandPool commandPool;
    VkCommandBuffer primaryCmdBuffer;

    // State tracking
    bool isRecording;
    bool isSubmitted;
    uint64_t frameNumber;

    // Methods
    void beginRecording();
    void endRecording();
    void submit(VkQueue queue, VkSemaphore wait, VkSemaphore signal);
    void waitForFence(VkDevice device);
    void reset(VkDevice device);
};
```

### Frame Flow Implementation

```cpp
class VKRenderer {
private:
    std::vector<VKFrame> frames_;
    uint32_t currentFrameIndex_ = 0;
    uint32_t frameInFlight_ = 0;

public:
    void beginFrame() {
        // 1. Wait for previous frame to finish
        VKFrame& frame = frames_[currentFrameIndex_];
        frame.waitForFence(device_);

        // 2. Acquire next image
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                             frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

        // 3. Begin command buffer recording
        frame.beginRecording();

        // 4. Begin render pass
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = swapchain_.getFramebuffer(imageIndex);
        // ...
        vkCmdBeginRenderPass(frame.primaryCmdBuffer, &renderPassInfo,
                           VK_SUBPASS_CONTENTS_INLINE);
    }

    void endFrame() {
        VKFrame& frame = frames_[currentFrameIndex_];

        // 1. End render pass
        vkCmdEndRenderPass(frame.primaryCmdBuffer);

        // 2. End command buffer recording
        frame.endRecording();

        // 3. Submit command buffer
        VkSubmitInfo submitInfo = {};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &frame.primaryCmdBuffer;
        // Wait for image available, signal render finished
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.imageAvailable;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &frame.renderFinished;
        frame.submit(graphicsQueue_, frame.imageAvailable, frame.renderFinished);

        // 4. Present
        VkPresentInfoKHR presentInfo = {};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &currentImageIndex_;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &frame.renderFinished;
        vkQueuePresentKHR(presentQueue_, &presentInfo);

        // 5. Advance frame index
        currentFrameIndex_ = (currentFrameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};
```

---

## Class Hierarchy

### Core Classes

```cpp
// Core renderer
class VKRenderer : public IRenderer {
    // Owns all Vulkan objects
    // Manages frame flow
    // Implements IRenderer interface
};

// Swapchain management
class VKSwapchain {
    VkSwapchainKHR handle_;
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    VkExtent2D extent_;

    void recreate(VkDevice device, VkSurfaceKHR surface, int width, int height);
    void destroy(VkDevice device);
};

// Memory management (VMA wrapper)
class VKMemoryAllocator {
    VmaAllocator allocator_;

    void* allocateBuffer(VkBufferCreateInfo& bufferInfo,
                        VmaMemoryUsage usage,
                        VkBuffer* buffer,
                        VmaAllocation* allocation);
    void* allocateImage(VkImageCreateInfo& imageInfo,
                       VmaMemoryUsage usage,
                       VkImage* image,
                       VmaAllocation* allocation);
    void free(VmaAllocation allocation);
};

// Buffer abstraction
class VKBuffer {
    VkBuffer handle_;
    VmaAllocation allocation_;
    void* mappedData_;
    VkDeviceSize size_;

    void setData(const void* data, size_t size);
    void* map();
    void unmap();
};

// Texture abstraction
class VKTexture {
    VkImage handle_;
    VkImageView view_;
    VmaAllocation allocation_;
    VkSampler sampler_;
    int width_, height_;

    void setData(const void* data, int width, int height);
};

// Shader wrapper
class VKShader : public IShader {
    VkShaderModule vertexModule_;
    VkShaderModule fragmentModule_;
    VkPipeline pipeline_;
    VkPipelineLayout layout_;

    bool loadFromSPIRV(const std::vector<char>& vertSPIRV,
                      const std::vector<char>& fragSPIRV);
};

// Per-frame resources
class VKFrame {
    VkSemaphore imageAvailable_;
    VkSemaphore renderFinished_;
    VkFence inFlight_;
    VkCommandPool commandPool_;
    VkCommandBuffer primaryCmdBuffer_;

    void beginRecording();
    void endRecording();
};
```

---

## Resource Lifecycle

### Creation Pattern

```cpp
// All resources follow this pattern:
class VKResource {
protected:
    bool initialized_ = false;

public:
    virtual ~VKResource() {
        if (initialized_) {
            destroy();
        }
    }

    virtual bool initialize() = 0;
    virtual void destroy() = 0;
};
```

### Transient Resources

```cpp
class VKTransientBuffer : public VKBuffer {
    // Automatically destroyed after use
    // Used for per-frame temporary data
};

class VKTransientTexture : public VKTexture {
    // Automatically destroyed after use
    // Used for render targets
};
```

---

## Descriptor Management

### Descriptor Set Layout Strategy

```cpp
// Global descriptor set (bound once)
struct GlobalDescriptorSet {
    // Camera uniforms
    // Time uniforms
    // Screen resolution
};

// Per-material descriptor set (bound per draw)
struct MaterialDescriptorSet {
    // Textures
    // Material properties
};

// Per-object descriptor set (optional)
struct ObjectDescriptorSet {
    // Transform matrices
    // Object-specific data
};
```

### Descriptor Pool Management

```cpp
class VKDescriptorPool {
    VkDescriptorPool pool_;
    std::vector<VkDescriptorSet> freeSets_;

    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
    void free(VkDescriptorSet set);
    void reset(); // Free all sets at once
};
```

---

## Pipeline Management

### Pipeline Cache

```cpp
class VKPipelineCache {
    VkPipelineCache cache_;

    void loadFromFile(const std::string& path);
    void saveToFile(const std::string& path);
    VkPipelineCache getHandle();
};
```

### Pipeline Creation

```cpp
class VKPipelineBuilder {
    VkGraphicsPipelineCreateInfo info_;

    VKPipelineBuilder& setVertexShader(VkShaderModule shader);
    VKPipelineBuilder& setFragmentShader(VkShaderModule shader);
    VKPipelineBuilder& setRenderPass(VkRenderPass renderPass);
    VKPipelineBuilder& setDescriptorSetLayout(VkDescriptorSetLayout layout);
    VkPipeline build(VkDevice device, VkPipelineCache cache);
};
```

---

## Audio Data Flow (SSBO)

### CPU Side

```cpp
class AudioDataBuffer {
    std::vector<float> ringBuffer_;
    size_t writeIndex_ = 0;
    size_t bufferSize_ = 4096; // FFT size

    void pushAudioData(const AudioFeatures& features) {
        // Write to ring buffer
        ringBuffer_[writeIndex_ % bufferSize_] = features.energy;
        ringBuffer_[(writeIndex_ + 1) % bufferSize_] = features.bassEnergy;
        // ... more features
        writeIndex_++;
    }
};
```

### GPU Side

```cpp
class VKAudioBuffer {
    VKBuffer ssbo_; // Storage buffer

    void update(const AudioDataBuffer& audioData) {
        // Single upload per frame
        ssbo_.setData(audioData.data(), audioData.size());
    }

    void bind(VkCommandBuffer cmd, VkPipelineLayout layout) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = ssbo_.getHandle();
        bufferInfo.range = VK_WHOLE_SIZE;

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               layout, 0, 1, &descriptorSet_, 0, nullptr);
    }
};
```

---

## Error Handling Strategy

### Validation Layers

```cpp
#ifdef DEBUG
#define ENABLE_VALIDATION_LAYERS 1
#else
#define ENABLE_VALIDATION_LAYERS 0
#endif

class VKDebugMessenger {
    VkDebugUtilsMessengerEXT messenger_;

    void setup(VkInstance instance);
    void destroy(VkInstance instance);
};
```

### Error Checking Macro

```cpp
#define VK_CHECK(result) \
    if (result != VK_SUCCESS) { \
        std::cerr << "Vulkan error: " << result << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        throw std::runtime_error("Vulkan error"); \
    }
```

---

## Initialization Sequence

```cpp
bool VKRenderer::initialize(int width, int height, const std::string& title) {
    // 1. Create window
    window_ = createWindow(title, width, height);

    // 2. Create instance
    instance_ = createInstance();

    // 3. Setup debug messenger
    debugMessenger_.setup(instance_);

    // 4. Create surface
    surface_ = createSurface(instance_, window_);

    // 5. Pick physical device
    physicalDevice_ = pickPhysicalDevice(instance_, surface_);

    // 6. Create logical device
    device_ = createDevice(physicalDevice_, surface_);

    // 7. Get queues
    graphicsQueue_ = getDeviceQueue(device_, graphicsQueueFamily_);
    presentQueue_ = getDeviceQueue(device_, presentQueueFamily_);

    // 8. Create VMA allocator
    allocator_ = createVMAAllocator(instance_, device_, physicalDevice_);

    // 9. Create swapchain
    swapchain_.create(device_, surface_, width, height);

    // 10. Create render pass
    renderPass_ = createRenderPass(device_, swapchain_.getImageFormat());

    // 11. Create framebuffers
    swapchain_.createFramebuffers(device_, renderPass_);

    // 12. Create command pools
    for (auto& frame : frames_) {
        frame.createCommandPool(device_, graphicsQueueFamily_);
    }

    // 13. Create descriptor pool
    descriptorPool_.create(device_);

    // 14. Create synchronization objects
    for (auto& frame : frames_) {
        frame.createSyncObjects(device_);
    }

    return true;
}
```

---

## Next Steps

1. Implement VKMemoryAllocator wrapper around VMA
2. Implement VKSwapchain with recreation logic
3. Implement VKFrame with state machine
4. Implement basic VKPipeline creation
5. Create MVP: fullscreen quad with audio SSBO
