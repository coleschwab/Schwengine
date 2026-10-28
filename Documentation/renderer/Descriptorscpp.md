# Schwengine System Architecture Guide: `Descriptors.md`

This document provides an exhaustive, low-level architectural breakdown of the `Descriptors` module implementation (`Descriptors.cpp`). It details how the engine translates high-level C++ uniform structures into explicit hardware-visible pointer layouts and resource binding schemas within the Vulkan driver environment.

---

## 1. Class Lifecycle Methods

### `Descriptors::Descriptors(...)`
```cpp
Descriptors::Descriptors(VkDevice device, uint32_t activeImageCount, const VkBuffer* uniformBuffers, size_t uboSize)
    : device(device) {
    createDescriptorSetLayout();
    createDescriptorPool(activeImageCount);
    createDescriptorSets(activeImageCount, uniformBuffers, uboSize);
}
```
* **Mechanics:** The constructor establishes resource ownership by capturing the logical hardware device handle. It then executes an atomic, sequential pipeline that defines the memory binding interface, allocates a driver-side allocation pool, and instantiates the multi-buffered data links matching your active swapchain image paths.
* **State Mutation:** Transitions the `Descriptors` instance from an uninitialized state container into an active, hardware-tracked interface engine holding populated layouts and pools.

### `Descriptors::~Descriptors()`
```cpp
Descriptors::~Descriptors() {
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }
}
```
* **Mechanics:** Reclaims memory structures during object destruction.
  1. Checks if `descriptorPool` is allocated; if valid, it calls `vkDestroyDescriptorPool`. **Crucial Hardware Behavior:** In Vulkan, destroying a parent descriptor pool automatically voids, frees, and reclaims all child `VkDescriptorSet` memory tracks generated from that pool. This eliminates the need to execute costly individual `vkFreeDescriptorSets` commands.
  2. Invokes `vkDestroyDescriptorSetLayout` to drop the interface template definitions from the driver memory table.
* **Invariants:** Handles are validated against `VK_NULL_HANDLE` to guarantee safe teardown patterns even if an initialization crash drops the system mid-flight.

---

## 2. Interface Layout Definitions

### `void Descriptors::createDescriptorSetLayout()`
```cpp
VkDescriptorSetLayoutBinding uboLayoutBinding{};
uboLayoutBinding.binding = 0;
uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboLayoutBinding.descriptorCount = 1;
...
vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
```
* **Mechanics:** Defines the structural memory contract that your C++ code must fulfill to communicate with graphics shaders.
  1. Populates a `VkDescriptorSetLayoutBinding` structure. It sets `binding = 0` to map this layout track straight to location slot `layout(binding = 0)` inside your GLSL vertex shader code.
  2. Sets the type attribute to `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` and assigns a count of `1`, defining this slot as a singular global transformation block rather than an array of structures.
  3. Sets `stageFlags` to `VK_SHADER_STAGE_VERTEX_BIT` to restrict data visibility exclusively to the vertex processing hardware engine, preventing unauthorized shader stages from wasting memory bandwidth sampling this block.
  4. Commits the interface template configuration to the driver using `vkCreateDescriptorSetLayout`.
* **Hardware Reality:** This layout layout functions as a compile-time interface template. The graphics driver uses it to generate proper byte-offset memory fetch pipelines for the GPU execution units before drawing occurs.

---

## 3. Storage Allocation Management

### `void Descriptors::createDescriptorPool(uint32_t activeImageCount)`
```cpp
VkDescriptorPoolSize poolSize{};
poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
poolSize.descriptorCount = activeImageCount;
...
vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
```
* **Mechanics:** Spawns a specialized hardware allocation memory pool context managed directly by the graphics driver. 
  1. Populates a `VkDescriptorPoolSize` manifest to specify the exact resource types this pool is authorized to allocate. It sets the type to `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER` and matches the total capacity count to your exact runtime `activeImageCount`.
  2. Populates the master `VkDescriptorPoolCreateInfo` configuration sheet, setting `maxSets` equal to `activeImageCount`. This ensures the pool requests exactly enough internal allocation slots to handle your multi-buffered frame indexing demands without wasting VRAM space.
  3. Instantiates the tracking pool via `vkCreateDescriptorPool`.

---

## 4. Descriptor Set Allocation & Mapping

### `void Descriptors::createDescriptorSets(...)`
```cpp
std::vector<VkDescriptorSetLayout> layouts(activeImageCount, descriptorSetLayout);
VkDescriptorSetAllocateInfo allocInfo{};
...
descriptorSets.resize(activeImageCount);
vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());
```
* **Mechanics:** Allocates the concrete interface descriptors and wires them to your physical global transform uniform memory ranges.
  1. Generates a temporary allocation vector (`layouts`) populated with repeating references to your baseline `descriptorSetLayout`. This 1:1 array mapping is a mandatory requirement of the `vkAllocateDescriptorSets` API to define the internal blueprint layout structure for each individual set slot being generated.
  2. Prepares the configuration blueprint via `VkDescriptorSetAllocateInfo`, referencing your active driver allocation `descriptorPool`, setting allocation tracking array bounds, and linking the layout blueprint list pointer.
  3. Allocates the descriptor arrays inside your private class container using `vkAllocateDescriptorSets`.

```cpp
for (uint32_t i = 0; i < activeImageCount; i++) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffers[i];
    bufferInfo.offset = 0;
    bufferInfo.range = uboSize;

    VkWriteDescriptorSet descriptorWrite{};
    ...
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}
```
* **The Update Loop:** Loops through every newly allocated multi-buffer track slot to perform a CPU-side copy operation that maps descriptor references directly to actual video hardware buffer data addresses:
  1. Instantiates a tracking structure (`VkDescriptorBufferInfo`), binding the frame track's corresponding raw `uniformBuffers[i]` handle allocation, setting a baseline read point offset (`0`), and setting the maximum data boundary clamp to your passed `uboSize` metric.
  2. Populates a master driver mutation manifest (`VkWriteDescriptorSet`). This layout configuration references your target destination container (`dstSet = descriptorSets[i]`), locks target destination binding paths (`dstBinding = 0`), and confirms data payload classification flags (`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER`).
  3. Executes `vkUpdateDescriptorSets`. This pushes your structural data sheet details to the logical device engine, telling the graphics driver exactly which memory byte lanes to query when drawing commands process this descriptor slot index.

---

## 5. High-Performance Architecture Notes

* **Implicit Set Disposal Efficiency:** Because `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` is omitted from pool creation configurations, individual sets cannot be manually freed on a whim. This design choice is highly intentional for engine core performance. It tells the graphics driver that this pool is immutable and stable, allowing the driver to allocate memory in a highly optimized, linear chunk without needing to maintain complex, fragmented allocation tracking tables.
* **Decoupled Architecture Scalability:** By abstracting descriptor lifecycles away from the central `VulkanContext` hub, you have completely modularized data layouts. If you want to integrate texture samplers or normal mapping arrays down the line, you do not have to pollute your engine core functions. You simply update `createDescriptorSetLayout` and `createDescriptorPool` inside this class file to declare additional binding tracks without breaking your master loop architecture.