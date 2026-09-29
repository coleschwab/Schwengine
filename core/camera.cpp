#include "Camera.h"
#include <windows.h>
#include "MathUtils.h"

Camera::Camera() {
    position = { 0.0f, 0.0f, 10.0f, 1.0f }; // Starting 5 units back
    forward = { 0.0f, 0.0f, -1.0f, 0.0f }; // Looking toward the origin
    up = { 0.0f, 1.0f, 0.0f, 0.0f };       // Y is up
    speed = 5.0f; // 5 units per second
    yaw = -90.0f; // Facing along negative Z
    pitch = 0.0f; // Level with the horizon
    mouseSensitivity = 0.08f; // Adjust as needed
    firstMouse = true; // To handle the first mouse movement
    lastX = 400.0f; // Initialize lastX
    lastY = 300.0f; // Initialize lastY

    ShowCursor(FALSE);
}

void Camera::update(float dt) {
    // --- 0. UI MODE TOGGLE (TAB KEY) ---
    bool tabIsPressed = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;

    if (tabIsPressed && !tabWasPressed) {
        uiMode = !uiMode;
        if (uiMode) {
            ShowCursor(TRUE);
        }
        else {
            ShowCursor(FALSE);
            firstMouse = true;
        }
    }
    tabWasPressed = tabIsPressed;

    if (uiMode) return;

    // --- 1. MOUSE LOOK (CENTER-LOCK METHOD) ---
    HWND hwnd = GetActiveWindow();
    // Only hijack the mouse if the engine actually has focus!
    if (hwnd && hwnd == GetForegroundWindow()) {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // Calculate the exact center of the window
        POINT center = { (clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2 };
        ClientToScreen(hwnd, &center);

        POINT cursorPos;
        if (GetCursorPos(&cursorPos)) {
            if (firstMouse) {
                SetCursorPos(center.x, center.y); // Force to center immediately
                firstMouse = false;
                return; // Skip delta calc on the first frame to avoid a snap
            }

            // Delta is now just current position minus the dead center
            float xoffset = (float)cursorPos.x - (float)center.x;
            float yoffset = (float)center.y - (float)cursorPos.y;

            // Only update if the mouse actually moved
            if (xoffset != 0.0f || yoffset != 0.0f) {
                xoffset *= mouseSensitivity;
                yoffset *= mouseSensitivity;

                yaw += xoffset;
                pitch += yoffset;

                if (pitch > 89.0f) pitch = 89.0f;
                if (pitch < -89.0f) pitch = -89.0f;

                float radYaw = yaw * Math::PI / 180.0f;
                float radPitch = pitch * Math::PI / 180.0f;

                Vector4 newForward;
                newForward.x = Math::FastCos(radYaw) * Math::FastCos(radPitch);
                newForward.y = Math::FastSin(radPitch);
                newForward.z = Math::FastSin(radYaw) * Math::FastCos(radPitch);
                newForward.w = 0.0f;

                newForward.normalize();
                forward = newForward;

                // Instantly snap the cursor back to the dead center for the next frame
                SetCursorPos(center.x, center.y);
            }
        }
    }

    // --- 2. KEYBOARD MOVEMENT ---
    if (GetAsyncKeyState('W') & 0x8000) position = position + (forward * (speed * dt));
    if (GetAsyncKeyState('S') & 0x8000) position = position - (forward * (speed * dt));

    Vector4 right = up.cross(forward);
    right.normalize();

    if (GetAsyncKeyState('D') & 0x8000) position = position - (right * (speed * dt));
    if (GetAsyncKeyState('A') & 0x8000) position = position + (right * (speed * dt));
}

Matrix4x4 Camera::getViewMatrix() {
    // The target is simply the camera's position plus its forward direction
    Vector4 target = position + forward;
    return Matrix4x4::LookAt(position, target, up);
}