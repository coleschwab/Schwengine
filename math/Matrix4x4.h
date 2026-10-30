#pragma once
#include "Vector4.h"
#include <cmath>
// ------------------------------------------------------------------
// Matrix4x4
// Implemented natively in Column-Major order to match Vulkan/GLSL 
// memory layout requirements without transposing before uniform binding.
// ------------------------------------------------------------------
struct Matrix4x4 {
    float m[16];

    Matrix4x4() {
        for (int i = 0; i < 16; i++) {
            m[i] = 0.0f;
        }
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;
    }

    static Matrix4x4 Identity() {
        return Matrix4x4();
    }

    Matrix4x4 Inverse() const {
        Matrix4x4 inv;

        inv.m[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
            m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

        inv.m[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
            m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

        inv.m[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
            m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

        inv.m[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

        inv.m[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
            m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

        inv.m[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
            m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

        inv.m[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
            m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

        inv.m[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

        inv.m[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
            m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

        inv.m[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
            m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

        inv.m[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

        inv.m[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

        inv.m[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
            m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

        inv.m[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
            m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

        inv.m[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

        inv.m[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        float det = m[0] * inv.m[0] + m[1] * inv.m[4] + m[2] * inv.m[8] + m[3] * inv.m[12];

        if (det == 0.0f) {
            return Matrix4x4::Identity(); // Fallback to avoid division by zero
        }

        det = 1.0f / det;

        for (int i = 0; i < 16; i++) {
            inv.m[i] = inv.m[i] * det;
        }

        return inv;
    }

    Vector4 multiply(const Vector4& v) const {
        Vector4 result;
        result.x = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w;
        result.y = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w;
        result.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w;
        result.w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w;
        return result;
    };

    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 res;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                res.m[col * 4 + row] =
                    m[0 * 4 + row] * other.m[col * 4 + 0] +
                    m[1 * 4 + row] * other.m[col * 4 + 1] +
                    m[2 * 4 + row] * other.m[col * 4 + 2] +
                    m[3 * 4 + row] * other.m[col * 4 + 3];
            }
        }
        return res;
    }

    static Matrix4x4 Translation(float tx, float ty, float tz) {
        Matrix4x4 res;
        res.m[12] = tx;
        res.m[13] = ty;
        res.m[14] = tz;
        return res;
    }

    static Matrix4x4 Perspective(float fovRadians, float aspect, float nearP, float farP) {
        Matrix4x4 res;
        for (int i = 0; i < 16; i++) {
            res.m[i] = 0.0f;
        }

        float g = 1.0f / tanf(fovRadians * 0.5f);

        res.m[0] = g / aspect;
        // VULKAN SPECIFIC: Invert the Y-axis. GLSL expects Y to point up, but Vulkan's NDC Y points down.
        res.m[5] = -g;

        // VULKAN SPECIFIC: Depth range is [0, 1], not OpenGL's [-1, 1]
        res.m[10] = farP / (nearP - farP);
        res.m[11] = -1.0f;
        res.m[14] = (nearP * farP) / (nearP - farP);

        return res;
    }

    static Matrix4x4 RotationY(float radians) {
        Matrix4x4 res; // Starts as identity matrix
        float c = cosf(radians);
        float s = sinf(radians);

        res.m[0] = c;
        res.m[2] = -s;
        res.m[8] = s;
        res.m[10] = c;

        return res;
    }

    static Matrix4x4 Scale(float s) {
        Matrix4x4 res; // Starts as an Identity matrix
        res.m[0] = s;  // Scale X
        res.m[5] = s;  // Scale Y
        res.m[10] = s; // Scale Z
        return res;
    }

    static Matrix4x4 LookAt(const Vector4& position, const Vector4& target, const Vector4& up) {
        Vector4 zaxis = (position - target).normalize();    // The "forward" vector.
        Vector4 xaxis = up.cross(zaxis).normalize();        // The "right" vector.
        Vector4 yaxis = zaxis.cross(xaxis);                 // The "up" vector.
        Matrix4x4 view;
        view.m[0] = xaxis.x;
        view.m[1] = yaxis.x;
        view.m[2] = zaxis.x;
        view.m[3] = 0.0f;
        view.m[4] = xaxis.y;
        view.m[5] = yaxis.y;
        view.m[6] = zaxis.y;
        view.m[7] = 0.0f;
        view.m[8] = xaxis.z;
        view.m[9] = yaxis.z;
        view.m[10] = zaxis.z;
        view.m[11] = 0.0f;
        view.m[12] = -xaxis.dot(position);
        view.m[13] = -yaxis.dot(position);
        view.m[14] = -zaxis.dot(position);
        view.m[15] = 1.0f;
        return view;
    }
};