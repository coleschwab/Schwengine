#pragma once
#include <vector>
#include <queue>
#include "Types.h"
#include "Components.h"
// ------------------------------------------------------------------
// ECS Registry
// Utilizes a strict Structure of Arrays (SoA) layout for cache-friendly 
// iteration. Entities are purely indices into parallel contiguous arrays.
// ------------------------------------------------------------------
class Registry {
private:
    uint32_t nextEntityId = 0;
    std::queue<Entity> freeIds;

public:
    // The cache-friendly flat arrays
    std::vector<TransformComponent> transforms;
    std::vector<HierarchyComponent> hierarchies;
    std::vector<MeshComponent> meshes;
    // Creates a new entity and aligns parallel arrays.
    // Architecture Note: Currently uses strict index-matching (1:1 component to entity ratio). 
    // Future optimization: Implement sparse sets to reduce memory footprint for entities 
    // that do not require all component types.
    Entity createEntity() {
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

    // Attaches a child entity to a parent entity in the scene graph.
    void attachEntity(Entity child, Entity parent) {
        HierarchyComponent& childHierarchy = hierarchies[child];
        HierarchyComponent& parentHierarchy = hierarchies[parent];

        // 1. Set the child's parent
        childHierarchy.parent = parent;

        // 2. Insert into the parent's linked list
        if (parentHierarchy.firstChild == MAX_ENTITIES) {
            // It's the first child
            parentHierarchy.firstChild = child;
        }
        else {
            // Find the last sibling and append
            Entity currentSibling = parentHierarchy.firstChild;
            while (hierarchies[currentSibling].nextSibling != MAX_ENTITIES) {
                currentSibling = hierarchies[currentSibling].nextSibling;
            }
            hierarchies[currentSibling].nextSibling = child;
            childHierarchy.prevSibling = currentSibling;
        }
    }
};