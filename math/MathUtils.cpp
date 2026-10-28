#include "MathUtils.h"

namespace Math {
    constexpr int TABLE_SIZE = 32;
    constexpr float STEP = TWO_PI / (float)TABLE_SIZE;

    // 32 pre-calculated exact values for a full circle
    static const float SIN_TABLE[TABLE_SIZE] = {
         0.000000f,  0.195090f,  0.382683f,  0.555570f,
         0.707106f,  0.831469f,  0.923879f,  0.980785f,
         1.000000f,  0.980785f,  0.923879f,  0.831469f,
         0.707106f,  0.555570f,  0.382683f,  0.195090f,
         0.000000f, -0.195090f, -0.382683f, -0.555570f,
        -0.707106f, -0.831469f, -0.923879f, -0.980785f,
        -1.000000f, -0.980785f, -0.923879f, -0.831469f,
        -0.707106f, -0.555570f, -0.382683f, -0.195090f
    };

    static const float COS_TABLE[TABLE_SIZE] = {
         1.000000f,  0.980785f,  0.923879f,  0.831469f,
         0.707106f,  0.555570f,  0.382683f,  0.195090f,
         0.000000f, -0.195090f, -0.382683f, -0.555570f,
        -0.707106f, -0.831469f, -0.923879f, -0.980785f,
        -1.000000f, -0.980785f, -0.923879f, -0.831469f,
        -0.707106f, -0.555570f, -0.382683f, -0.195090f,
         0.000000f,  0.195090f,  0.382683f,  0.555570f,
         0.707106f,  0.831469f,  0.923879f,  0.980785f
    };

    float FastSin(float angle) {
        // 1. Wrap the angle bounds natively without the slow fmod()
        while (angle < 0.0f) angle += TWO_PI;
        while (angle >= TWO_PI) angle -= TWO_PI;

        // 2. Find the closest table index (a)
        float exactIndex = angle / STEP;
        int index = (int)exactIndex;
        if (index >= TABLE_SIZE) index = 0; // Safety clamp

        float a_sin = SIN_TABLE[index];
        float a_cos = COS_TABLE[index];

        // 3. Calculate the tiny remainder (b)
        float b = angle - (index * STEP);

        // 4. Taylor series approximation of the tiny remainder
        float b_sin = b;
        float b_cos = 1.0f - (0.5f * b * b);

        // 5. Angle Addition Identity: sin(a + b) = sin(a)cos(b) + cos(a)sin(b)
        return (a_sin * b_cos) + (a_cos * b_sin);
    }

    float FastCos(float angle) {
        while (angle < 0.0f) angle += TWO_PI;
        while (angle >= TWO_PI) angle -= TWO_PI;

        float exactIndex = angle / STEP;
        int index = (int)exactIndex;
        if (index >= TABLE_SIZE) index = 0;

        float a_sin = SIN_TABLE[index];
        float a_cos = COS_TABLE[index];

        float b = angle - (index * STEP);

        float b_sin = b;
        float b_cos = 1.0f - (0.5f * b * b);

        // Angle Addition Identity: cos(a + b) = cos(a)cos(b) - sin(a)sin(b)
        return (a_cos * b_cos) - (a_sin * b_sin);
    }
}