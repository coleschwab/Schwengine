#version 450

// We are outputting a patch of 16 control points
layout (vertices = 16) out;

// Inputs from the Vertex Shader
layout (location = 0) in vec4 inColor[];
layout (location = 1) in vec2 inTexCoord[];

// Outputs to the Evaluation Shader
layout (location = 0) out vec4 outColor[];
layout (location = 1) out vec2 outTexCoord[];

void main() {
    // Pass the control points directly through
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outColor[gl_InvocationID] = inColor[gl_InvocationID];
    outTexCoord[gl_InvocationID] = inTexCoord[gl_InvocationID];

    // Only the first thread needs to set the subdivision levels
    if (gl_InvocationID == 0) {
        // Subdivide the quad into a 32x32 grid
        gl_TessLevelInner[0] = 32.0; 
        gl_TessLevelInner[1] = 32.0; 
        
        gl_TessLevelOuter[0] = 32.0;
        gl_TessLevelOuter[1] = 32.0;
        gl_TessLevelOuter[2] = 32.0;
        gl_TessLevelOuter[3] = 32.0;
    }
}