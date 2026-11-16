#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Descriptors {
private:
    VkDevice device;
    uint32_t activeImageCount;
    VkBuffer* uniformBuffers;
	VkBuffer* instanceBuffers;
    VkDeviceSize instanceBufferSize;
    VkDeviceSize uboSize;

    // --- NEW VARIABLES ---
    VkBuffer* lightBuffers;
    VkDeviceSize lightBufferSize;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets(const std::vector<VkImageView>& imageViews, VkSampler sampler);

public:
    Descriptors(VkDevice device, uint32_t activeImageCount,
        VkBuffer* uniformBuffers, VkDeviceSize uboSize,
        VkBuffer* lightBuffers, VkDeviceSize lightBufferSize,
        VkBuffer* instanceBuffers, VkDeviceSize instanceBufferSize,
        const std::vector<VkImageView>& imageViews, VkSampler sampler);

    ~Descriptors();

    void updateTextures(const std::vector<VkImageView>& imageViews, VkSampler sampler);

    VkDescriptorSetLayout getLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getSet(uint32_t index) const { return descriptorSets[index]; }
};