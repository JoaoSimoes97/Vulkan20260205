#pragma once

#include "camera.h"
#include <SDL3/SDL_keyboard.h>

/**
 * Updates camera position from keyboard state (WASD / arrows / QE).
 * Pan speed is applied per frame; keyState is from SDL_GetKeyboardState(nullptr).
 */
inline void CameraController_Update(Camera& camera, const bool* keyState, float panSpeed) {
    if (keyState == nullptr || panSpeed <= 0.f)
        return;
    float x, y, z;
    camera.GetPosition(x, y, z);
    if (keyState[SDL_SCANCODE_W]) z -= panSpeed;
    if (keyState[SDL_SCANCODE_S]) z += panSpeed;
    if (keyState[SDL_SCANCODE_A]) x -= panSpeed;
    if (keyState[SDL_SCANCODE_D]) x += panSpeed;
    if (keyState[SDL_SCANCODE_Q]) y -= panSpeed;
    if (keyState[SDL_SCANCODE_E]) y += panSpeed;
    if (keyState[SDL_SCANCODE_LEFT])  x -= panSpeed;
    if (keyState[SDL_SCANCODE_RIGHT]) x += panSpeed;
    if (keyState[SDL_SCANCODE_UP])    y += panSpeed;
    if (keyState[SDL_SCANCODE_DOWN])  y -= panSpeed;
    camera.SetPosition(x, y, z);
}
