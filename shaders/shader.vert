#version 450

// 1. Inputs from your C++ Vertex struct
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor; // <-- ADDED: We must read the paint color from C++!

// 2. Outputs to Fragment Shader (MUST MATCH .FRAG EXACTLY)
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// We leave the push constant here to align memory, but we don't need to 
// forward the index because the fragment shader can read it directly!
layout(push_constant) uniform PushConstants {
    mat4 model;
    uint textureIndex;
    uint emissiveIndex;
    uint aoIndex;
    uint normalIndex;
} push;

struct InstanceData {
    mat4 model;
    uint textureIndex;
    float pad1, pad2, pad3;
};

layout(std140, set = 0, binding = 3) readonly buffer InstanceBuffer {
    InstanceData instances[];
} instanceData;

void main() {
    mat4 currentModel = instanceData.instances[gl_InstanceIndex].model;
    
    vec4 worldPos = currentModel * vec4(inPosition.xyz, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    // 3. Pass the exactly matched data to the fragment shader
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    
    mat3 normalMatrix = transpose(inverse(mat3(currentModel)));
    fragNormal = normalize(normalMatrix * inNormal.xyz);
    
    fragPos = worldPos.xyz;
}