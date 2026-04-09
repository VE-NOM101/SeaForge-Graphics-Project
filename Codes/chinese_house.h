#pragma once
// ================================================================
// chinese_house.h  —  Stylized traditional Chinese house (pagoda)
//                     with curved B-spline roofs on a raised platform.
// ================================================================

#include "shader.h"
#include <glm/glm.hpp>

// Draw a complete Chinese house scene at world position (x, y, z).
// Includes: multi-layer platform, two-story pagoda house with curved
// roofs, entrance stairs, windows, surrounding trees and bushes.
void drawChineseHouse(Shader &s, glm::mat4 p, float x, float y, float z);
