# Schwengine System Architecture Guide: `Pipeline.md`

This document provides an exhaustive, low-level architectural breakdown of the `Pipeline` module implementation (`Pipeline.cpp`). It details how the engine bakes programmable shader stages, fixed-function configurations, and layout interfaces into an immutable GPU hardware state object.

---

## 1. Class Lifecycle Methods

### `Pipeline::Pipeline(...)`
```cpp
Pipeline::Pipeline(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout, const std::string& vertFilePath, const std::string& fragFilePath)
    : device(device) {
    createGraphicsPipeline(renderPass, descriptorSetLayout, vertFilePath, fragFilePath);
}
```
* **Mechanics:** Captures the parent logical device reference handle to govern low-level resource tracking. It immediately initiates `createGraphicsPipeline`, passing required execution dependencies to construct the underlying graphics pipeline layout.
* **Architecture Impact:** Adheres strictly to the **Resource Acquisition Is Initialization (RAII)** pattern. An instance of this class cannot exist in an uninitialized or volatile state; constructing the class directly instantiates the hardware-baked GPU execution lanes.

### `Pipeline::~Pipeline()`
```cpp
Pipeline::~Pipeline() {
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
}
```
* **Mechanics:** Handles the autonomous teardown of pipeline assets when the object falls out of scope or is explicitly reset by the parent execution context.
  1. Validates the `graphicsPipeline` handle and fires `vkDestroyPipeline` to release the monolithic hardware execution state block.
  2. Validates the `pipelineLayout` handle and fires `vkDestroyPipelineLayout` to reclaim driver-side linkage paths.
* **Hardware Safety:** By managing its own disposal via an encapsulation block, it removes human error from order-of-destruction tracking. When `VulkanContext` triggers a pointer reset, this module strips itself clean while the base `VkDevice` layer remains online.

---

## 2. Command Binding Interface

### `void Pipeline::bind(VkCommandBuffer commandBuffer)`
```cpp
void Pipeline::bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
}
```
* **Mechanics:** Binds the current monolithic pipeline configuration to an active execution track via `vkCmdBindPipeline`.
* **Hardware Reality:** This operation updates the execution state registers on the graphics card. It dictates how subsequent `vkCmdDraw` invocations interpret input byte-streams, compute transformation geometry, execute rasterization, evaluate depth, and blend final fragments into memory targets.

---

## 3. Low-Level Shader Module Compilation

### `VkShaderModule Pipeline::createShaderModule(const std::string& filename)`
```cpp
std::ifstream file(filename, std::ios::ate | std::ios::binary);
size_t fileSize = static_cast<size_t>(file.tellg());
file.seekg(0);
```
* **File Operations:** Opens compiled SPIR-V shader files in a binary, random-access state (`std::ios::ate`). By reading the stream cursor position at the end of the file via `tellg()`, the engine accurately extracts byte configurations to deduce the memory buffer sizing requirements before rewinding the stream to index 0.

```cpp
size_t bufferSizeInBytes = (fileSize + 3) & ~3;
size_t uint32Count = bufferSizeInBytes / 4;
uint32_t* buffer = new uint32_t[uint32Count];
memset(buffer, 0, bufferSizeInBytes);
```
* **The Bitwise Alignment Trick:** The Vulkan API strictly mandates that SPIR-V binary code pointer addresses must align cleanly to **4-byte (`uint32_t`) memory boundaries**. Raw file read streaming can result in trailing, unaligned bytes. 
  * The bitwise logic `(fileSize + 3) & ~3` rounds the file allocation footprint upward to the nearest multiple of 4 bytes.
  * A raw `uint32_t` heap block is spawned using the aligned capacity count, and the space is pre-cleared via `memset` to avoid passing garbage data into the Vulkan driver layer.

```cpp
file.read(reinterpret_cast<char*>(buffer), fileSize);
...
VkShaderModuleCreateInfo createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
createInfo.codeSize = fileSize;
createInfo.pCode = buffer;
vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
```
* **Module Instantiation:** Streams raw SPIR-V byte structures out of the storage media file system directly into the aligned heap block. It configures a `VkShaderModuleCreateInfo` tracking manifest, binding the byte-length profile and mapping the data track pointer. It completes instantiation via `vkCreateShaderModule`, then clears the transient host heap buffer (`delete[] buffer`) to avoid memory leakage.

---

## 4. Monolithic GPU State Assembly

### `void Pipeline::createGraphicsPipeline(...)`
```cpp
VkShaderModule vertShaderModule = createShaderModule(vertFilePath);
VkShaderModule fragShaderModule = createShaderModule(fragFilePath);
```
* **Step 1: Transient Assets Loading:** Compiles transient shader modules out of the local build path directories, generating active entry handles for processing.

```cpp
auto bindingDescription = Vertex::getBindingDescription();
auto attributeDescriptions = Vertex::getAttributeDescriptions();

VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
vertexInputInfo.vertexBindingDescriptionCount = 1;
vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
vertexInputInfo.vertexAttributeDescriptionCount = 3;
vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
```
* **Step 2: Vertex Memory Geometry Mapping:** Queries your `Vertex` layout definition system to configure structural layout maps. It sets input strides, configures memory access frequencies, and maps three individual attribute layers (Positions, Colors, UV Coordinates) to match shader assembly locations.

```cpp
VkPipelineInputAssemblyStateCreateInfo inputAssembly{ ..., VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE };
VkPipelineViewportStateCreateInfo viewportState{ ..., 1, nullptr, 1, nullptr };
VkPipelineRasterizationStateCreateInfo rasterizer{ ..., VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, ... };
VkPipelineMultisampleStateCreateInfo multisampling{ ..., VK_SAMPLE_COUNT_1_BIT, ... };
VkPipelineDepthStencilStateCreateInfo depthStencil{ ..., VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, ... };
```
* **Step 3: Fixed-Function State Definitions:** Configures the hardware execution rules that dictate how geometry morphs into pixels:
  * `inputAssembly`: Instructs the GPU to group sets of three sequential vertices into independent surfaces (`VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`).
  * `viewportState`: Prepares allocations for exactly one viewport and one scissor canvas. It sets their data track pointers to null because these attributes will be injected dynamically inside the main render loop execution path.
  * `rasterizer`: Configures fill rendering modes (`VK_POLYGON_MODE_FILL`), drops inverted back-faces to optimize processing execution overhead (`VK_CULL_MODE_BACK_BIT`), and establishes winding rules to isolate front surfaces (`VK_FRONT_FACE_COUNTER_CLOCKWISE`).
  * `multisampling`: Disables multi-sampling antialiasing, locking allocations to a standard 1-bit profile for low-overhead rendering.
  * `depthStencil`: Activates hardware depth-testing safeguards (`VK_TRUE`), permitting the engine to compute overlay values via an incremental less-than evaluation scheme (`VK_COMPARE_OP_LESS`).

```cpp
VkPipelineColorBlendAttachmentState colorBlendAttachment{};
colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
colorBlendAttachment.blendEnable = VK_FALSE;
```
* **Step 4: Output Canvas Blending:** Dictates how computed fragment colors mix with existing backbuffer data blocks. By setting `blendEnable = VK_FALSE`, it configures the pipeline to overwrite old frame pixel values instantly, saving compute cycles when alpha translucency layers are not required.

```cpp
VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.setLayoutCount = 1;
pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
```
* **Step 5: Resource Interface Generation:** Packages the uniform block binding interfaces. It compiles a master pipeline interface layout tracking system (`VkPipelineLayout`), configuring it to observe structural formats defined within the global `descriptorSetLayout`.

```cpp
VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
VkPipelineDynamicStateCreateInfo dynamicState{ ..., 2, dynamicStates };
```
* **Step 6: Dynamic State Configurations:** Isolates specific fixed-function configuration pathways to allow real-time updates. By marking viewports and scissors as dynamic, the engine can change resolution scales on the fly inside `recordCommandBuffer` without needing to recreate the master pipeline.

```cpp
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.pStages = shaderStages;
pipelineInfo.pVertexInputState = &vertexInputInfo;
...
pipelineInfo.layout = pipelineLayout;
pipelineInfo.renderPass = renderPass;

vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
```
* **Step 7: Monolithic State Baking:** Packages every compiled sub-manifest component layout sheet into a central assembly structure (`VkGraphicsPipelineCreateInfo`). It targets the engine's global `renderPass`, links the uniform layouts, and fires `vkCreateGraphicsPipelines` to commit the entire configuration block to hardware silicon.

```cpp
vkDestroyShaderModule(device, fragShaderModule, nullptr);
vkDestroyShaderModule(device, vertShaderModule, nullptr);
```
* **Step 8: Cleanup of Intermediate Assets:** Immediately following successful pipeline generation, the engine drops the intermediate `vertShaderModule` and `fragShaderModule` tracking components. Because the code loops have been baked straight into the monolithic hardware pipeline state object, keeping these raw shader structures online in driver tracking space would be a waste of memory.

---

## 5. High-Performance Architecture Notes

* **The Monolithic Invariant:** In modern graphics systems, a `VkPipeline` is immutable once created. This design is highly intentional for engine core performance. It allows the graphics driver to pre-compile your state configurations, minimize driver validation checks during execution loops, and maximize graphics card throughput profile lines.
* **Modular Code Flexibility:** Isolating pipeline assembly steps into a standalone module unlocks seamless engine expansion. If you want to introduce a separate pipeline later for a different visual effect, you don't touch your engine core initialization loops; you simply declare an additional `Pipeline` unique pointer object initialized with alternate shader tracks.