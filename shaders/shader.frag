#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// 1. Inputs from Vertex Shader
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos; // <-- CRITICAL for Bump Maps!

// 2. Global Bindless Texture Array
layout(binding = 1) uniform sampler2D texSamplers[1024];

// 3. 80-byte Push Constant Block
layout(push_constant) uniform PushConstants {
    mat4 model;
    uint textureIndex;
    uint emissiveIndex;
    uint aoIndex;
    uint normalIndex;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    // Safely clamp all indices
    uint texIdx = push.textureIndex >= 1024 ? 0 : push.textureIndex;
    uint emissIdx = push.emissiveIndex >= 1024 ? 0 : push.emissiveIndex;
    uint aoIdx = push.aoIndex >= 1024 ? 0 : push.aoIndex;
    uint normIdx = push.normalIndex >= 1024 ? 0 : push.normalIndex;

    // Base Color & Vertex Paint Fallback
    vec4 baseColor = texture(texSamplers[nonuniformEXT(texIdx)], fragTexCoord);
    if (texIdx == 0) baseColor *= fragColor; 
    if (baseColor.a < 0.1) discard;

    // Ambient Occlusion
    float ao = 1.0;
    if (aoIdx != 0) {
        ao = texture(texSamplers[nonuniformEXT(aoIdx)], fragTexCoord).r;
    }

    // Normal Mapping (Screen-Space Derivatives)
    vec3 N = normalize(fragNormal);
    if (normIdx != 0) {
        vec3 mapNormal = texture(texSamplers[nonuniformEXT(normIdx)], fragTexCoord).rgb * 2.0 - 1.0;
        vec3 q1  = dFdx(fragPos);
        vec3 q2  = dFdy(fragPos);
        vec2 st1 = dFdx(fragTexCoord);
        vec2 st2 = dFdy(fragTexCoord);
        vec3 T  = normalize(q1*st2.t - q2*st1.t);
        vec3 B  = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * mapNormal);
    }

    // Emissive Glow
    vec3 emission = vec3(0.0);
    if (emissIdx != 0) {
        emission = texture(texSamplers[nonuniformEXT(emissIdx)], fragTexCoord).rgb * 2.5;
    }

    // Final Lighting Math
    float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * baseColor.rgb * ao; 
    
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3)); 
    float diff = max(dot(N, lightDir), 0.0);
    vec3 diffuse = diff * 0.9 * baseColor.rgb; 

    vec3 result = ambient + diffuse + emission;
    outColor = vec4(result, baseColor.a);
}