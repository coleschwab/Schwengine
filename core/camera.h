#pragma once
#include "Vector4.h"
#include "Matrix4x4.h"

class Camera {
public:
    Camera();

    void update(float dt);
    Matrix4x4 getViewMatrix();

    // Add a getter so main.cpp or ImGui could check the state
    bool isUIModeActive() const { return uiMode; }

    float* getSensitivityPtr() { return &mouseSensitivity; }

private:
    Vector4 position;
    Vector4 forward;
    Vector4 up;

    float speed;
    float yaw;
    float pitch;
    float mouseSensitivity;

    bool firstMouse;
    float lastX;
    float lastY;

    // UI Toggle State
    bool uiMode = false;
    bool tabWasPressed = false;
};