#pragma once
#include <vector>
#include <queue>
#include "Types.h"
#include "Components.h"

class Registry {
private:
    uint32_t nextEntityId = 0;
    std::queue<Entity> freeIds;

public:
    // The cache-friendly flat arrays
    std::vector<TransformComponent> transforms;
    std::vector<HierarchyComponent> hierarchies;
    std::vector<MeshComponent> meshes;

    Entity createEntity() {
        Entity id;
        if (!freeIds.empty()) {
            id = freeIds.front();
            freeIds.pop();
            // Reset recycled components
            transforms[id] = TransformComponent();
            hierarchies[id] = HierarchyComponent();
            meshes[id] = MeshComponent();
        }
        else {
            id = nextEntityId++;
            transforms.push_back(TransformComponent());
            hierarchies.push_back(HierarchyComponent());
            meshes.push_back(MeshComponent());
        }
        return id;
    }

    void destroyEntity(Entity id) {
        freeIds.push(id);
        meshes[id].isVisible = false; // Hide it immediately
    }
};