#pragma once
#include <vulkan/vulkan.h>
#include <vector> // <-- REQUIRED FOR STD::VECTOR

class Descriptors {
public:
    // UPDATED: Constructor now takes a vector of image views
    Descriptors(VkDevice device, uint32_t activeImageCount, VkBuffer* uniformBuffers, VkDeviceSize uboSize, const std::vector<VkImageView>& imageViews, VkSampler sampler);
    ~Descriptors();

    VkDescriptorSetLayout getLayout() const { return descriptorSetLayout; }
    VkDescriptorSet getSet(uint32_t index) const { return descriptorSets[index]; }

    // UPDATED: Plural update function takes a vector of image views
    void updateTextures(const std::vector<VkImageView>& imageViews, VkSampler sampler);

private:
    VkDevice device;
    uint32_t activeImageCount;
    VkBuffer* uniformBuffers;
    VkDeviceSize uboSize;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    void createDescriptorSetLayout();
    void createDescriptorPool();
    // UPDATED: Internal set creation takes a vector of image views
    void createDescriptorSets(const std::vector<VkImageView>& imageViews, VkSampler sampler);
};