// ==================== B6: SHIP AUTOPILOT — CUBIC B-SPLINE PATH FOLLOWER ====================
//
// Drives the patrol ship along a smooth B-spline path:
//   1. Ship position follows the spline curve directly
//   2. Heading smoothly aligns with path tangent (+PI since bow faces local -Z)
//   3. Speed decelerates near the endpoint for smooth docking
//   4. Works with wave physics (patrolPitch/Roll/Heave still applied)

#include "autopilot.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <glm/glm.hpp>

static const float PI = 3.14159265f;

// ── B6: Patrol ship state (globals shared via autopilot.h) ──────────────────
// Water grid: 70×70 centered at origin → X ∈ [-35,35], Z ∈ [-35,35]
// Dock: X ∈ [-21, 1], Z ∈ [-11, 11]. Water edge at X ≈ 1.
// Camera looks from SE → bottom-right of scene = positive X, positive Z.
//
// Start: near SE corner of water (further right & closer to the edge)
float patrolX          = 28.0f;
float patrolZ          = 30.0f;
// Initial heading: computed from path tangent at t=0 (+ PI for bow facing -Z)
// Path starts heading roughly west → tangent ≈ (-1, 0) → atan2(-1,0)+PI = PI/2
float patrolHeading    = 1.57f;
float patrolVelocityZ  = 0.0f;
float patrolAngularVel = 0.0f;

Autopilot::Autopilot()
{
    // Path from SE corner of water → sweeps west → curves northwest →
    // stops at dock water edge (X≈2, does NOT overlap dock).
    //
    // Dock platform: X ∈ [-21, 1], Z ∈ [-11, 11]. Edge at X=1.
    // Path ends at X≈2 (just outside the dock edge, in the water).
    //
    // 15 control points → 12 B-spline segments = long smooth arc.
    // Final 3 duplicated for spline convergence.
    waypoints = {
    { 28.0f, 0.0f, 30.0f},   // Start

    // Wide smooth arc (NO zig-zag)
    { 26.0f, 0.0f, 29.8f},
    { 23.0f, 0.0f, 29.0f},
    { 20.0f, 0.0f, 27.8f},
    { 17.0f, 0.0f, 26.2f},
    { 14.0f, 0.0f, 24.5f},

    // Transition — tightening curve
    { 11.0f, 0.0f, 23.0f},
    { 8.5f,  0.0f, 21.8f},
    { 6.5f,  0.0f, 20.8f},
    { 5.0f,  0.0f, 20.0f},

    // Align to dock direction
    { 3.5f,  0.0f, 19.3f},
    { 2.0f,  0.0f, 18.8f},

    // STRAIGHT END (very important)
    { 0.0f,  0.0f, 18.5f},
    { -3.0f, 0.0f, 18.5f},
    { -6.0f, 0.0f, 18.5f},
    { -10.0f,0.0f, 18.5f}
    };
}

void Autopilot::reset()
{
    currentT = 0.0f;
    integral = 0.0f;
    prevError = 0.0f;
    enabled = false;
}

// ── Uniform cubic B-spline evaluation ───────────────────────────────────────
glm::vec3 Autopilot::getSplinePoint(float t)
{
    int n = (int)waypoints.size();
    int i = (int)t;
    if (i < 0) i = 0;
    if (i > n - 4) i = n - 4;
    float u = t - (float)i;

    float u2 = u * u;
    float u3 = u2 * u;
    float b0 = (1.0f - u) * (1.0f - u) * (1.0f - u) / 6.0f;
    float b1 = (3.0f * u3 - 6.0f * u2 + 4.0f) / 6.0f;
    float b2 = (-3.0f * u3 + 3.0f * u2 + 3.0f * u + 1.0f) / 6.0f;
    float b3 = u3 / 6.0f;

    return b0 * waypoints[i] + b1 * waypoints[i+1]
         + b2 * waypoints[i+2] + b3 * waypoints[i+3];
}

// ── Main update: ship follows spline directly ───────────────────────────────
void Autopilot::update(float dt)
{
    if (!enabled || waypoints.size() < 4) return;
    if (dt <= 0.0f || dt > 0.5f) return;

    float maxT = (float)(waypoints.size() - 3);

    // ── 1. Sample spline position and tangent ────────────────────────────
    glm::vec3 pos     = getSplinePoint(currentT);
    glm::vec3 posNext = getSplinePoint(glm::min(currentT + 0.02f, maxT));
    glm::vec3 tangent = posNext - pos;
    float tangentLen  = glm::length(tangent);

    // ── 2. Set ship position directly from spline ────────────────────────
    patrolX = pos.x;
    patrolZ = pos.z;

    // ── 3. Smooth heading toward tangent direction ───────────────────────
    //   Patrol ship bow faces LOCAL -Z → add PI to align bow with travel.
    if (tangentLen > 0.0001f) {
        glm::vec3 tDir = glm::normalize(tangent);
        float desiredHeading = atan2f(tDir.x, tDir.z) + PI;

        float headingDiff = desiredHeading - patrolHeading;
        while (headingDiff >  PI) headingDiff -= 2.0f * PI;
        while (headingDiff < -PI) headingDiff += 2.0f * PI;

        float turnRate = glm::min(3.0f * dt, 1.0f);
        patrolHeading += headingDiff * turnRate;
    }

    // ── 4. Speed control (decelerate near endpoint) ──────────────────────
    glm::vec3 endPoint = waypoints.back();
    float distToEnd = glm::length(pos - endPoint);
    float cruiseSpeed = 5.0f;
    float speed = glm::clamp(distToEnd * 0.35f, 0.0f, cruiseSpeed);
    patrolVelocityZ = speed;

    // ── 5. Advance path parameter based on speed ─────────────────────────
    float progressRate = speed / 8.0f;
    currentT += progressRate * dt;

    // ── 6. Stop condition ────────────────────────────────────────────────
    if (currentT >= maxT - 0.01f || distToEnd < 1.5f) {
        currentT = maxT;
        pos = getSplinePoint(maxT);
        patrolX = pos.x;
        patrolZ = pos.z;
        patrolVelocityZ  = 0.0f;
        patrolAngularVel = 0.0f;
        enabled = false;
        printf("[B6] Autopilot: Patrol complete — ship docked.\n");
    }
}