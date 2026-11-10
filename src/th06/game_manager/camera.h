#pragma once

// Sets the camera matrices from camera.viewport, offset and zoom, with
// the far plane at 10000 + extra_far. Updates camera.distance.
void GameManager_SetUpCamera(float extra_far);
// Sets the camera matrices without offset or zoom.
void GameManager_SetUpFixedCamera(float extra_far);
