#pragma once
// ==================== B3: SHIP DRAG / INERTIA + TURNING WITH PIVOT POINT ====================
// ==================== B4: REAL BUOYANCY + STABILITY (ARCHIMEDES)          ====================
//
// B3: Ship has linear momentum (no instant stop) AND rotational inertia.
//     Real ships rotate around a pivot point ~30% from the bow.
//     ,/. (comma/period) keys apply turning thrust; angular velocity decays via drag.
//     I/K keys apply forward/backward thrust; linear velocity decays via drag.
//
// B4: True Archimedes buoyancy: draft = mass/(rho*A).
//     Metacentric height GM = KB + BM - KG determines roll stability.
//     GM > 0 -> stable (self-righting); GM < 0 -> capsize.
//     Roll driven by metacentric pendulum ODE with wave forcing.

#include <glm/glm.hpp>

struct GLFWwindow;

// ── B3: Ship movement state ───────────────────────────────────────────────────
extern float shipVelocityZ;      // forward speed in ship-local frame (m/s)
extern float shipHeading;        // Y-axis rotation (radians), 0 = facing +Z
extern float shipAngularVel;     // yaw rate (radians/s)

// ── B4: Buoyancy state ───────────────────────────────────────────────────
extern float shipTotalMass;      // total ship + cargo mass (normalized units)
extern float shipCenterOfMassX;  // transverse CoM (port=-, starboard=+)
extern float shipCenterOfMassY;// vertical CoM height above keel
extern float shipCenterOfMassZ;
extern float shipPitchForContainers;
extern float shipPitchVelocity;      // pitch angular velocity for cargo trim ODE
extern float shipDraft;          // current draft (computed each frame)
extern float shipRollVelocity;   // for metacentric pendulum roll physics
extern bool  triggerCapsize;     // true when |shipRoll| > 70 deg
extern float capsizeTimer;       // animation timer for capsize sequence
extern float currentGM;

// ── D1: Overload sinking ──────────────────────────────────────────────────────
// Triggered when shipTotalMass > OVERLOAD_THRESHOLD (150 units = base 50 + ~4 slots heavily loaded).
// sinkingOffset accumulates each frame — applied as extra negative heave.
extern bool  triggerSinking;     // true once overload threshold crossed
extern float sinkingOffset;      // accumulated downward displacement (negative, grows each frame)

// ── B5: Cargo weight distribution ────────────────────────────────────────────
// 4 slots: port-outer (x=-1.8), port-inner (x=-0.6), starb-inner (+0.6), starb-outer (+1.8).
// Uneven loading shifts CoM_x → feeds B4 metacentric roll ODE → capsize if extreme.
extern float      containerWeights[4];  // mass per slot (normalized tons, min 0)
extern const float containerSlotX[4]; // ship-local transverse X per slot (fixed)
extern const float containerSlotZ[4];
extern int        activeSlot;           // slot being loaded/unloaded by [/] keys (0-3)

// ── Public API ────────────────────────────────────────────────────────────────

// B3: Process I/K/,/. input, apply linear drag + turning inertia + pivot point.
// Must be called BEFORE updateWavePhysics (updates shipX, shipZ, shipHeading).
void updateShipMovement(GLFWwindow* window, float dt);

// B4: Compute draft, KB, BM, GM; drive metacentric roll ODE; apply buoyancy heave.
// Must be called AFTER updateWavePhysics (uses waveTargetRoll, waveTargetHeave).
void updateShipBuoyancy(float dt);
