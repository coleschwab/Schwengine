#pragma once
#include "Vector4.h"
#include <cmath>

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
        res.m[5] = -g;
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