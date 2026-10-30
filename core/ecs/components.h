#pragma once
#include "Types.h"
#include "../../math/Matrix4x4.h"

// 1. Spatial Data (Local space only)
struct TransformComponent {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float yaw = 0.0f;
    float scale = 1.0f;

    // The absolute final matrix the renderer uses
    Matrix4x4 worldMatrix = Matrix4x4::Identity();

    Matrix4x4 getLocalMatrix() const {
        return Matrix4x4::Translation(x, y, z) * Matrix4x4::RotationY(yaw) * Matrix4x4::Scale(scale);
    }
};

// 2. Hierarchy Data (The Scene Graph)
// Implemented as an intrusive linked list to guarantee flat memory 
// and completely avoid per-entity dynamic heap allocations.
struct HierarchyComponent {
    Entity parent = MAX_ENTITIES;
    Entity firstChild = MAX_ENTITIES;
    Entity nextSibling = MAX_ENTITIES;
    Entity prevSibling = MAX_ENTITIES;
};

// 3. Visual Data
struct MeshComponent {
    uint32_t meshID = 0;
    bool isVisible = true;
};