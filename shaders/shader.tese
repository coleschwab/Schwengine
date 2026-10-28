#version 450

// We are evaluating a grid (quads) with equal spacing
layout(quads, equal_spacing, ccw) in;

layout (location = 0) in vec4 inColor[];
layout (location = 1) in vec2 inTexCoord[];

layout (location = 0) out vec4 fragColor;
layout (location = 1) out vec2 fragTexCoord;

// Uniform buffer for camera matrices
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Basis function math
float B0(float t) { return (1.0 - t) * (1.0 - t) * (1.0 - t); }
float B1(float t) { return 3.0 * t * (1.0 - t) * (1.0 - t); }
float B2(float t) { return 3.0 * t * t * (1.0 - t); }
float B3(float t) { return t * t * t; }

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    float bu[4] = float[](B0(u), B1(u), B2(u), B3(u));
    float bv[4] = float[](B0(v), B1(v), B2(v), B3(v));

    vec4 finalPos = vec4(0.0);
    vec4 finalColor = vec4(0.0);
    vec2 finalTex = vec2(0.0);

    // Summation of the 16 control points
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            // The flat array index (i * 4 + j)
            int idx = i * 4 + j;
            float weight = bu[i] * bv[j];
            
            finalPos += gl_in[idx].gl_Position * weight;
            finalColor += inColor[idx] * weight;
            finalTex += inTexCoord[idx] * weight;
        }
    }

    fragColor = finalColor;
    fragTexCoord = finalTex;
    
    // Project the evaluated point to the screen
    gl_Position = ubo.proj * ubo.view * ubo.model * finalPos;
}