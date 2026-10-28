#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// ADD THIS: The Push Constant block coming from C++
layout(push_constant) uniform PushConstants {
    mat4 model;
    uint textureIndex;
} push;

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inNormal;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main() {
    // UPDATED: Swap ubo.model for push.model
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition.xyz, 1.0);
    
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    
    // UPDATED: Convert the normal into world space using push.model
    fragNormal = mat3(push.model) * inNormal.xyz;
    
    // UPDATED: Pass the position of the pixel in world space using push.model
    fragPos = vec3(push.model * vec4(inPosition.xyz, 1.0));
}