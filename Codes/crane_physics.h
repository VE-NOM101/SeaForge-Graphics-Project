#pragma once
// ==================== B2: CRANE 2D SPHERICAL PENDULUM + SWING COLLISION ====================
//
// The container hangs from the boom tip on a cable of length L.
// It swings as a 2D spherical pendulum with two independent angles:
//   θ (theta) — lateral swing (perpendicular to boom axis, driven by turret J/L rotation)
//   φ (phi)   — longitudinal swing (along boom axis, driven by boom pitch O/P changes)
//
// Physics (exact nonlinear ODE for each axis):
//   θ'' = -(g/L)·sin(θ) - b·θ' + driving_turret
//   φ'' = -(g/L)·sin(φ) - b·φ' + driving_pitch
//
// Collision: if swinging hook overlaps cargo ship AABB → elastic bounce + red flash.
// Cable: rendered as Catmull-Rom spline (boomTip → midpoint displaced by swing → hookPos).

#include <glm/glm.hpp>

// ── Pendulum state — 2 independent axes ─────────────────────────────────────
extern float pendulumTheta;       // lateral swing angle (rad) — from turret rotation
extern float pendulumOmega;       // lateral angular velocity (rad/s)
extern float pendulumPhi;         // longitudinal swing angle (rad) — from boom pitch
extern float pendulumPhiOmega;    // longitudinal angular velocity (rad/s)

// ── Collision state ─────────────────────────────────────────────────────────
extern bool  triggerCollision;    // true when container hits ship hull
extern float collisionFlashTimer; // counts down from 0.5s to 0 (red flash duration)

// ── Cable length (dynamic: extends during lowering, retracts during lift/return) ─
extern float craneCableLength;    // current cable length in meters (default 6.0)

// ── Boom tip + hook world positions (for spline cable rendering) ────────────
extern glm::vec3 boomTipWorldPos; // world-space boom tip (cable attachment point)
extern glm::vec3 hookWorldPos;    // world-space hook/container position

// ── Public API ──────────────────────────────────────────────────────────────

// Call every frame after processInput.
// prevCraneAngle/prevCranePitch: previous frame values for computing angular rates.
// carrying: true when a container is attached (heavier → slower damp, less drive sensitivity)
//           false for empty hook (lighter → faster damp, more drive sensitivity)
void updateCranePhysics(float craneArmAngle, float craneArmPitch,
                        float prevCraneAngle, float prevCranePitch,
                        float shipX, float shipZ, float dt, bool carrying = false);
