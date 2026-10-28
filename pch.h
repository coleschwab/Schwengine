#pragma once

// Precompiled header for commonly used system and project headers to improve build times
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

// Frequently-included system headers (keep stable, heavy headers here)
#ifdef _WIN32
#include <Windows.h>
#endif
#include <vulkan/vulkan.h>
