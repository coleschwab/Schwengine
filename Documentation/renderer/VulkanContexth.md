# Schwengine Header Specifications: `VulkanContext.h`

This document details the layout data layouts, type selections, structural abstractions, and functional definitions of the `VulkanContext` interface header file.

---

## 1. Structural Component Breakdowns

The header exposes three standalone data layouts crucial for device querying, lane mapping, and block transformations.

### A. `SwapChainSupportDetails`
A structured manifest used to poll physical hardware capability limits before instantiating target textures.
* `capabilities`: A native `VkSurfaceCapabilitiesKHR` footprint tracking bounding limitations (e.g., maximum image count, strict layer transformations, current resolution boundaries).
* `formats`: An allocation-free `Array<VkSurfaceFormatKHR, 64>` tracking color formats and matching color ranges.
* `presentModes`: An explicit `Array<VkPresentModeKHR, 16>` logging frame refresh methods.
* `formatCount` / `presentModeCount`: Primitive index trackers logging exact query array limits.

### B. `QueueFamilyIndices`
An extraction layer used to pinpoint hardware computing lanes.
* `graphicsFamily`: An optional type container tracking the hardware pipeline queue index capable of processing rasterization draw calls.
* `presentFamily`: An optional type container tracking the hardware pipeline queue index capable of processing display compositor image swaps.
* `isComplete()`: An atomic validation method ensuring both execution slots possess assigned values before allowing setup to advance.

### C. `UniformBufferObject (UBO)`
The host-accessible layout struct mapped directly onto GPU-visible memory to distribute transformations globally across vertex shader instances.
* `model`: A `Matrix4x4` updating object orientation relative to world center coordinates.
* `view`: A `Matrix4x4` calculating camera perspective modifications.
* `projection`: A `Matrix4x4` mapping 3D perspective dimensions into normalized device clip spaces.

---

## 2. Core Member Variable Declarations

The private fields block establishes a sandboxed state structure broken into functional execution divisions:

### A. Window & Core Hardware Handles
* `windowHandle`: Holds the native Win32 `HWND` descriptor linking the Vulkan surface context to the OS layer.
* `framebufferResized`: A simple boolean flag set by OS window loop procedures to force swapchain re-evaluations during active rendering steps.
* `instance`: The foundational global software link to the system Vulkan runtime layer.
* `surface`: The presentation canvas bridge linking raw drawing data to display screen regions.
* `physicalDevice`: Points to the chosen hardware chip layout (isolated via driver query enumerations).
* `device`: The active logical management reference utilized to issue device commands and maintain hardware tracking pools.
* `graphicsQueue` / `presentQueue`: Pointers mapping computing streams to target hardware lanes.

### B. Swapchain Sheets & Framebuffers
* `swapChain`: The dynamic active double/triple backbuffer image loop controller handle.
* `activeImageCount`: Tracks the total number of hardware frames initialized by the active swapchain setup.
* `swapChainImages`: A fixed `Array` housing individual texture buffer canvases assigned by the presentation driver.
* `swapChainImageFormat` / `swapChainExtent`: Cached configurations documenting color layouts and resolution boundaries.
* `swapChainImageViews` / `swapChainFramebuffers`: Fixed buffer sheets allocating render layer views and targeting drawing connections per frame track.

### C. Subsystem Integrations (RAII Smart Pointers)
* `descriptors`: A `std::unique_ptr` handling descriptor sets, allocations, pools, and shader object layout definitions.
* `graphicsPipeline`: A `std::unique_ptr` caching immutable state sheets (e.g., blending mechanics, shader stages, culling rules).

### D. Hardware Memory Allocations & Optimization Tracks
* `vertexBuffer` / `vertexBufferMemory`: Allocates and pins low-overhead memory fields targeting physical geometry vertices.
* `uniformBuffers` / `uniformBuffersMemory`: Array chains holding independent matrix memory blocks mapped once per active swapchain image track.
* `uniformBuffersMapped`: An explicit `Array<void*, MAX_SWAPCHAIN_IMAGES>` keeping long-lived virtual address pointers permanently mapped to host system space. This layout entirely removes costly `vkMapMemory` and `vkUnmapMemory` context boundary switches from the runtime engine draw loop.

### E. Command Allocators & Synchronization Timelines
* `commandPool`: The parent memory block allocating individual command lanes.
* `commandBuffers`: Unique command buffers recording rendering tracks concurrently across multi-buffered targets.
* `imageAvailableSemaphores` / `renderFinishedSemaphores`: GPU-to-GPU timeline synchronization primitives regulating access loops between presentation controllers and computing units.
* `inFlightFences`: CPU-to-GPU timeline synchronization barriers regulating host application execution speeds to ensure the CPU never outruns the hardware processing speed limits.
* `currentFrame`: A simple loop track counter indexing multi-buffered ring allocation slots.

---

## 3. Method Specifications & API Map

### A. Public Lifecycle Methods
* `initVulkan(HWND)`: Invokes core initialization routines to bring up hardware capabilities, allocate system memory blocks, and mount dynamic unique subsystems.
* `drawFrame()`: The central runtime integration engine synchronizing fences, querying available frame canvases, triggering uniform matrix writes, dispatching execution queues, and pushing completed imagery onto the active desktop display window.

### B. Private Core Sub-Allocators
* `createBuffer(...)`: The primary memory mapper creating virtual `VkBuffer` objects, identifying required memory layouts, querying memory locations, and binding physical chunks onto abstract resource descriptors.
* `createImage(...)`: Allocates 2D texture surfaces, matches hardware parameters, claims memory regions, and completes low-level object binding.
* `findMemoryType(...)`: Filters available physical GPU hardware memory sheets to identify a matching pool based on type configurations and target attributes (e.g., host-visible cache pathways).
* `recordCommandBuffer(...)`: Binds active graphics layout profiles, viewports, vertex buffers, and frame descriptor sheets onto the current command stream before committing the instruction list to active processing pools.
* `recreateSwapChain()`: Flushes active hardware queues, wipes volatile structural tracks via `cleanupSwapChain()`, and completely rebuilds presentation backbuffers to reflect updated window dimensions cleanly.