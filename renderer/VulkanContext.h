#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vulkan/vulkan.h>
#include <optional>
#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include <string>

// Engine component headers
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Array.h"
#include "Descriptors.h" 
#include "Pipeline.h"    

constexpr int MAX_SWAPCHAIN_IMAGES = 4;

struct SubMesh {
    uint32_t startIndex;
    uint32_t indexCount;
    uint32_t textureIndex;
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    Array<VkSurfaceFormatKHR, 64> formats{};
    Array<VkPresentModeKHR, 16> presentModes{};
    int formatCount = 0;
    int presentModeCount = 0;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct UniformBufferObject {
    Matrix4x4 view;
    Matrix4x4 projection;
};

struct PushConstantData {
    Matrix4x4 modelMatrix;
    uint32_t textureIndex;
};

// Each render submission specifies its transform AND which mesh slot in VRAM to draw
struct RenderObject {
    Matrix4x4 transform;
    uint32_t meshID = 0;
};

// Represents one independent model stored in VRAM
struct VulkanMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    std::vector<SubMesh> subMeshes;
};

class Camera;

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    void initVulkan(HWND hwnd);
    void createSurface(HWND hwnd);
    void listGPUs();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void initSwapChain();
    void drawFrame(const Matrix4x4& viewMatrix, Camera* camera, const std::vector<RenderObject>& sceneObjects);
    void setResizeFlag() { framebufferResized = true; }
    void beginUI();

    // Uploads a GLB to a new VRAM slot and returns its meshID
    uint32_t uploadMesh(const std::string& filename);
    void freeMesh(uint32_t meshID);

private:
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    void createImGuiDescriptorPool();
    void initImGui();
    std::unique_ptr<Pipeline> wireframePipeline;
    bool useWireframe = false;
    bool useVSync = true;
    bool enableCulling = false;

    void rebuildPipelines();

    // Window & OS States
    HWND windowHandle = nullptr;
    bool framebufferResized = false;
    void updateUniformBuffer(uint32_t currentImage, const Matrix4x4& viewMatrix, float modelScale);

    // Core Vulkan Instance & Hardware Handles
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    // Swapchain Subsystem Handles
    VkSwapchainKHR swapChain;
    unsigned int activeImageCount = 0;
    Array<VkImage, MAX_SWAPCHAIN_IMAGES> swapChainImages{};
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    Array<VkImageView, MAX_SWAPCHAIN_IMAGES> swapChainImageViews{};
    Array<VkFramebuffer, MAX_SWAPCHAIN_IMAGES> swapChainFramebuffers{};

    // Global Render Abstractions
    VkRenderPass renderPass;

    // RAII Subsystem Unique Pointers
    std::unique_ptr<Descriptors> descriptors;
    std::unique_ptr<Pipeline> graphicsPipeline;

    // Depth Stencil Buffers
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // VRAM Warehouse: all loaded models live here
    std::vector<VulkanMesh> loadedMeshes;
    std::vector<uint32_t> freeMeshSlots; // Recycled indices

    // Legacy single-buffer hooks (to be phased out)
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t currentVertexCount = 0;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    uint32_t currentIndexCount = 0;

    void createIndexBuffer(const std::vector<uint32_t>& indices);
    void cleanupIndexBuffer();

    // Uniform Buffers
    Array<VkBuffer, MAX_SWAPCHAIN_IMAGES> uniformBuffers{};
    Array<VkDeviceMemory, MAX_SWAPCHAIN_IMAGES> uniformBuffersMemory{};
    Array<void*, MAX_SWAPCHAIN_IMAGES> uniformBuffersMapped{};

    // Command Management
    VkCommandPool commandPool = VK_NULL_HANDLE;
    Array<VkCommandBuffer, MAX_SWAPCHAIN_IMAGES> commandBuffers{};

    // Sync Objects
    Array<VkSemaphore, MAX_SWAPCHAIN_IMAGES> imageAvailableSemaphores{};
    Array<VkSemaphore, MAX_SWAPCHAIN_IMAGES> renderFinishedSemaphores{};
    Array<VkFence, MAX_SWAPCHAIN_IMAGES> inFlightFences{};
    size_t currentFrame = 0;

    // Low-Level Internal Query Helpers
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const SwapChainSupportDetails& support);
    VkPresentModeKHR chooseSwapPresentMode(const SwapChainSupportDetails& support);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    std::deque<std::function<void()>> deletionQueue;

    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createDepthResources();
    void createUniformBuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    std::vector<std::string> availableMeshes;
    int currentMeshIndex = 0;
    float currentMeshScale = 1.0f;

    void scanAssetsFolder();
    void cleanupVertexBuffer();

    // Texture Pipeline
    std::vector<VkImage> textureImages;
    std::vector<VkDeviceMemory> textureImageMemories;
    std::vector<VkImageView> textureImageViews;
    VkSampler textureSampler = VK_NULL_HANDLE;
    std::vector<SubMesh> currentSubMeshes;

    void createTextureImage(unsigned char* pixels, int texWidth, int texHeight);

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderObject>& sceneObjects);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void cleanupSwapChain();
    void recreateSwapChain();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void cleanupTexture();
    void createTextureSampler();

    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(VkFormat* candidates, int candidateCount, VkImageTiling tiling, VkFormatFeatureFlags features);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};