#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class Pipeline {
public:
    Pipeline(
        VkDevice device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout descriptorSetLayout,
        const std::string& vertFilePath,
        const std::string& fragFilePath,
        const std::string& tescFilePath,
        const std::string& teseFilePath,
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE
    );

    ~Pipeline();

    void bind(VkCommandBuffer commandBuffer);

    // ADD THIS EXACT LINE:
    VkPipelineLayout getLayout() const { return pipelineLayout; }

private:
    VkDevice device;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkShaderModule createShaderModule(const std::string& filename);

    void createGraphicsPipeline(
        VkRenderPass renderPass,
        VkDescriptorSetLayout descriptorSetLayout,
        const std::string& vertFilePath,
        const std::string& fragFilePath,
        const std::string& tescFilePath,
        const std::string& teseFilePath,
        VkPolygonMode polygonMode,
        VkCullModeFlags cullMode
    );
};