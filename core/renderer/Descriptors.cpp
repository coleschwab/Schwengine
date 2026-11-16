#include "Descriptors.h"
#include <stdexcept>
#include <array>
#include <iostream>

// Added instanceBuffers and instanceBufferSize to the constructor
Descriptors::Descriptors(VkDevice device, uint32_t activeImageCount,
    VkBuffer* uniformBuffers, VkDeviceSize uboSize,
    VkBuffer* lightBuffers, VkDeviceSize lightBufferSize,
    VkBuffer* instanceBuffers, VkDeviceSize instanceBufferSize,
    const std::vector<VkImageView>& imageViews, VkSampler sampler)
    : device(device), activeImageCount(activeImageCount),
    uniformBuffers(uniformBuffers), uboSize(uboSize),
    lightBuffers(lightBuffers), lightBufferSize(lightBufferSize),
    instanceBuffers(instanceBuffers), instanceBufferSize(instanceBufferSize) {

    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSets(imageViews, sampler);
}

Descriptors::~Descriptors() {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
}

void Descriptors::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1024;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding lightLayoutBinding{};
    lightLayoutBinding.binding = 2;
    lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lightLayoutBinding.descriptorCount = 1;
    lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // Binding 3: Instance SSBO
    VkDescriptorSetLayoutBinding instanceLayoutBinding{};
    instanceLayoutBinding.binding = 3;
    instanceLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instanceLayoutBinding.descriptorCount = 1;
    instanceLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Group all 4 bindings into a single array BEFORE creating the layout
    std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
        uboLayoutBinding, samplerLayoutBinding, lightLayoutBinding, instanceLayoutBinding
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void Descriptors::createDescriptorPool() {
    // Increased array size to 3 for the 3 different types of descriptors
    std::array<VkDescriptorPoolSize, 3> poolSizes{};

    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = activeImageCount * 2; // MVP + Lights

    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = activeImageCount * 1024; // Textures

    // Add capacity for the Storage Buffer
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = activeImageCount;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = activeImageCount;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Descriptors::createDescriptorSets(const std::vector<VkImageView>& imageViews, VkSampler sampler) {
    std::vector<VkDescriptorSetLayout> layouts(activeImageCount, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = activeImageCount;
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(activeImageCount);
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    updateTextures(imageViews, sampler);
}

void Descriptors::updateTextures(const std::vector<VkImageView>& imageViews, VkSampler sampler) {
    if (imageViews.empty() || sampler == VK_NULL_HANDLE) {
        std::cout << "Skipping descriptor update: Texture arrays are empty." << std::endl;
        return;
    }

    for (size_t i = 0; i < activeImageCount; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = uboSize;

        VkDescriptorBufferInfo lightInfo{};
        lightInfo.buffer = lightBuffers[i];
        lightInfo.offset = 0;
        lightInfo.range = lightBufferSize;

        // Instance Buffer Info
        VkDescriptorBufferInfo instanceInfo{};
        instanceInfo.buffer = instanceBuffers[i];
        instanceInfo.offset = 0;
        instanceInfo.range = instanceBufferSize;

        std::vector<VkDescriptorImageInfo> imageInfos(1024);
        for (size_t j = 0; j < 1024; j++) {
            imageInfos[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[j].imageView = (j < imageViews.size()) ? imageViews[j] : imageViews[0];
            imageInfos[j].sampler = sampler;
        }

        // Increased writes array to 4
        std::array<VkWriteDescriptorSet, 4> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1024;
        descriptorWrites[1].pImageInfo = imageInfos.data();

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &lightInfo;

        // Binding 3: Instance Buffer
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = descriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &instanceInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}