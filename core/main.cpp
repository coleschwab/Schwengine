#include "Window.h"
#include "VulkanContext.h"
#include "AssetManager.h" // Includes the Librarian
#include "Camera.h"
#include "ecs/Registry.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include "imgui.h" // Needed for the Editor UI

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR pCmdLine, _In_ int nShowCmd) {

    SetProcessDPIAware();
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);

    std::cout << "Schwengine is spawning..." << std::endl;

    // 1. Start the Window
    Window myWindow(800, 600, "Schwengine");

    // 2. Start Vulkan (All-in-one Domino Chain)
    VulkanContext graphics;
    graphics.initVulkan(myWindow.getHandle());
    SetWindowLongPtr(myWindow.getHandle(), GWLP_USERDATA, (LONG_PTR)&graphics);
    graphics.listGPUs();

    // 3. Start the Asset Manager
    AssetManager assets(&graphics);

    // 4. SETUP ECS & CAMERA
    Registry registry;
    Camera myCamera;

    auto lastTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    float fpsAccumulator = 0.0f;

    // Editor UI State
    std::vector<std::string> availableFiles = assets.getAvailableAssets();
    int selectedFileIndex = 0;
    float spawnOffset = 0.0f; // Stagger spawns so they don't overlap perfectly
	float spawnScale = 1.0f;

    // 5. The Engine Loop
    while (myWindow.processMessages()) {

        auto currentTime = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        if (dt > 0.25f) dt = 0.25f; // Clamp to avoid spiral of death

        frameCount++;
        fpsAccumulator += dt;
        if (fpsAccumulator >= 1.0f) {
            std::cout << "FPS: " << frameCount << std::endl;
            frameCount = 0;
            fpsAccumulator -= 1.0f;
        }

        // Camera update
        myCamera.update(dt);

        // --- 6. START THE UI FRAME ---
        // (Make sure you moved ImGui_ImplVulkan_NewFrame() into this function!)
        graphics.beginUI();

        // --- 7. BUILD THE ASSET SPAWNER UI ---
        ImGui::Begin("Asset Spawner");
        if (!availableFiles.empty()) {
            if (ImGui::BeginCombo("Select Mesh", availableFiles[selectedFileIndex].c_str())) {
                for (int i = 0; i < availableFiles.size(); i++) {
                    bool isSelected = (selectedFileIndex == i);
                    if (ImGui::Selectable(availableFiles[i].c_str(), isSelected)) {
                        selectedFileIndex = i;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::SliderFloat("Spawn Scale", &spawnScale, 0.001f, 5.0f, "%.3f");

            if (ImGui::Button("Spawn Asset in Scene")) {
                std::string chosenFile = availableFiles[selectedFileIndex];
                uint32_t modelID = assets.getOrLoadMesh(chosenFile);

                Entity newEntity = registry.createEntity();
                registry.meshes[newEntity].meshID = modelID;

                registry.transforms[newEntity].scale = spawnScale;
                registry.transforms[newEntity].z = 10.0f;
                registry.transforms[newEntity].x = spawnOffset;

                spawnOffset += 3.0f;
            }
        }
        else {
            ImGui::Text("No .glb files found in Assets/ directory!");
        }
        ImGui::End();

        // --- 8. ECS TRANSFORM SYSTEM ---
        for (Entity i = 0; i < registry.transforms.size(); ++i) {   
            if (!registry.meshes[i].isVisible) continue;

            Matrix4x4 localMat = registry.transforms[i].getLocalMatrix();
            Entity parentID = registry.hierarchies[i].parent;

            if (parentID != MAX_ENTITIES) {
                // World = Parent's World * Local Offset
                registry.transforms[i].worldMatrix = registry.transforms[parentID].worldMatrix * localMat;
            }
            else {
                // No parent, so World = Local
                registry.transforms[i].worldMatrix = localMat;
            }
        }

        // --- 9. PREPARE RENDER DATA ---
        std::vector<RenderObject> sceneObjects;
        for (Entity i = 0; i < registry.transforms.size(); ++i) {
            if (registry.meshes[i].isVisible) {
                sceneObjects.push_back({
                    registry.transforms[i].worldMatrix,
                    registry.meshes[i].meshID // Pass the ID to Vulkan!
                    });
            }
        }

        // --- 10. SUBMIT TO VULKAN ---
        graphics.drawFrame(myCamera.getViewMatrix(), &myCamera, sceneObjects);
    }

    return 0;
}