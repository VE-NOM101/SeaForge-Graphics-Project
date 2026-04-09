#ifndef AUTOPILOT_H
#define AUTOPILOT_H
// ==================== B6: SHIP AUTOPILOT (CUBIC B-SPLINE + PID) ====================
//
// Autonomous navigation for the patrol ship using:
//   1. Cubic B-spline path (smooth curve through waypoints)
//   2. PID cross-track error correction (lateral stability)
//   3. Heading-toward-tangent steering (turns toward path direction)
//   4. Distance-based speed control (auto slow-down + docking)
//
// Integrates with existing wave physics (patrolPitch/Roll/Heave).
// Toggle: H key enables/disables patrol autopilot.

#include <vector>
#include <glm/glm.hpp>

// ── B6: Patrol ship dynamic state (shared with main.cpp + wave_physics.cpp) ──
extern float patrolX;             // world X position
extern float patrolZ;             // world Z position
extern float patrolHeading;       // yaw (radians), 0 = facing +Z
extern float patrolVelocityZ;     // forward speed (m/s)
extern float patrolAngularVel;    // yaw rate (rad/s)

class Autopilot {
public:
    bool enabled = false;
    float currentT = 0.0f;       // progression along spline [0, numSegments]

    // PID State
    float integral = 0.0f;
    float prevError = 0.0f;

    // PID Gains (tuned for patrol ship turning radius)
    float Kp = 1.5f;
    float Ki = 0.005f;
    float Kd = 0.8f;

    std::vector<glm::vec3> waypoints;

    Autopilot();
    void reset();

    // Core: updates patrol ship position, heading, velocity each frame
    void update(float dt);

    // Evaluate cubic B-spline at parameter t
    glm::vec3 getSplinePoint(float t);
};

#endif