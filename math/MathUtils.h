#pragma once

namespace Math {
    constexpr float PI = 3.14159265f;
    constexpr float TWO_PI = 6.28318530f;

    // Custom Table-Driven Taylor approximations
    float FastSin(float angle);
    float FastCos(float angle);
}