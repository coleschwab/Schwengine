#pragma once
#include "Registry.h"
#include "../../math/Matrix4x4.h"

// ------------------------------------------------------------------
// Transform System
// CURRENT: Uses recursive graph traversal via intrusive linked list.
// TODO (Optimization): Implement Topological Sorting on the entity arrays 
// to guarantee parents are stored before children. This will allow us to 
// replace the recursive traversal with a single, flat, cache-coherent for-loop.
// ------------------------------------------------------------------
class TransformSystem {
private:
    // Recursive function to push parent matrices down the tree
    static void updateNode(Registry& registry, Entity current, const Matrix4x4& parentWorldMatrix) {
        // 1. Calculate this entity's world matrix
        TransformComponent& transform = registry.transforms[current];

        // Matrix multiplication order: Parent * Local 
        transform.worldMatrix = parentWorldMatrix * transform.getLocalMatrix();

        // 2. Pass this new world matrix down to all siblings/children
        HierarchyComponent& hierarchy = registry.hierarchies[current];
        Entity child = hierarchy.firstChild;

        while (child != MAX_ENTITIES) {
            updateNode(registry, child, transform.worldMatrix);
            child = registry.hierarchies[child].nextSibling;
        }
    }

public:
    static void update(Registry& registry, uint32_t activeEntityCount) {
        // Loop through all active entities to find the Roots
        for (Entity i = 0; i < activeEntityCount; ++i) {
            if (registry.hierarchies[i].parent == MAX_ENTITIES) {

                // If it has no parent, its world matrix is just its local matrix
                TransformComponent& transform = registry.transforms[i];
                transform.worldMatrix = transform.getLocalMatrix();

                // Start the recursive chain for its children
                Entity child = registry.hierarchies[i].firstChild;
                while (child != MAX_ENTITIES) {
                    updateNode(registry, child, transform.worldMatrix);
                    child = registry.hierarchies[child].nextSibling;
                }
            }
        }
    }
};