#pragma once
// ================================================================
// street_lamp.h  —  Classic double-arm street lamp with B-spline
//                   scrollwork arms and lantern heads.
// ================================================================

#include "shader.h"
#include <glm/glm.hpp>

// Evaluate a point on a uniform cubic B-spline.
//   pts: array of n control points
//   t:   parameter in [0, 1] spanning the full curve
glm::vec3 cubicBSplinePoint(const glm::vec3* pts, int n, float t);

// Render a tube of constant radius along a cubic B-spline.
//   segments: number of cylinder pieces used to approximate the curve
void drawTubeAlongSpline(Shader &s, glm::mat4 p, const glm::vec3* pts, int n,
                         float radius, int segments, glm::vec3 color);

// Draw one complete double-lamp street light at world position (x, y, z).
// y is the ground-level base of the lamp.
void drawStreetLamp(Shader &s, glm::mat4 p, float x, float y, float z, float rotY = 0.0f);
