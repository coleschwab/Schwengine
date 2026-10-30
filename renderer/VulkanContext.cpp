#include "VulkanContext.h"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <chrono>
#include "Vertex.h"
#include "Array.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_vulkan.h"
#include "camera.h"
#include "stb_image.h"
#include <unordered_map>
#include <filesystem>
#include <string>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

struct TextureData {
    unsigned char* pixels;
    int width, height, channels;
};

struct ModelData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;
    std::vector<TextureData> textures;
};

ModelData loadModel(const std::string& filepath, float scale) {
    ModelData model;

    cgltf_options options = {};
    cgltf_data* data = NULL;

    // 1. Parse the JSON/Header structure of the .glb file
    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);
    if (result != cgltf_result_success) {
        throw std::runtime_error("Failed to parse glTF file: " + filepath);
    }

    // 2. Load the actual binary data (Vertices, Textures, etc.) into memory
    result = cgltf_load_buffers(&options, data, filepath.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("Failed to load glTF buffers: " + filepath);
    }

    // --- EXTRACT EMBEDDED IMAGES ---
    for (cgltf_size i = 0; i < data->images_count; ++i) {
        cgltf_image* image = &data->images[i];
        TextureData texData{ nullptr, 0, 0, 0 };

        if (image->buffer_view) {
            uint8_t* bufferData = (uint8_t*)image->buffer_view->buffer->data + image->buffer_view->offset;
            // stbi_load_from_memory decodes the raw binary directly from RAM!
            texData.pixels = stbi_load_from_memory(bufferData, (int)image->buffer_view->size, &texData.width, &texData.height, &texData.channels, STBI_rgb_alpha);
        }
        model.textures.push_back(texData);
    }

    // 3. Prove it worked!
    std::cout << "Successfully loaded glTF!" << std::endl;
    std::cout << "Found " << data->meshes_count << " mesh(es) and " << data->materials_count << " material(s)." << std::endl;

    uint32_t vertexOffset = 0;
    uint32_t indexOffset = 0;

    // 1. Loop through the NODES, not the raw meshes!
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        cgltf_node* node = &data->nodes[i];

        // Skip nodes that don't contain geometry (like cameras or empty attachment points)
        if (!node->mesh) continue;

        cgltf_mesh* mesh = node->mesh;

        // Get the exact world position/rotation matrix of this specific car part
        float transformMat[16];
        cgltf_node_transform_world(node, transformMat);

        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            // Failsafe: Ignore 2D helper lines or splines left by the 3D artist
            if (primitive->type != cgltf_primitive_type_triangles) continue;

            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;
            cgltf_accessor* uvAccessor = nullptr;

            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                cgltf_attribute* attr = &primitive->attributes[k];
                if (attr->type == cgltf_attribute_type_position) posAccessor = attr->data;
                else if (attr->type == cgltf_attribute_type_normal) normAccessor = attr->data;
                else if (attr->type == cgltf_attribute_type_texcoord) uvAccessor = attr->data;
            }

            if (!posAccessor || !primitive->indices) continue;

            // Setup the SubMesh for our engine
            SubMesh subMesh{};
            subMesh.startIndex = indexOffset;
            subMesh.indexCount = static_cast<uint32_t>(primitive->indices->count);
            subMesh.textureIndex = 0; // Default fallback

            // --- NEW: Default to white if the artist didn't specify a color ---
            float matColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

            // Map the material to the exact texture array index AND grab the paint color!
            if (primitive->material && primitive->material->has_pbr_metallic_roughness) {

                // 1. Check for Image Textures (Decals, License plates, etc.)
                cgltf_texture_view* baseColorTex = &primitive->material->pbr_metallic_roughness.base_color_texture;
                if (baseColorTex->texture && baseColorTex->texture->image) {
                    subMesh.textureIndex = static_cast<uint32_t>(baseColorTex->texture->image - data->images);
                }

                // 2. Check for Mathematical Paint Colors (Car body panels)
                cgltf_float* factor = primitive->material->pbr_metallic_roughness.base_color_factor;
                matColor[0] = factor[0];
                matColor[1] = factor[1];
                matColor[2] = factor[2];
                matColor[3] = factor[3];
            }

            if (primitive->material && primitive->material->has_pbr_metallic_roughness) {
                cgltf_texture_view* baseColor = &primitive->material->pbr_metallic_roughness.base_color_texture;
                if (baseColor->texture && baseColor->texture->image) {
                    subMesh.textureIndex = static_cast<uint32_t>(baseColor->texture->image - data->images);
                }
            }

            // 3. Extract and BAKE the vertices
            for (cgltf_size v = 0; v < posAccessor->count; ++v) {
                Vertex vertex{};

                float pos[3] = { 0, 0, 0 };
                cgltf_accessor_read_float(posAccessor, v, pos, 3);

                // BAKE THE TRANSFORM: Multiply the vertex position by the part's matrix
                float worldX = transformMat[0] * pos[0] + transformMat[4] * pos[1] + transformMat[8] * pos[2] + transformMat[12];
                float worldY = transformMat[1] * pos[0] + transformMat[5] * pos[1] + transformMat[9] * pos[2] + transformMat[13];
                float worldZ = transformMat[2] * pos[0] + transformMat[6] * pos[1] + transformMat[10] * pos[2] + transformMat[14];

                vertex.pos = { worldX * scale, worldY * scale, worldZ * scale, 1.0f };

                if (normAccessor) {
                    float norm[3] = { 0, 0, 0 };
                    cgltf_accessor_read_float(normAccessor, v, norm, 3);

                    // Rotate the normals so lighting doesn't break if the part was rotated
                    float normX = transformMat[0] * norm[0] + transformMat[4] * norm[1] + transformMat[8] * norm[2];
                    float normY = transformMat[1] * norm[0] + transformMat[5] * norm[1] + transformMat[9] * norm[2];
                    float normZ = transformMat[2] * norm[0] + transformMat[6] * norm[1] + transformMat[10] * norm[2];

                    vertex.normal = { normX, normY, normZ, 0.0f };
                }
                else {
                    vertex.normal = { 0.0f, 1.0f, 0.0f, 0.0f };
                }

                if (uvAccessor) {
                    float uv[2] = { 0, 0 };
                    cgltf_accessor_read_float(uvAccessor, v, uv, 2);
                    vertex.texCoord = { uv[0], uv[1] };
                }
                else {
                    vertex.texCoord = { 0.0f, 0.0f };
                }

                vertex.color = { matColor[0], matColor[1], matColor[2], matColor[3] };
                model.vertices.push_back(vertex);
            }

            // 4. Extract Indices
            for (cgltf_size idx = 0; idx < primitive->indices->count; ++idx) {
                uint32_t index = cgltf_accessor_read_index(primitive->indices, idx);
                model.indices.push_back(index + vertexOffset);
            }

            model.subMeshes.push_back(subMesh);

            vertexOffset += static_cast<uint32_t>(posAccessor->count);
            indexOffset += static_cast<uint32_t>(primitive->indices->count);
        }
    }
    // 4. Clean up the library's memory allocation
    cgltf_free(data);

    return model;
}

VulkanContext::VulkanContext()
    : instance(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE),
    physicalDevice(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE),
    graphicsQueue(VK_NULL_HANDLE),
    presentQueue(VK_NULL_HANDLE),
    swapChain(VK_NULL_HANDLE),
    swapChainImageFormat(VK_FORMAT_UNDEFINED),
    renderPass(VK_NULL_HANDLE),
    depthImage(VK_NULL_HANDLE),
    depthImageMemory(VK_NULL_HANDLE),
    depthImageView(VK_NULL_HANDLE),
    vertexBuffer(VK_NULL_HANDLE),
    vertexBufferMemory(VK_NULL_HANDLE),
    activeImageCount(0),
    currentVertexCount(0)
{
    swapChainExtent = { 0, 0 };
}

VulkanContext::~VulkanContext() {
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    graphicsPipeline.reset();
    wireframePipeline.reset();
    descriptors.reset();

    cleanupVertexBuffer();
    cleanupIndexBuffer();
    cleanupTexture();
    cleanupSwapChain();

    for (auto it = deletionQueue.rbegin(); it != deletionQueue.rend(); ++it) {
        (*it)();
    }
    deletionQueue.clear();
}

void VulkanContext::initVulkan(HWND hwnd) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Schwengine";
    appInfo.apiVersion = VK_API_VERSION_1_3;
    windowHandle = hwnd;

    const char* extensionNames[] = { "VK_KHR_surface", "VK_KHR_win32_surface" };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensionNames;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    deletionQueue.push_back([this]() { vkDestroyInstance(instance, nullptr); });

    createSurface(hwnd);
    deletionQueue.push_back([this]() { vkDestroySurfaceKHR(instance, surface, nullptr); });

    pickPhysicalDevice();
    createLogicalDevice();
    initSwapChain();
    createImageViews();
    createRenderPass();
    createDepthResources();
    createFramebuffers();

    // --- CRITICAL FIX: Command pool MUST exist before we load textures! ---
    createCommandPool();
    createCommandBuffers();

    scanAssetsFolder();

    // --- REORDERED SECTION ---

    // 1. Create uniform buffers first
    createUniformBuffers();

    // 2. Initialize Descriptors BEFORE loading the mesh!
    // Note: We safely pass VK_NULL_HANDLE for the sampler here because 
    // loadMeshIntoBuffer will create the real one in a millisecond.
    descriptors = std::make_unique<Descriptors>(
        device,
        activeImageCount,
        uniformBuffers.data(),
        sizeof(UniformBufferObject),
        std::vector<VkImageView>{},
        VK_NULL_HANDLE
    );

    // --- END REORDERED SECTION ---
     
    rebuildPipelines();

    createSyncObjects();
    createImGuiDescriptorPool();
    initImGui();

    std::cout << "Schwengine Graphics Pipeline fully initialized!" << std::endl;
}


void VulkanContext::listGPUs() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("Failed to find GPUs with Vulkan support!");

    Array<VkPhysicalDevice, 16> devices{};
    if (deviceCount > 16) deviceCount = 16;
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    std::cout << "Found " << deviceCount << " GPU(s):" << std::endl;
    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        std::cout << " - " << properties.deviceName << std::endl;
    }
}

void VulkanContext::createSurface(HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = hwnd;
    createInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan surface!");
    }
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    Array<VkPhysicalDevice, 16> devices{};
    if (deviceCount > 16) deviceCount = 16;
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (uint32_t i = 0; i < deviceCount; i++) {
        if (isDeviceSuitable(devices[i])) {
            physicalDevice = devices[i];
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) physicalDevice = devices[0];

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "Selected GPU: " << props.deviceName << std::endl;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    Array<VkQueueFamilyProperties, 32> queueFamilies{};
    if (queueFamilyCount > 32) queueFamilyCount = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) indices.presentFamily = i;
        if (indices.isComplete()) break;
    }
    return indices;
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    deletionQueue.push_back([this]() { vkDestroyDevice(device, nullptr); });
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        if (formatCount > 64) formatCount = 64;
        details.formatCount = formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        if (presentModeCount > 16) presentModeCount = 16;
        details.presentModeCount = presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const SwapChainSupportDetails& support) {
    for (int i = 0; i < support.formatCount; i++) {
        if (support.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return support.formats[i];
        }
    }
    return support.formats[0];
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const SwapChainSupportDetails& support) {
    if (!useVSync) {
        for (int i = 0; i < support.presentModeCount; i++) {
            if (support.presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) return support.presentModes[i];
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

void VulkanContext::rebuildPipelines() {
    vkDeviceWaitIdle(device); // Halt the GPU

    graphicsPipeline.reset();
    wireframePipeline.reset();

    std::string tescPath = "";
    std::string tesePath = "";

    VkCullModeFlags cullMode = enableCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;

    graphicsPipeline = std::make_unique<Pipeline>(
        device, renderPass, descriptors->getLayout(),
        "shaders/vert.spv", "shaders/frag.spv",
        tescPath, tesePath, VK_POLYGON_MODE_FILL, cullMode
    );

    wireframePipeline = std::make_unique<Pipeline>(
        device, renderPass, descriptors->getLayout(),
        "shaders/vert.spv", "shaders/frag.spv",
        tescPath, tesePath, VK_POLYGON_MODE_LINE, cullMode
    );
}

VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
    VkExtent2D actualExtent = { 800, 600 };
    actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
    return actualExtent;
}

void VulkanContext::initSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    if (imageCount > MAX_SWAPCHAIN_IMAGES) imageCount = MAX_SWAPCHAIN_IMAGES;
    activeImageCount = imageCount;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VulkanContext::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat VulkanContext::findDepthFormat() {
    VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
    return findSupportedFormat(candidates, 3, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

void VulkanContext::beginUI() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void VulkanContext::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthAttachmentRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    Array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    deletionQueue.push_back([this]() { vkDestroyRenderPass(device, renderPass, nullptr); });
}

void VulkanContext::createFramebuffers() {
    for (uint32_t i = 0; i < activeImageCount; i++) {
        Array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanContext::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) throw std::runtime_error("failed to create image!");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, findMemoryType(memReqs.memoryTypeBits, properties) };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) throw std::runtime_error("failed to allocate image memory!");

    vkBindImageMemory(device, image, imageMemory, 0);
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

VkFormat VulkanContext::findSupportedFormat(VkFormat* candidates, int candidateCount, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (int i = 0; i < candidateCount; i++) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidates[i], &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return candidates[i];
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return candidates[i];
    }
    throw std::runtime_error("failed to find supported format!");
}

VkImageView VulkanContext::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, VK_IMAGE_VIEW_TYPE_2D, format, {}, { aspectFlags, 0, 1, 0, 1 } };
    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) throw std::runtime_error("failed to create image view!");
    return imageView;
}

void VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr };
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) throw std::runtime_error("failed to create buffer!");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memRequirements.size, findMemoryType(memRequirements.memoryTypeBits, properties) };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) throw std::runtime_error("failed to allocate buffer memory!");

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanContext::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    deletionQueue.push_back([this]() { vkDestroyCommandPool(device, commandPool, nullptr); });
}

void VulkanContext::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = activeImageCount;

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanContext::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < activeImageCount; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }

        deletionQueue.push_back([this, i]() {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
            });
    }
}

void VulkanContext::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderObject>& sceneObjects) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearValues[2]{};
    clearValues[0].color = { {0.05f, 0.05f, 0.05f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = 2;
    renderPassInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if (useWireframe) {
        wireframePipeline->bind(commandBuffer);
    }
    else {
        graphicsPipeline->bind(commandBuffer);
    }
    VkViewport viewport{ 0.0f, 0.0f, (float)swapChainExtent.width, (float)swapChainExtent.height, 0.0f, 1.0f };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{ {0, 0}, swapChainExtent };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // 1. Bind global descriptor sets (Camera matrices, textures)
    VkDescriptorSet activeSet = descriptors->getSet(imageIndex);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getLayout(), 0, 1, &activeSet, 0, nullptr);

    // === PHASE 3: THE MULTI-OBJECT DRAW LOOP ===
    struct PushConstantData {
        Matrix4x4 modelMatrix;
        uint32_t textureIndex;
    };

    // 2. Loop over every object dynamically spawned by the ECS
    for (const auto& obj : sceneObjects) {

        // Failsafe: Don't draw if the mesh ID is invalid
        if (obj.meshID >= loadedMeshes.size()) continue;

        // 3. Grab the specific model from the VRAM warehouse
        const VulkanMesh& mesh = loadedMeshes[obj.meshID];

        // 4. Bind this specific model's geometry to the GPU
        VkBuffer vertexBuffers[] = { mesh.vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // 5. Loop over the sub-meshes (materials) of this specific model
        for (const auto& subMesh : mesh.subMeshes) {

            PushConstantData pushData{};
            pushData.modelMatrix = obj.transform;
            pushData.textureIndex = subMesh.textureIndex;

            vkCmdPushConstants(
                commandBuffer,
                graphicsPipeline->getLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PushConstantData),
                &pushData
            );

            // DRAW IT!
            vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1, subMesh.startIndex, 0, 0);
        }
    }
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (draw_data) {
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void VulkanContext::drawFrame(const Matrix4x4& viewMatrix, Camera* camera, const std::vector<RenderObject>& sceneObjects) {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // --- 2. BUILD YOUR SETTINGS MENU ---
    ImGui::Begin("Engine Settings");
    ImGui::Text("Performance");
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();

    if (camera) {
        ImGui::SliderFloat("Mouse Sensitivity", camera->getSensitivityPtr(), 0.01f, 0.5f);
    }
    ImGui::Checkbox("Wireframe Mode", &useWireframe);

    if (ImGui::Checkbox("V-Sync (Limit FPS)", &useVSync)) {
        vkDeviceWaitIdle(device);
        recreateSwapChain(); // Rebuilds the presentation engine
    }

    if (ImGui::Checkbox("Backface Culling", &enableCulling)) {
        rebuildPipelines(); // Recompiles the shaders instantly
    }

    // --- LIVE MESH SWAPPING UI ---

    if (!availableMeshes.empty()) {
        // 1. The Dropdown
        const char* previewValue = availableMeshes[currentMeshIndex].c_str();
        if (ImGui::BeginCombo("Select Mesh", previewValue)) {
            for (int i = 0; i < availableMeshes.size(); i++) {
                bool isSelected = (currentMeshIndex == i);
                if (ImGui::Selectable(availableMeshes[i].c_str(), isSelected)) {
                    if (currentMeshIndex != i) {
                        currentMeshIndex = i;

                        // VERY IMPORTANT: Halt the GPU before swapping out its memory!
                        vkDeviceWaitIdle(device);
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    ImGui::End();

    // --- 3. BAKE THE DRAW DATA ---
    ImGui::Render();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, sceneObjects);
    updateUniformBuffer(imageIndex, viewMatrix, currentMeshScale);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    }

    currentFrame = (currentFrame + 1) % activeImageCount;
}

void VulkanContext::scanAssetsFolder() {
    availableMeshes.clear();

    // UPDATED: Now uses relative path!
    std::string assetDir = "Assets/";

    if (std::filesystem::exists(assetDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(assetDir)) {
            if (entry.path().extension() == ".glb") {
                availableMeshes.push_back(entry.path().filename().string());
            }
        }
    }
    else {
        std::cout << "Assets directory not found! Expected path: " << std::filesystem::absolute(assetDir) << std::endl;
    }
}

void VulkanContext::cleanupVertexBuffer() {
    // Safely destroy the old buffer before creating a new one
    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }
}

uint32_t VulkanContext::uploadMesh(const std::string& filename) {
    std::string assetDir = "Assets/";
    std::string fullPath = assetDir + filename;

    // Load without applying a hardcoded scale! Scale is handled by the ECS now.
    ModelData model = loadModel(fullPath, 1.0f);

    if (model.vertices.empty() || model.indices.empty()) {
        std::cout << "Warning: Model " << filename << " is empty. Returning default." << std::endl;
        return 0; // Return the first loaded mesh as a failsafe
    }

    VulkanMesh newMesh{};
    newMesh.indexCount = static_cast<uint32_t>(model.indices.size());
    newMesh.subMeshes = model.subMeshes;

    // 1. Create and map Vertex Buffer
    VkDeviceSize vBufferSize = sizeof(model.vertices[0]) * model.vertices.size();
    createBuffer(vBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        newMesh.vertexBuffer, newMesh.vertexMemory);

    void* vData;
    vkMapMemory(device, newMesh.vertexMemory, 0, vBufferSize, 0, &vData);
    memcpy(vData, model.vertices.data(), (size_t)vBufferSize);
    vkUnmapMemory(device, newMesh.vertexMemory);

    // 2. Create and map Index Buffer
    VkDeviceSize iBufferSize = sizeof(model.indices[0]) * model.indices.size();
    createBuffer(iBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        newMesh.indexBuffer, newMesh.indexMemory);

    void* iData;
    vkMapMemory(device, newMesh.indexMemory, 0, iBufferSize, 0, &iData);
    memcpy(iData, model.indices.data(), (size_t)iBufferSize);
    vkUnmapMemory(device, newMesh.indexMemory);

    // --- THE MISSING LINK: TEXTURES & DESCRIPTORS ---
    bool rebuildDescriptors = false;

    // 1. Capture the global offset BEFORE adding new textures!
    uint32_t globalTextureOffset = static_cast<uint32_t>(textureImageViews.size());

    // 2. Upload the textures from this specific model
    for (auto& tex : model.textures) {
        createTextureImage(tex.pixels, tex.width, tex.height);
        rebuildDescriptors = true;
    }

    // 3. Failsafe: Make a dummy pixel if VRAM is completely empty
    if (textureImageViews.empty()) {
        createTextureImage(nullptr, 1, 1);
        rebuildDescriptors = true;
    }

    // 4. FIX TRAP 2: Shift the sub-mesh texture indices to match global VRAM!
    for (auto& subMesh : newMesh.subMeshes) {
        subMesh.textureIndex += globalTextureOffset;
    }

    // 5. Rebuild descriptors safely
    if (rebuildDescriptors || textureSampler == VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device); // MUST halt GPU before changing memory

        if (textureSampler == VK_NULL_HANDLE) {
            createTextureSampler();
        }

        // FIX TRAP 3: Enforce a strict ceiling so large models don't crash the engine!
        const size_t MAX_SHADER_TEXTURES = 1024; // This must match your shader.frag array limit
        std::vector<VkImageView> paddedViews = textureImageViews;

        // If the Porsche pushes us over 100, chop off the excess to save the GPU
        if (paddedViews.size() > MAX_SHADER_TEXTURES) {
            std::cout << "CRITICAL WARNING: Loaded more than " << MAX_SHADER_TEXTURES
                << " textures! Truncating array to prevent GPU crash." << std::endl;
            paddedViews.resize(MAX_SHADER_TEXTURES);
        }

        // If we are under 100, pad it up
        while (paddedViews.size() < MAX_SHADER_TEXTURES) {
            paddedViews.push_back(paddedViews[0]);
        }

        descriptors = std::make_unique<Descriptors>(
            device,
            activeImageCount,
            uniformBuffers.data(),
            sizeof(UniformBufferObject),
            paddedViews, // Safe, clamped array
            textureSampler
        );

        rebuildPipelines(); // Inform the graphics pipeline of the new layout
    }

    // Store it in the warehouse and return its ID
    uint32_t meshID = static_cast<uint32_t>(loadedMeshes.size());
    loadedMeshes.push_back(newMesh);

    std::cout << "Asset Manager safely warehoused: " << filename << " at ID " << meshID << std::endl;
    return meshID;
}

void VulkanContext::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    for (unsigned int i = 0; i < activeImageCount; i++) {
        createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i], uniformBuffersMemory[i]);

        // === FIXED: Added missing persistent cleanup hooks for active swapchain instances ===
        deletionQueue.push_back([this, i]() {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            });
    }
}

void VulkanContext::updateUniformBuffer(uint32_t currentImage, const Matrix4x4& viewMatrix, float modelScale) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();

    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};

    ubo.view = viewMatrix;

    float aspect = swapChainExtent.width / (float)swapChainExtent.height;
    ubo.projection = Matrix4x4::Perspective(0.7853f, aspect, 0.1f, 1000.0f);

    void* data;
    vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
}

void VulkanContext::createImageViews() {
    for (uint32_t i = 0; i < activeImageCount; i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanContext::cleanupSwapChain() {
    if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
    if (depthImage != VK_NULL_HANDLE) vkDestroyImage(device, depthImage, nullptr);
    if (depthImageMemory != VK_NULL_HANDLE) vkFreeMemory(device, depthImageMemory, nullptr);

    for (uint32_t i = 0; i < activeImageCount; i++) {
        if (swapChainFramebuffers[i] != VK_NULL_HANDLE) vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
        if (swapChainImageViews[i] != VK_NULL_HANDLE) vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }
    if (swapChain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void VulkanContext::createIndexBuffer(const std::vector<uint32_t>& indices) {
    currentIndexCount = static_cast<uint32_t>(indices.size());
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Create buffer with INDEX_BUFFER usage flag
    createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer,
        indexBufferMemory
    );

    void* data;
    vkMapMemory(device, indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, indexBufferMemory);
}

void VulkanContext::cleanupIndexBuffer() {
    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }
    currentIndexCount = 0;
}

void VulkanContext::recreateSwapChain() {
    RECT rect;
    GetClientRect(windowHandle, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    while (width == 0 || height == 0) {
        MSG msg;
        if (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        GetClientRect(windowHandle, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
        height = rect.bottom - rect.top;
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    initSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

void VulkanContext::createImGuiDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool!");
    }

    deletionQueue.push_back([this]() { vkDestroyDescriptorPool(device, imguiPool, nullptr); });
}

void VulkanContext::initImGui() {
    // 1. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 2. Setup Dear ImGui style (Dark mode is the standard!)
    ImGui::StyleColorsDark();

    // 3. Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(windowHandle);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physicalDevice;
    init_info.Device = device;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = graphicsQueue;

    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = activeImageCount;
    init_info.ImageCount = activeImageCount;
    init_info.UseDynamicRendering = false; // We are using a classic RenderPass
    init_info.PipelineInfoMain.RenderPass = renderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);

    // 4. Add ImGui cleanup to your RAII deletion queue
    deletionQueue.push_back([]() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        });
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Submit to the queue and force the CPU to wait until the GPU is done copying
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanContext::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void VulkanContext::createTextureImage(unsigned char* pixels, int texWidth, int texHeight) {
    bool usingGeneratedTexture = false;

    // Failsafe: If the pixels are null, generate the emergency magenta pixel
    if (!pixels) {
        std::cout << "CRITICAL: glTF texture missing! Generating emergency magenta pixel." << std::endl;
        texWidth = 1;
        texHeight = 1;
        pixels = new stbi_uc[4]{ 255, 0, 255, 255 };
        usingGeneratedTexture = true;
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Free the pixels from RAM since they are safely in the staging buffer
    if (usingGeneratedTexture) {
        delete[] pixels;
    }
    else {
        stbi_image_free(pixels);
    }

    VkImage newImage;
    VkDeviceMemory newMemory;

    createImage(
        texWidth, texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        newImage,
        newMemory
    );

    transitionImageLayout(newImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, newImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    transitionImageLayout(newImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    textureImages.push_back(newImage);
    textureImageMemories.push_back(newMemory);
    textureImageViews.push_back(createImageView(newImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT));

    std::cout << "Successfully moved glTF texture to VRAM." << std::endl;
}

void VulkanContext::cleanupTexture() {
    if (textureSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, textureSampler, nullptr);
        textureSampler = VK_NULL_HANDLE;
    }

    for (auto imageView : textureImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    textureImageViews.clear();

    for (auto image : textureImages) {
        vkDestroyImage(device, image, nullptr);
    }
    textureImages.clear();

    for (auto memory : textureImageMemories) {
        vkFreeMemory(device, memory, nullptr);
    }
    textureImageMemories.clear();
}

void VulkanContext::createTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}