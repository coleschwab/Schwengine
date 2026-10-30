# Schwengine ⚙️

Note: Schwengine is currently in active architectural development. Asset loading and file paths are currently statically linked to facilitate rapid iteration on the core Vulkan rendering and memory pipelines.

A custom 3D graphics rendering engine built entirely from scratch in C++ and Vulkan. 

## Architecture & Features
To deepen my understanding of graphics programming and low-level memory management, this engine intentionally avoids standard external libraries (like GLM) in favor of native implementations.
*   **Custom Math Library:** Proprietary implementations for all core linear algebra, 3D spatial transformations, matrix logic, and camera raycasting calculations.
*   **Vulkan Graphics Pipeline:** Custom `VulkanContext` handling swapchains, command buffers, and raw memory allocation. *Includes custom GLSL Tessellation Control (TESC) and Tessellation Evaluation (TESE) shaders.*
*   **Entity Component System (ECS):** Custom data-oriented architecture for managing game objects, components, and hierarchical transforms efficiently in memory.

## Key Code References for Review
If you are reviewing this repository, the following files represent the core proprietary architecture:
*   `core/math/Matrix4x4.h` & `Vector4.h` - Native mathematical implementations (including branchless inverse matrix calculation).
*   `core/camera.cpp` - Custom camera raycasting, raw Win32 mouse hooking, and view matrix calculations.
*   `renderer/VulkanContext.cpp` - Core Vulkan initialization and state management.
*   `core/ecs/registry.h` & `core/systems/TransformSystem.h` - Underlying logic for the ECS and hierarchical matrix multiplication.
*   `shaders/shader.tesc` & `shaders/shader.tese` - Custom Vulkan hardware tessellation implementation.

*Note: Standard header-only libraries (stb_image, tiny_obj_loader, cgltf) are isolated in the `vendor/` directory and utilized strictly for standard asset and texture loading, leaving the math, rendering, and core architecture entirely native.*
