#pragma once

#include <vulkan/vulkan.h>
#include "Vector4.h"
#include "Vector2.h" 
#include "Array.h"

struct Vertex {
    Vector4 pos;
    Vector4 color;
    Vector2 texCoord;
    Vector4 normal;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static Array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        Array<VkVertexInputAttributeDescription, 4> attributes{};

        // Location 0: Position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, pos);

        // Location 1: Color
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, color);

        // Location 2: Texture Coordinates
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, texCoord);

        // Location 3: Normals
        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[3].offset = offsetof(Vertex, normal);

        return attributes;
    }
};