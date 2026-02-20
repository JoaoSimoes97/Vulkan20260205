#pragma once

#include "camera.h"
#include <SDL3/SDL_keyboard.h>
#include <cmath>

/**
 * Updates camera position from keyboard state (WASD / arrows / QE).
 * Movement is relative to camera facing direction (FPS-style).
 * Uses delta time for frame-rate independent movement.
 * @param camera The camera to update
 * @param keyState Keyboard state from SDL_GetKeyboardState(nullptr)
 * @param moveSpeed Units per second (e.g., 8.0f = 8 meters per second)
 * @param deltaTime Seconds since last frame
 */
inline void CameraController_Update(Camera& camera, const bool* keyState, float moveSpeed, float deltaTime) {
    if (keyState == nullptr || moveSpeed <= 0.f || deltaTime <= 0.f)
        return;
    
    // Frame-rate independent movement: speed * deltaTime
    float move = moveSpeed * deltaTime;
    
    float x, y, z;
    camera.GetPosition(x, y, z);
    
    // Get camera directions
    float fx, fy, fz;
    camera.GetForward(fx, fy, fz);
    
    float rx, ry, rz;
    camera.GetRight(rx, ry, rz);
    
    // Forward/backward (relative to camera facing, but stay level - ignore Y component)
    float fwdX = fx, fwdZ = fz;
    float fwdLen = std::sqrt(fwdX * fwdX + fwdZ * fwdZ);
    if (fwdLen > 0.001f) {
        fwdX /= fwdLen;
        fwdZ /= fwdLen;
    }
    
    if (keyState[SDL_SCANCODE_W]) { x += fwdX * move; z += fwdZ * move; }
    if (keyState[SDL_SCANCODE_S]) { x -= fwdX * move; z -= fwdZ * move; }
    
    // Left/right (strafe)
    if (keyState[SDL_SCANCODE_A]) { x -= rx * move; z -= rz * move; }
    if (keyState[SDL_SCANCODE_D]) { x += rx * move; z += rz * move; }
    
    // Up/down (world Y axis)
    if (keyState[SDL_SCANCODE_E]) y += move;
    if (keyState[SDL_SCANCODE_Q]) y -= move;
    
    // Arrow keys for rotation
    constexpr float kRotateSpeed = 2.0f; // radians per second
    float rotAmount = kRotateSpeed * deltaTime;
    if (keyState[SDL_SCANCODE_LEFT])  camera.AddRotation(-rotAmount, 0.f);
    if (keyState[SDL_SCANCODE_RIGHT]) camera.AddRotation(rotAmount, 0.f);
    if (keyState[SDL_SCANCODE_UP])    camera.AddRotation(0.f, rotAmount);
    if (keyState[SDL_SCANCODE_DOWN])  camera.AddRotation(0.f, -rotAmount);
    
    camera.SetPosition(x, y, z);
}

/**
 * Updates camera rotation from mouse movement (FPS-style look).
 * @param camera The camera to update
 * @param deltaX Mouse X movement (pixels)
 * @param deltaY Mouse Y movement (pixels)
 * @param sensitivity Mouse sensitivity (radians per pixel)
 */
inline void CameraController_MouseLook(Camera& camera, float deltaX, float deltaY, float sensitivity = 0.002f) {
    camera.AddRotation(deltaX * sensitivity, -deltaY * sensitivity);
}
