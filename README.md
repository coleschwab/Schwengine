# Schwengine ⚙️

Note: Schwengine is currently in active architectural development. Asset loading and file paths are currently statically linked to facilitate rapid iteration on the core Vulkan rendering and memory pipelines.

A custom 3D graphics rendering engine built entirely from scratch in C++ and Vulkan. 

## Architecture & Features
To deepen my understanding of graphics programming and low-level memory management, this engine intentionally avoids standard external libraries (like GLM) in favor of native implementations.
*   **Custom Math Library:** Proprietary implementations for all core linear algebra, 3D spatial transformations, matrix logic, and camera raycasting calculations.
*   **Vulkan Graphics Pipeline:** Custom `VulkanContext` handling swapchains, command buffers, and raw memory allocation.
