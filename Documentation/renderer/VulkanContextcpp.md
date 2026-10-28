# Schwengine System Architecture Guide: `VulkanContext.md`

This document provides an exhaustive, low-level architectural breakdown of the `VulkanContext` implementation. It details the underlying Vulkan API call mechanics, memory mutations, synchronization paths, and hardware resource lifecycles for every function in the core engine hub.

---

## 1. Class Lifecycle Methods

### `VulkanContext::VulkanContext()`
```cpp
VulkanContext::VulkanContext()
    : instance(VK_NULL_HANDLE), ...
```
* **Mechanics:** Initializes all primitive, non-RAII Vulkan handles explicitly to `VK_NULL_HANDLE` (or `VK_FORMAT_UNDEFINED` / zero values for scalar fields). This prevents undefined behavior or garbage pointer evaluations during early system validation checks or application initialization failures.
* **State Mutation:** Mutates the state layout of the `VulkanContext` instance inside host memory, setting active structural handles to safe zero-states.

### `VulkanContext::~VulkanContext()`
```cpp
VulkanContext::~VulkanContext() {
    if (device != VK_NULL_HANDLE) { vkDeviceWaitIdle(device); }
    graphicsPipeline.reset();
    descriptors.reset();
    cleanupSwapChain();
    for (auto it = deletionQueue.rbegin(); it != deletionQueue.rend(); ++it) { (*it)(); }
    deletionQueue.clear();
}
```
* **Mechanics:** Handles the orderly shutdown of the graphics subsystem.
  1. Blocks the host CPU thread via `vkDeviceWaitIdle` until all submitted GPU commands are completely executed, ensuring no assets are destroyed mid-execution.
  2. Invokes `.reset()` on the `graphicsPipeline` and `descriptors` unique pointers. This explicitly calls their respective destructors to release their dependent Vulkan structures while the logical hardware device is still fully online.
  3. Executes `cleanupSwapChain()` to tear down volatile window-dependent layout attachments.
  4. Iterates through the functional lambda registry (`deletionQueue`) in strict **Last-In, First-Out (LIFO)** order to destroy base components (logical devices, surfaces, instances).
* **Memory Invariants:** After the queue flush completes, `deletionQueue.clear()` drops the wrapper heap memory footprints, leaving the core context container in a pristine state before it goes out of scope.

---

## 2. Core API & Device Initialization Subsystem

### `void VulkanContext::initVulkan(HWND hwnd)`
* **Mechanics:** The central entry point orchestrating the initialization sequence for the Vulkan engine. It populates driver configuration manifests, initializes driver-level extensions, and coordinates helper functions to bring components online in a safe dependency hierarchy.
* **Execution Flow Matrix:**

| Step | Invoked Function / Subsystem Block | Hardware Lifecycle & Queue State Modification |
| :--- | :--- | :--- |
| **1** | `vkCreateInstance` | Initializes the Vulkan runtime library loader and links native application metadata with active display layers. Registers the clean-up hook onto the internal queue. |
| **2** | `createSurface(hwnd)` | Hooks into the native Win32 window manager subsystem. Binds the OS handle to the instance interface layer. Sets up the primary destruction hook. |
| **3** | `pickPhysicalDevice()` | Queries the driver enumerator to find and isolate discrete GPU execution chips. |
| **4** | `createLogicalDevice()` | Brings execution interfaces online by creating hardware command queues. Saves command lanes onto the internal context structures. |
| **5** | `initSwapChain()` ... `createFramebuffers()` | Allocates frame backing structures, format sheets, color attachments, depth testing targets, and layout canvases. |
| **6** | `createVertexBuffer()` / `createUniformBuffers()` | Allocates and pins video memory segments for geometry buffers and host-visible global execution transformations. |
| **7** | `std::make_unique<Descriptors>` | Allocates memory descriptor pools, set layouts, and links binding addresses to uniform buffer footprints. |
| **8** | `std::make_unique<Pipeline>` | Compiles and caches the immutable GPU state engine using your shader binary files. |
| **9** | `createCommandPool()` ... `createSyncObjects()` | Instantiates the thread allocation pools for recording command streams and constructs frame processing sync objects. |

### `void VulkanContext::createSurface(HWND hwnd)`
```cpp
VkWin32SurfaceCreateInfoKHR createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
createInfo.hwnd = hwnd;
createInfo.hinstance = GetModuleHandle(nullptr);
vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface);
```
* **Mechanics:** Interfaces directly with the Windows OS platform layer via the `VK_KHR_win32_surface` extension. It maps the raw Win32 Window handle (`HWND`) and module handle (`HINSTANCE`) to an abstract `VkSurfaceKHR` representation within the engine workspace.
* **Lifecycle:** On successful surface creation, a cleanup lambda is immediately pushed onto the `deletionQueue` to guarantee that `vkDestroySurfaceKHR` drops the platform connection before the primary instance context is torn down.

### `void VulkanContext::listGPUs()`
* **Mechanics:** Queries the driver configuration database to find all connected physical graphics accelerators using `vkEnumeratePhysicalDevices`. It stores these hardware targets inside a zero-allocation `Array<VkPhysicalDevice, 16>` container to avoid heap allocations. It iterates through the found targets, fetches their internal specifications via `vkGetPhysicalDeviceProperties`, and routes their structural identifiers (`deviceName`) to the diagnostic output log.

### `bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device)`
* **Mechanics:** Evaluates a hardware device target to verify it matches engine requirements.
* **Return Value Constraints:** Inspects the core attributes data sheet. It returns `true` only if the hardware reported architecture target matches `VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`, ensuring processing workloads are routed to a dedicated graphics card rather than an integrated processor.

### `void VulkanContext::pickPhysicalDevice()`
* **Mechanics:** Walks the array of evaluated hardware units populated by `vkEnumeratePhysicalDevices`. It routes each discovered device handle through `isDeviceSuitable` to identify candidate chips.
* **Fallback Invariant:** If no discrete hardware layouts are detected on the host motherboard, it logs a warning and falls back to index `0` of the discovered devices array. It queries the chosen hardware chip's identifier text via `vkGetPhysicalDeviceProperties` and updates the internal `physicalDevice` pointer state.

### `QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device)`
* **Mechanics:** Queries the low-level processing lanes supported by the targeted GPU chip using `vkGetPhysicalDeviceQueueFamilyProperties`. It stores these inside a local `Array<VkQueueFamilyProperties, 32>` and loops through them to locate two critical execution queues:
  1. Checks for the `VK_QUEUE_GRAPHICS_BIT` flag to assign the core graphics pipeline queue index.
  2. Calls `vkGetPhysicalDeviceSurfaceSupportKHR`, passing the active window surface context, to locate an execution lane capable of processing screen composition and flip presentation routines.
* **Optimization Invariant:** Once both index locations are successfully resolved, the loop terminates early via an internal completeness check (`indices.isComplete()`).

### `void VulkanContext::createLogicalDevice()`
```cpp
vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
```
* **Mechanics:** Establishes the primary software communication link to your graphics card. It configures a queue activation structure (`VkDeviceQueueCreateInfo`) using the graphics queue index discovered by `findQueueFamilies`, requesting a single processing lane with maximum priority (`1.0f`). It populates extension tracking lists with `VK_KHR_SWAPCHAIN_EXTENSION_NAME` to activate display output layers, then instantiates the driver interface via `vkCreateDevice`.
* **State Mutation:** Populates the `device` handle member variable and queries the driver to assign pointers for `graphicsQueue` and `presentQueue`. It then appends a `vkDestroyDevice` destruction loop to the core lifecycle tracker.

---

## 3. Swapchain Infrastructure Subsystem

### `void VulkanContext::initSwapChain()`
```cpp
vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
```
* **Mechanics:** Configures the image presentation buffers that interface with the OS display manager.
  1. Queries capability structures to configure optimal dimensions (`chooseSwapExtent`), color layouts (`chooseSwapSurfaceFormat`), and display synchronization profiles (`chooseSwapPresentMode`).
  2. Determines the frame image count—targeting your minimum capability plus one additional buffer to minimize presentation stalling.
  3. Populates a `VkSwapchainCreateInfoKHR` manifest. If the graphics and presentation queue indices diverge, it configures sharing modes to `VK_SHARING_MODE_CONCURRENT` to manage command processing automatically. If the queue index layout matches, it forces `VK_SHARING_MODE_EXCLUSIVE` to achieve maximum driver optimization profiles.
  4. Spawns the driver presentation pipeline via `vkCreateSwapchainKHR`.
* **State Mutation:** Updates the internal tracking state of `swapChainExtent` and `swapChainImageFormat`. It populates `activeImageCount` and stores the generated frame textures inside the `swapChainImages` array container using `vkGetSwapchainImagesKHR`.

### `SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device)`
* **Mechanics:** Queries the hardware driver to check compatibility between the physical GPU chip and the active window surface via `vkGetPhysicalDeviceSurfaceCapabilitiesKHR`. It fills an internal `Array` struct with color arrangement formats via `vkGetPhysicalDeviceSurfaceFormatsKHR` and collects available synchronization models via `vkGetPhysicalDeviceSurfacePresentModesKHR`.

### `VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(const SwapChainSupportDetails& support)`
* **Mechanics:** Filters the list of color arrangements provided by the hardware driver to find an optimal format.
* **Selection Criteria:** Loops through the available configuration sets, checking for a combination of an uncompressed SRGB layout (`VK_FORMAT_B8G8R8A8_SRGB`) and non-linear color space processing (`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`). If the targeted pair is absent, it safely falls back to the driver's default index (`0`) format layout.

### `VkPresentModeKHR VulkanContext::chooseSwapPresentMode(const SwapChainSupportDetails& support)`
* **Mechanics:** Searches the driver configuration tables to determine the presentation synchronization model.
* **Selection Criteria:** Prioritizes triple-buffered vertical synchronization via `VK_PRESENT_MODE_MAILBOX_KHR` to allow low-latency rendering without screen tearing. If the graphics driver does not support mailbox presentation, it falls back to standard double-buffered vertical synchronization (`VK_PRESENT_MODE_FIFO_KHR`), which is a mandatory requirement guaranteed across all Vulkan compliance specifications.

### `VkExtent2D VulkanContext::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)`
* **Mechanics:** Computes the pixel resolution dimensions for your rendering surface targets.
* **Selection Criteria:** If the window manager locks dimensions to the current extent, it returns that specific resolution layout immediately. Otherwise, it extracts window metrics from the host OS layer, using `std::max` and `std::min` to safely clamp values between the hardware driver's `minImageExtent` and `maxImageExtent` boundary parameters.

### `void VulkanContext::cleanupSwapChain()`
```cpp
if (depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, depthImageView, nullptr);
```
* **Mechanics:** Frees all temporary, window-dependent memory allocations within the engine workspace. It destroys the depth canvas image view, its underlying memory allocations, and loops through the active frames to destroy all attached `VkFramebuffer` structures and `VkImageView` components. Finally, it drops the base swapchain abstraction handle via `vkDestroySwapchainKHR`.
* **Invariant:** All handles are explicitly checked against `VK_NULL_HANDLE` before dropping, allowing it to execute safely even during partial initialization failures.

### `void VulkanContext::recreateSwapChain()`
```cpp
void VulkanContext::recreateSwapChain() {
    ... while (width == 0 || height == 0) { GetClientRect(windowHandle, &rect); ... }
    vkDeviceWaitIdle(device);
    cleanupSwapChain();
    initSwapChain(); ...
}
```
* **Mechanics:** Manages window resizing and display resolution mutations dynamically.
  1. Queries window layout metrics via Win32 `GetClientRect`. If the window is minimized (width and height drop to zero), it enters a blocking thread state to process passive OS window loop messages until the dimensions expand again.
  2. Calls `vkDeviceWaitIdle` to flush the active hardware command queues safely.
  3. Invokes `cleanupSwapChain()` to release out-of-date image assets.
  4. Re-executes the initialization sequence (`initSwapChain`, `createImageViews`, `createDepthResources`, `createFramebuffers`) to regenerate all canvas frames at the new resolution metrics.

---

## 4. Hardware Memory Allocators & Core Modifiers

### `void VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)`
```cpp
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
vkBindBufferMemory(device, buffer, bufferMemory, 0);
```
* **Mechanics:** Allocates low-level memory segments on your graphics card for data buffers.
  1. Configures a buffer usage manifest (`VkBufferCreateInfo`), specifying allocation footprints and usage targets (e.g., Vertex data, Uniform blocks).
  2. Spawns the abstract buffer infrastructure object via `vkCreateBuffer`.
  3. Queries the hardware allocation requirements using `vkGetBufferMemoryRequirements` to determine memory alignment rules and compatible memory type bitmasks.
  4. Routes alignment parameters through `findMemoryType` to select an appropriate memory pool matching your configuration flags (e.g., host-visible mapping memory).
  5. Pinpoints and allocates a physical video memory block via `vkAllocateMemory`.
  6. Binds the allocated physical memory range to the abstract buffer handle via `vkBindBufferMemory`.

### `void VulkanContext::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)`
* **Mechanics:** Allocates low-level pixel array grids on your graphics card for texturing or canvas layers. It sets configuration dimensions within a `VkImageCreateInfo` structure, using 2D allocations (`VK_IMAGE_TYPE_2D`), single mip levels, and an exclusive sharing mode. It initializes the layout handle via `vkCreateImage`. Like the buffer allocator above, it queries memory requirements via `vkGetImageMemoryRequirements`, translates memory flags via `findMemoryType`, allocates hardware video memory using `vkAllocateMemory`, and binds the allocation to the image handle via `vkBindImageMemory`.

### `uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)`
* **Mechanics:** Matches buffer or image requirements against the physical memory pools available on your GPU. It fetches the system memory configuration architecture using `vkGetPhysicalDeviceMemoryProperties`. It then loops through all available memory types, checking two conditions:
  1. Verifies if the type matching index fits inside the hardware required filter bitmask (`typeFilter & (1 << i)`).
  2. Verifies if the memory pool contains all required capability flags (`propertyFlags & properties == properties`), such as host-visible memory synchronization.
* **Exception Invariant:** If no available memory blocks match the requested configuration flags, it throws a `std::runtime_error`.

### `VkFormat VulkanContext::findSupportedFormat(VkFormat* candidates, int candidateCount, VkImageTiling tiling, VkFormatFeatureFlags features)`
* **Mechanics:** Scans candidate formats to locate a pixel format supported by the active GPU chip. It loops through the candidates array, querying their hardware capabilities via `vkGetPhysicalDeviceFormatProperties`. Depending on the image tiling type, it inspects either `linearTilingFeatures` or `optimalTilingFeatures` to confirm all required capability flags are supported.

### `VkImageView VulkanContext::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)`
```cpp
VkImageViewCreateInfo viewInfo{ ..., image, VK_IMAGE_VIEW_TYPE_2D, format, ... };
vkCreateImageView(device, &viewInfo, nullptr, &imageView);
```
* **Mechanics:** Instantiates an allocation layout sheet allowing your graphics shaders to interpret image data structures. It populates a `VkImageViewCreateInfo` structure, binding the target image, configuring a 2D access model (`VK_IMAGE_VIEW_TYPE_2D`), mapping standard color component swizzling arrays, and defining spatial target bounds (such as color or depth test masks). It generates the tracking target via `vkCreateImageView`.

### `VkFormat VulkanContext::findDepthFormat()`
* **Mechanics:** Identifies a supported pixel format for depth testing. It creates an inline array containing preferred formats: `VK_FORMAT_D32_SFLOAT`, `VK_FORMAT_D32_SFLOAT_S8_UINT`, and `VK_FORMAT_D24_UNORM_S8_UINT`. It passes these candidates to `findSupportedFormat`, requesting optimal image tiling configurations matched with depth stencil attachments (`VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT`).

---

## 5. Pipeline Resource Generators

### `void VulkanContext::createDepthResources()`
* **Mechanics:** Generates the depth canvas used to handle 3D coordinate overlapping calculations. It queries a valid format using `findDepthFormat`, then passes resolution constraints down to `createImage`. This allocates a device-local memory target configured for depth stencil attachments (`VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT`). It then wraps this allocation in a dedicated descriptor view layer via `createImageView`, passing depth aspect tracking masks (`VK_IMAGE_ASPECT_DEPTH_BIT`).

### `void VulkanContext::createFramebuffers()`
```cpp
Array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };
framebufferInfo.pAttachments = attachments.data();
vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
```
* **Mechanics:** Binds your render targets to the graphics pipeline. It loops through the available swapchain image count, creating an attachment pair array containing the active frame's color image view and the shared global depth image view. It populates a `VkFramebufferCreateInfo` structure with resolution parameters and links it to the engine's global `VkRenderPass`. It then calls `vkCreateFramebuffer` to instantiate the frame drawing canvases.

### `void VulkanContext::createVertexBuffer()`
```cpp
auto cubeVertices = CleanGeometry::getCubeVertices();
VkDeviceSize bufferSize = sizeof(cubeVertices[0]) * cubeVertices.size();
createBuffer(bufferSize, ..., vertexBuffer, vertexBufferMemory);
void* data;
vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
memcpy(data, cubeVertices.data(), (size_t)bufferSize);
vkUnmapMemory(device, vertexBufferMemory);
```
* **Mechanics:** Allocates and populates the hardware buffer containing your 3D geometry coordinates.
  1. Requests raw coordinate lists from the `CleanGeometry` namespace and computes the total allocation size footprint.
  2. Invokes `createBuffer` to allocate a staging area configured for host-visible, coherent memory data paths (`VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`).
  3. Maps the allocated virtual video memory into the host CPU's address space using `vkMapMemory`.
  4. Executes a standard `memcpy` routine to copy the raw geometry attributes directly into the mapped address range.
  5. Unmaps the memory range via `vkUnmapMemory` to flush data paths to the GPU, then registers a cleanup hook onto the deletion queue.

### `void VulkanContext::createUniformBuffers()`
* **Mechanics:** Instantiates the data channels used to pass global transformation parameters (model, view, and projection matrices) to your shaders. It loops through the available swapchain image slots, calling `createBuffer` to instantiate independent data buffers sized exactly to your `UniformBufferObject` layout struct. Each allocated buffer is configured for host-visible, coherent data transfers. A cleanup lambda is immediately registered for each buffer instance to handle resource disposal during engine shutdown.

---

## 6. GPU Execution Timeline & Command Control Loops

### `void VulkanContext::createCommandPool()`
```cpp
poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
```
* **Mechanics:** Instantiates the memory pool context manager used to allocate command buffers within the graphics subsystem. It configures a `VkCommandPoolCreateInfo` manifest, setting flags to `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT` to allow the engine to recycle and re-record individual command buffers every frame. It links the allocator pool to the primary graphics queue family index and instantiates the pool handle via `vkCreateCommandPool`.

### `void VulkanContext::createCommandBuffers()`
```cpp
VkCommandBufferAllocateInfo allocInfo{};
allocInfo.commandPool = commandPool;
allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
allocInfo.commandBufferCount = activeImageCount;
vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
```
* **Mechanics:** Allocates command execution tracks from your command pool. It populates a `VkCommandBufferAllocateInfo` configuration manifest, linking it to the newly created `commandPool`. It sets the level parameter to `VK_COMMAND_BUFFER_LEVEL_PRIMARY`, which allows these buffers to be submitted directly to the hardware execution queues for processing. It allocates an array of command buffers matching your active swapchain image count via `vkAllocateCommandBuffers`.

### `void VulkanContext::createSyncObjects()`
```cpp
fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
```
* **Mechanics:** Instantiates the synchronization objects used to regulate data processing speeds between the CPU host thread and the GPU processing pipeline. It loops through your active frame allocation tracks, initializing two distinct synchronization primitives:
  1. Spawns binary execution semaphores (`imageAvailableSemaphores` and `renderFinishedSemaphores`) to manage task ordering on the GPU timeline.
  2. Spawns thread fences (`inFlightFences`) to manage synchronization between the host CPU and the GPU. Fences are initialized with the `VK_FENCE_CREATE_SIGNALED_BIT` flag pre-set, ensuring the engine loop does not lock up on its very first frame pass.

### `void VulkanContext::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)`
```cpp
vkBeginCommandBuffer(commandBuffer, &beginInfo);
vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
graphicsPipeline->bind(commandBuffer);
vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->getLayout(), ...);
vkCmdDraw(commandBuffer, 36, 1, 0, 0);
vkCmdEndRenderPass(commandBuffer);
vkEndCommandBuffer(commandBuffer);
```
* **Mechanics:** Records hardware instructions into an active command buffer for a specific frame target.
  1. Opens the recording session via `vkBeginCommandBuffer`.
  2. Starts a render pass context via `vkCmdBeginRenderPass`, binding the frame's target framebuffer canvas, setting target draw boundaries, and configuring standard background clear colors.
  3. Binds the immutable graphics pipeline state using your RAII module (`graphicsPipeline->bind`).
  4. Sets runtime drawing configurations dynamically via `vkCmdSetViewport` and `vkCmdSetScissor` commands.
  5. Binds the vertex buffer allocation to input slot `0` via `vkCmdBindVertexBuffers`.
  6. Binds your uniform transformation descriptors using `vkCmdBindDescriptorSets`, pulling the active frame's descriptor set from the module layout sheets.
  7. Appends a drawing instruction (`vkCmdDraw`) to rasterize the 36 cube vertex elements.
  8. Closes the render pass session via `vkCmdEndRenderPass` and finalizes recording via `vkEndCommandBuffer`.

### `void VulkanContext::drawFrame()`
* **Mechanics:** The core runtime execution pump, executing a tightly synchronized cycle across the CPU and GPU processing timelines:

```text
[CPU: Wait for Frame Fence] ➔ [Acquire Swapchain Image] ➔ [Reset Frame Fence]
                                                                  │
[Present Image on Screen] ◄── [Submit to GPU Queue] ◄── [Record Command Buffer]
```

* **Detailed Execution Flow Step-by-Step Breakdown:**
  1. **CPU Execution Throttle:** Calls `vkWaitForFences` to halt the host CPU thread until the GPU finishes processing the targeted frame track.
  2. **Image Acquisition:** Invokes `vkAcquireNextImageKHR` to request a free backbuffer frame from the swapchain, linking it to the active `imageAvailableSemaphore`. If the window resized mid-flight, it catches `VK_ERROR_OUT_OF_DATE_KHR` and triggers swapchain regeneration.
  3. **Fence Reset:** Manually resets the frame fence via `vkResetFences` to lock the host timeline before command generation begins.
  4. **Command Buffer Setup:** Resets the target frame's command buffer memory and invokes `recordCommandBuffer` to append the fresh drawing instructions.
  5. **UBO Transformation Map:** Invokes `updateUniformBuffer` to write the updated rotation matrices into host-mapped uniform memory ranges.
  6. **Hardware Queue Submission:** Prepares submission parameters inside a `VkSubmitInfo` manifest. It links the active command buffer, attaches the `imageAvailableSemaphore` to block color output operations until the buffer is ready, and sets the `renderFinishedSemaphore` to signal when rendering completes. It submits the instruction stream to your graphics card via `vkQueueSubmit`, passing the active frame fence handle to handle CPU synchronization tracking.
  7. **Display Presentation:** Populates a presentation manifest (`VkPresentInfoKHR`), linking the active frame index and the `renderFinishedSemaphore`. It queues the image for screen presentation via `vkQueuePresentKHR`. If the presentation engine flags out-of-date assets, it triggers a swapchain reconstruction before updating your frame tracking indices (`currentFrame`).

### `void VulkanContext::updateUniformBuffer(uint32_t currentImage)`
```cpp
UniformBufferObject ubo{};
ubo.model = Matrix4x4::RotationY(time * 1.2f);
...
void* data;
vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
memcpy(data, &ubo, sizeof(ubo));
vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
```
* **Mechanics:** Computes real-time 3D coordinate transformations and updates the active uniform buffer. It samples your hardware system clock via `high_resolution_clock`, calculating elapsed time metrics to compute dynamic rotation matrices. It constructs a fresh `UniformBufferObject` manifest, applying matrix translation operations to place the model within your 3D view space. It maps the frame's uniform buffer video memory footprint using `vkMapMemory`, executes a quick `memcpy` routine to update the data, and unmaps the range via `vkUnmapMemory` to make the updated transformations available to your vertex shaders.