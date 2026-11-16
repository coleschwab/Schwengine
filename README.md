# Schwengine ⚙️

> *Note: Schwengine is currently in active architectural development. Asset loading and file paths are statically linked to facilitate rapid iteration on core Vulkan rendering and memory pipelines.*

A custom 3D graphics rendering engine built entirely from scratch in C++ and Vulkan.

---

## Architecture & Features
To deepen understanding of graphics programming and low-level memory management, this engine intentionally avoids standard heavy math libraries (like GLM) in favor of native implementations.

* **Custom Math Library:** Proprietary implementations for all core linear algebra, 3D spatial transformations, matrix logic, and camera raycasting calculations (`Matrix4x4`, `Vector4`).
* **Vulkan 1.3 Rendering Pipeline:** Custom `VulkanContext` handling swapchains, command buffers, raw memory allocation, and dynamic state pipelines (including Vulkan 1.3 dynamic culling).
* **Bindless Material Architecture:** Global VRAM texture array (`sampler2D texSamplers[1024]`) utilizing `nonuniformEXT` and SSBO-driven instance streaming (`InstanceData` aligned to 80 bytes).
* **Advanced Material Support:** Real-time parsing and routing for Base Color, Emissive Maps (candle glow), Ambient Occlusion (AO) maps, and Normal Maps (using screen-space derivatives for physical 3D bumps).
* **Hardware Tessellation:** Custom GLSL Tessellation Control (TESC) and Tessellation Evaluation (TESE) shaders.
* **Entity Component System (ECS):** Custom data-oriented architecture for managing game objects, components, and hierarchical transforms efficiently in memory.

---

## How to Use

1. **Launch the Engine:** Compile and run the executable (`x64-Release` recommended for full compiler optimizations and maximum texture loading speed).
2. **Spawn Assets:** 
   * Open the **Asset Spawner** panel in the ImGui UI.
   * Select your target asset (e.g., Sponza) and click spawn.
   * Monitor the black console/terminal window to track real-time loading progress and VRAM allocation.
3. **Navigation:** Once the scene loads into memory, use standard **WASD** keys combined with mouse look to navigate freely through the 3D environment.
4. **Engine Settings UI:** 
   * Use the settings window to toggle **Backface Culling** (intelligently managed via glTF `double_sided` flags), **V-Sync**, and **Wireframe Mode**.
   * *Known limitation:* The engine settings mesh selector is currently nonfunctional and is scheduled for architectural removal in a future update.

---

## Engine Architecture Roadmap

The roadmap below outlines the next phases of development to push Schwengine toward AAA-tier performance and rendering fidelity:

### Phase 1: Performance & Multi-Threading Optimization
* **Worker Thread Command Buffer Recording:** Move `recordCommandBuffer` loops to a parallel job system to saturate CPU cores.
* **CPU-Side Frustum Culling:** Implement bounding-box vs. camera frustum checks to skip rendering off-screen objects.
* **GPU Profiling:** Integrate RenderDoc capture passes to isolate driver stalls and draw-call overhead.

### Phase 2: Dynamic Lighting & Shadows
* **Cascaded Shadow Maps (CSM):** Build an offscreen depth-only render pass from the sun's perspective with Percentage-Closer Filtering (PCF) for smooth shadow edges.
* **Clustered Forward Lighting:** Transition from a single sun vector to a tiled screen-space light grid supporting hundreds of dynamic point lights.

### Phase 3: Modern PBR (Physically Based Rendering)
* **Metallic & Roughness Maps:** Expand the material SSBO pipeline to ingest metallic and roughness texture channels.
* **Cook-Torrance BRDF:** Implement microfacet specular reflection math for physically accurate surface responses.
* **Image-Based Lighting (IBL):** Add environment cubemaps for realistic ambient reflections.

### Phase 4: Scene Architecture & Systems
* **Spatial Partitioning (BVH):** Implement Bounding Volume Hierarchies for raycasting, collision detection, and efficient queries.
* **Async Asset Streaming:** Move file I/O and texture decoding to background worker threads to eliminate stutter.

---

## Key Code References for Review
* `core/math/Matrix4x4.h` & `Vector4.h` — Native mathematical implementations (including branchless inverse matrix calculations).
* `core/camera.cpp` — Custom camera raycasting, raw Win32 mouse hooking, and view matrix calculations.
* `renderer/VulkanContext.cpp` — Core Vulkan initialization, bindless descriptor array setup, and state management.
* `core/ecs/registry.h` & `core/systems/TransformSystem.h` — Underlying logic for the ECS and hierarchical matrix multiplication.
* `shaders/shader.vert` & `shaders/shader.frag` — Bindless SSBO vertex ingestion and screen-space derivative normal/AO mapping.
