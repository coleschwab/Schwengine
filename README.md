# Schwengine ⚙️

A custom 3D graphics rendering engine built entirely from scratch in C++ and Vulkan. 

## Architecture & Features
To deepen my understanding of graphics programming and low-level memory management, this engine intentionally avoids standard external libraries (like GLM) in favor of native implementations.
*   **Custom Math Library:** Proprietary implementations for all core linear algebra, 3D spatial transformations, matrix logic, and camera raycasting calculations.
*   **Vulkan Graphics Pipeline:** Custom `VulkanContext` handling swapchains, command buffers, and raw memory allocation.
*   **Asset Reverse-Engineering Pipeline:** End-to-end workflow for extracting, parsing, and rendering complex 3D meshes directly from compiled Xbox 360 ISOs using specialized command-line tools and FatXplorer.

## Getting Started / Build Instructions
*(Write out the exact steps someone needs to compile this on their own machine. Because you are using CMake, it should look something like this:)*

1. Clone the repository
2. Ensure the Vulkan SDK and CMake are installed.
3. Generate the build files: `cmake -B build`
4. Compile the engine: `cmake --build build`
