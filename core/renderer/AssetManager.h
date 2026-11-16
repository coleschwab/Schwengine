#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>
#include <filesystem>

// We must include this so the inline function knows about uploadMesh()
#include "VulkanContext.h"

class AssetManager {
private:
    VulkanContext* graphics = nullptr;
    std::unordered_map<std::string, uint32_t> meshCache;

public:
    AssetManager(VulkanContext* context) : graphics(context) {}

    uint32_t getOrLoadMesh(const std::string& filepath) {
        auto it = meshCache.find(filepath);
        if (it != meshCache.end()) {
            return it->second; // Already in VRAM, return cached ID
        }

        // Request VulkanContext to allocate and upload to VRAM
        uint32_t newID = graphics->uploadMesh(filepath);
        meshCache[filepath] = newID;
        return newID;
    }

    std::vector<std::string> getAvailableAssets() {
        std::vector<std::string> assets;
        std::string assetDir = "Assets/";

        if (std::filesystem::exists(assetDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(assetDir)) {
                if (entry.path().extension() == ".glb") {
                    assets.push_back(entry.path().filename().string());
                }
            }
        }
        return assets;
    }
};