#pragma once
// ==================== B1: WAVE WATER MESH + WAVE-SHIP INTERACTION ====================
// Gerstner wave water grid, CPU-side wave sampling, pitch/roll/heave for all vessels.

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"

// Forward-declare GLFW window (avoid pulling full GLFW header into every includer)
struct GLFWwindow;

// ── Wave parameters (shared so shaders use identical constants) ──────────────
struct WaveParams {
    float k;      // wave number
    float A;      // amplitude
    float w;      // angular frequency
    float phi;    // phase offset
    glm::vec2 dir; // wave direction (normalized)
};

// Two superimposed Gerstner waves
extern const WaveParams WAVE1;
extern const WaveParams WAVE2;

// ── Cargo ship wave-response state ──────────────────────────────────────────
extern float shipX;
extern float shipPitch;          // radians, nose-up positive
extern float shipRoll;           // radians, port-down positive (B4: driven by metacentric ODE)
extern float shipHeave;          // vertical offset (B4: wave heave + draft)

// Damping parameters
extern float pitchDamp;
extern float rollDamp;
extern float naturalRollFreq;

// B4: wave-computed targets (set by updateWavePhysics, consumed by ship_physics)
extern float waveTargetRoll;     // wave-induced roll target (before metacentric dynamics)
extern float waveTargetHeave;    // smoothed wave heave (before draft offset)

// ── Ancient boat state (position + wave response + rowing) ──────────────────
extern float boatX, boatZ, boatYaw;       // world position and heading (radians)
extern float boatPitch, boatRoll, boatHeave;
extern float boatVelX, boatVelZ;          // linear velocity
extern float boatAngVel;                  // yaw angular velocity
extern float leftOarPhase, rightOarPhase; // 0..1 stroke animation cycle
extern bool  leftOarActive, rightOarActive;
extern bool  boatReverseMode;             // true when Ctrl held (reverse rowing)
extern float leftOarTheta, rightOarTheta; // physics-driven sweep angle (radians)
extern float leftOarOmega, rightOarOmega; // oar angular velocity (rad/s)

// ── Patrol ship wave-response state ─────────────────────────────────────────
extern float patrolPitch, patrolRoll, patrolHeave;

// ── Buoy wave-response state (7 buoys) ──────────────────────────────────────
extern float buoyHeave[];
extern float buoyTiltX[];   // pitch-like tilt
extern float buoyTiltZ[];   // roll-like tilt

// ── Water mesh GPU handles ──────────────────────────────────────────────────
extern unsigned int waterVAO, waterVBO, waterEBO;
extern int waterIndexCount;

// ── Public API ──────────────────────────────────────────────────────────────

// Call once at init: generates a flat grid mesh (gridX x gridZ vertices)
void createWaterMesh(int gridX, int gridZ, float worldW, float worldD);

// CPU-side Gerstner wave height at world position (wx,wz) at time t
float waveAt(float wx, float wz, float t);

// Call every frame AFTER processInput: updates cargo ship wave response.
// shipZ is passed in (owned by main.cpp).
void updateWavePhysics(float shipZ, float dt, float time);

// Call every frame: processes T/G keys for ancient boat rowing,
// updates boat position/velocity, and wave response for all other vessels.
void updateBoatAndOthers(GLFWwindow* window, float dt, float time);

// Renders the water grid. Sets useWave=1 and passes time uniform so vertex
// shader applies Gerstner displacement. Restores useWave=0 afterward.
void drawWaterMesh(Shader &s, glm::mat4 parentTransform, float time);

