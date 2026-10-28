#version 450

// GLSL requires this extension to use variables to index into an array of samplers!
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

// Binding 1: We now have an ARRAY of 16 textures
layout(binding = 1) uniform sampler2D texSamplers[1024];

// UPDATED: Added mat4 model so the memory layout matches the Vertex shader exactly!
layout(push_constant) uniform PushConstants {
    mat4 model; 
    uint textureIndex;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the exact texture specified by the sub-mesh
    uint index = push.textureIndex;
    vec4 sampledTexture = texture(texSamplers[nonuniformEXT(index)], fragTexCoord);

    // MULTIPLY the image texture by the material color (fragColor) passed from C++!
    vec4 finalBaseColor = sampledTexture * fragColor;

    // Discard invisible pixels
    if(finalBaseColor.a < 0.1) {
        discard;
    }

    // Basic ambient + diffuse lighting using the combined color
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * finalBaseColor.rgb;

    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); 
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * finalBaseColor.rgb;

    vec3 result = ambient + diffuse;
    
    outColor = vec4(result, finalBaseColor.a);
}