#pragma once
#include <cstdint>

// An Entity is purely an index into our component arrays
using Entity = uint32_t;

// A constant representing a null/invalid entity
const Entity MAX_ENTITIES = 0xFFFFFFFF;