// ==================== B3: SHIP DRAG / INERTIA + TURNING WITH PIVOT POINT ====================
// ==================== B4: REAL BUOYANCY + STABILITY (ARCHIMEDES)          ====================
//
// B3 Physics — Linear momentum + rotational inertia:
//
//   Linear:   F_thrust(I/K) - F_drag = m * a
//             shipVelocityZ -= shipDrag * shipVelocityZ * dt   (exponential decay)
//             Position updates along heading direction each frame.
//
//   Rotation: tau_turn( , / . ) - tau_angDrag = I * alpha
//             shipAngularVel -= shipAngDrag * shipAngularVel * dt
//
//   Pivot point: real ships rotate around a point ~30% from the bow.
//     Before heading update: compute pivot in world space.
//     After heading update:  reposition ship center relative to pivot.
//     Result: bow swings fast, stern lags behind — realistic turning arc.
//
// B4 Physics — Archimedes buoyancy + metacentric stability:
//
//   F_buoy   = rho_water * g * V_sub
//   draft    = totalMass / (rho_water * waterplaneArea)
//   KB       = draft / 2                         (center of buoyancy height)
//   BM       = I_waterplane / V_sub              (metacentric radius)
//   KG       = shipCenterOfMassY                 (height of center of mass)
//   GM       = KB + BM - KG                      (metacentric height)
//
//   Roll ODE (metacentric pendulum):
//     alpha_roll = -(g * GM / (KG * 0.8)) * shipRoll
//                  - damping * shipRollVelocity
//                  + wave_forcing + heel_forcing
//
//   GM > 0 -> stable (self-righting oscillation)
//   GM < 0 -> UNSTABLE -> capsize when |roll| > 70 deg

#include "ship_physics.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/glm.hpp>
#include "wave_physics.h"

// ── B3: Movement state ────────────────────────────────────────────────────────
float shipVelocityZ   = 0.0f;   // forward speed (ship-local frame)
float shipHeading     = 0.0f;   // Y-axis rotation (rad), 0 = facing +Z
float shipAngularVel  = 0.0f;   // yaw angular velocity (rad/s)

// ── B4: Buoyancy state ───────────────────────────────────────────────────────
float shipTotalMass     = 50.0f;   // base ship mass (normalized units)
float shipCenterOfMassX = 0.0f;    // transverse CoM offset (port=-, starboard=+)
float shipCenterOfMassY = 1.2f;    // vertical CoM height above keel
float shipCenterOfMassZ = 0.0f;
float shipPitchForContainers = 0.0f;
float shipPitchVelocity     = 0.0f;    // pitch angular velocity (rad/s) for trim ODE
float shipDraft         = 0.5f;    // current draft (recomputed each frame)
float shipRollVelocity  = 0.0f;    // roll angular velocity (rad/s)
bool  triggerCapsize    = false;
float capsizeTimer      = 0.0f;
bool  triggerSinking    = false;   // D1: set when shipTotalMass > 100
float sinkingOffset     = 0.0f;   // D1: accumulated downward displacement

// ── B5: Cargo weight distribution ────────────────────────────────────────────
// 4 container slots: port-outer, port-inner, starboard-inner, starboard-outer.
// Ship-local X positions determine transverse CoM for stability.
// [/] keys add/remove weight from activeSlot; C cycles to next slot.
float containerWeights[4]    = {5.0f, 5.0f, 5.0f, 5.0f}; // tons per slot (equal loading)
const float containerSlotX[4] = {
    -1.8f,
    -0.6f,
     0.6f,
     1.8f
};
const float containerSlotZ[4] = {
    -7.5f,  // slot 0: port-outer (furthest aft)
    -6.5f,  // slot 1: port-inner
    -5.5f,  // slot 2: starboard-inner
    -4.5f   // slot 3: starboard-outer (closest to bridge)
}; // ship-local longitudinal Z — must match visual position (~Z=-6 on deck)
int   activeSlot             = 0;                           // slot currently selected (0-3)

// ── B3: Linear motion constants ───────────────────────────────────────────────
static const float SHIP_THRUST     = 8.0f;    // acceleration from I/K keys (m/s^2)
static const float SHIP_DRAG       = 2.5f;    // linear drag coefficient
static const float SHIP_MAX_SPEED  = 6.0f;    // speed clamp (m/s)

// ── B3: Turning constants ─────────────────────────────────────────────────────
static const float SHIP_TURN_THRUST = 1.5f;   // angular accel from Q/E keys (rad/s^2)
static const float SHIP_ANG_DRAG    = 3.0f;   // angular drag coefficient
static const float SHIP_MAX_ANG_VEL = 0.8f;   // angular velocity clamp (rad/s)
// Pivot point: 30% from bow -> forward of geometric center by ~2.5 units
// (ship hull half-length = ~9.0, 30% of 18 = 5.4, offset from center = 9-5.4 = 3.6,
//  but reduced to 2.5 for visual balance)
static const float PIVOT_OFFSET     = 2.5f;

// ── B4: Buoyancy constants ────────────────────────────────────────────────────
// Hull geometry constants (must match drawCargoShip dimensions!)
//   hullW = 5.0 (beam), hullL = 18.0 (length)
//   Waterplane area = effective L × W for Archimedes draft calculation
static const float RHO_WATER        = 1.025f;  // seawater density (normalized)
static const float WATERPLANE_W     = 5.0f;    // waterplane beam (matches hull width)
static const float WATERPLANE_L     = 18.0f;   // waterplane effective length (matches hull)
static const float WATERPLANE_AREA  = WATERPLANE_L * WATERPLANE_W;  // = 90.0
static const float GRAVITY          = 9.81f;
static const float ROLL_DAMPING     = 1.5f;    // roll viscous damping
static const float PITCH_STIFFNESS  = 15.0f;   // pitch spring stiffness (restoring force)
static const float PITCH_DAMPING    = 4.0f;    // pitch viscous damping
static const float CAPSIZE_ANGLE    = glm::radians(70.0f);   // capsize trigger
static const float CAPSIZE_TARGET   = glm::radians(90.0f);   // final capsize angle

// ── External globals (owned by other modules) ─────────────────────────────────
// Declared explicitly here to avoid pulling wave_physics.h's glad/shader dependencies.
extern float shipX;          // wave_physics.cpp — cargo ship X position
extern float shipZ;          // main.cpp        — cargo ship Z position
extern float shipRoll;       // wave_physics.cpp — driven by metacentric ODE (B4)
extern float shipHeave;      // wave_physics.cpp — set here (wave heave + draft)
extern float waveTargetRoll;   // wave_physics.cpp — wave-induced roll target for ODE
extern float waveTargetHeave;  // wave_physics.cpp — smoothed wave heave

float currentGM = 0.0f;


// ═══════════════════════════════════════════════════════════════════════════
// B3: updateShipMovement — velocity-based motion + turning with pivot point
// ═══════════════════════════════════════════════════════════════════════════
void updateShipMovement(GLFWwindow* window, float dt)
{
    if (dt <= 0.0f || dt > 0.5f) return;
    if (triggerCapsize) {
        // During capsize: kill velocities, no input
        shipVelocityZ  *= 0.95f;
        shipAngularVel *= 0.95f;
        return;
    }

    // ── 1. Read thrust input (I/K for forward/back) ──────────────────────
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        shipVelocityZ += SHIP_THRUST * dt;
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        shipVelocityZ -= SHIP_THRUST * dt;

    // ── 2. Linear drag (exponential velocity decay) ──────────────────────
    //   dv/dt = -drag * v  ->  v(t) = v0 * e^(-drag*t)
    //   Discrete: v -= drag * v * dt
    shipVelocityZ -= SHIP_DRAG * shipVelocityZ * dt;
    shipVelocityZ = glm::clamp(shipVelocityZ, -SHIP_MAX_SPEED, SHIP_MAX_SPEED);

    // ── 3. Forward motion along heading ──────────────────────────────────
    //   heading = 0 -> ship faces +Z
    //   Ship moves in the direction it's pointing.
    shipX += sinf(shipHeading) * shipVelocityZ * dt;
    shipZ += cosf(shipHeading) * shipVelocityZ * dt;

    // ── 4. Read turning input ( , = port, . = starboard) ─────────────────
    //   Q/E are reserved for camera (quit / up).
    if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS)
        shipAngularVel += SHIP_TURN_THRUST * dt;   // turn port (left)
    if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS)
        shipAngularVel -= SHIP_TURN_THRUST * dt;   // turn starboard (right)

    // ── 5. Angular drag (exponential decay) ──────────────────────────────
    shipAngularVel -= SHIP_ANG_DRAG * shipAngularVel * dt;
    shipAngularVel = glm::clamp(shipAngularVel, -SHIP_MAX_ANG_VEL, SHIP_MAX_ANG_VEL);

    // ── 6. Pivot-point rotation ──────────────────────────────────────────
    //   Real ships rotate around a point ~30% from the bow, not the center.
    //   The bow swings fast, the stern lags behind.
    //
    //   Algorithm:
    //     A. Find pivot position in world space (before heading change)
    //     B. Update heading
    //     C. Reposition ship center relative to pivot (with new heading)

    // Step A: pivot is PIVOT_OFFSET forward of ship center, along current heading.
    //   Ship forward vector = (sinf(heading), 0, cosf(heading)).
    //   Pivot lies FORWARD: pivotZ = shipZ + cosf(heading)*offset (not minus).
    float pivotX = shipX + sinf(shipHeading) * PIVOT_OFFSET;
    float pivotZ = shipZ + cosf(shipHeading) * PIVOT_OFFSET;

    // Step B: update heading
    shipHeading += shipAngularVel * dt;

    // Step C: ship center is PIVOT_OFFSET behind the pivot, along new heading
    shipX = pivotX - sinf(shipHeading) * PIVOT_OFFSET;
    shipZ = pivotZ - cosf(shipHeading) * PIVOT_OFFSET;
}

// ═══════════════════════════════════════════════════════════════════════════
// B4: updateShipBuoyancy — Archimedes draft + metacentric roll + capsize
// ═══════════════════════════════════════════════════════════════════════════
void updateShipBuoyancy(float dt)
{
    if (dt <= 0.0f || dt > 0.5f) return;

    // ── B5: Update ship total mass and transverse CoM from cargo slots ────
    //   totalMass = base ship (50) + sum of all container weights
    //   CoM_x     = weighted average of slot X positions (positive = starboard)
    //   CoM_y     = rises slightly as containers stack higher (0.015 per ton)
    {
        float totalCargo = 0.0f, weightedX = 0.0f, weightedZ = 0.0f;
        for (int i = 0; i < 4; i++) {
            totalCargo += containerWeights[i];
            weightedX += containerWeights[i] * containerSlotX[i];
            weightedZ += containerWeights[i] * containerSlotZ[i];  // ✅ ADD THIS
        }
        shipTotalMass     = 50.0f + totalCargo;
        shipCenterOfMassX = (shipTotalMass > 0.001f) ? (weightedX / shipTotalMass) : 0.0f;

        shipCenterOfMassY = 1.2f + totalCargo * 0.015f;
        shipCenterOfMassZ = (shipTotalMass > 0.001f) ? (weightedZ / shipTotalMass) : 0.0f;
    }

    // ── 1. Compute draft from Archimedes' principle ──────────────────────
    //   F_buoy = rho * g * V_sub = weight = m * g
    //   V_sub  = waterplaneArea * draft
    //   => draft = totalMass / (rho * waterplaneArea)
    shipDraft = shipTotalMass / (RHO_WATER * WATERPLANE_AREA);

    // ── 2. Stability parameters ──────────────────────────────────────────
    //   KB = draft / 2     (center of buoyancy at half-draft depth)
    //   BM = I_wp / V_sub  (metacentric radius)
    //        I_wp = (L * W^3) / 12  for rectangular waterplane
    //   KG = shipCenterOfMassY
    //   GM = KB + BM - KG  (metacentric height: positive = stable)
    float KB = shipDraft / 2.0f;
    float Vsub = shipDraft * WATERPLANE_AREA;
    float I_wp = (WATERPLANE_L * WATERPLANE_W * WATERPLANE_W * WATERPLANE_W) / 12.0f;
    float BM = (Vsub > 0.001f) ? (I_wp / Vsub) : 100.0f;  // guard div-by-zero
    float GM = KB + BM - shipCenterOfMassY;

    currentGM = GM;

    const char* stabilityState;

    if (GM > 0.5f)
        stabilityState = "STABLE";
    else if (GM > 0.0f)
        stabilityState = "WARNING";
    else
        stabilityState = "CRITICAL";
    // ── 3. Metacentric pendulum ODE for roll ─────────────────────────────
    //   The wave-sampled target roll provides external forcing.
    //   The restoring moment is proportional to GM (Archimedes).
    //   Transverse CoM offset (from uneven cargo) adds constant heel.
    //
    //   alpha = -(g * GM / (KG * 0.8)) * roll       [restoring]
    //           - damping * rollVelocity              [viscous damping]
    //           + wave_forcing                        [sea state]
    //           + heel_forcing                        [cargo imbalance]
    //
    //   When GM > 0: restoring moment -> stable oscillation
    //   When GM < 0: destabilizing -> roll grows -> capsize

    if (!triggerCapsize)
    {
        // Wave forcing: push roll toward wave-computed target
        float waveForcing = (waveTargetRoll - shipRoll) * 3.0f;

        // Heel from transverse center-of-mass offset (uneven cargo loading)
        float heelForcing = -GRAVITY * shipCenterOfMassX * 0.3f;

        // Effective KG (clamp to avoid div-by-zero for empty ship)
        float effectiveKG = glm::max(shipCenterOfMassY, 0.1f);

        // Roll angular acceleration
        float rollAlpha = -(GRAVITY * GM / (effectiveKG * 0.8f)) * shipRoll
                          - ROLL_DAMPING * shipRollVelocity
                          + waveForcing
                          + heelForcing;
        // ── Pitch trim ODE (damped spring toward cargo-offset target) ────
        //   Target: small tilt proportional to longitudinal CoM offset
        //   Spring: restoring force pulls pitch toward target
        //   Damping: prevents oscillation / divergence
        float targetTrim = shipCenterOfMassZ * 0.05f;
        float pitchAlpha = -PITCH_STIFFNESS * (shipPitchForContainers - targetTrim)
                           - PITCH_DAMPING * shipPitchVelocity;
        shipPitchVelocity += pitchAlpha * dt;
        shipPitchForContainers += shipPitchVelocity * dt;

        // Semi-implicit Euler (update velocity first for better energy behavior)
        shipRollVelocity += rollAlpha * dt;
        shipRoll += shipRollVelocity * dt;
    }

    // ── 4. Capsize detection ─────────────────────────────────────────────
    //   When |shipRoll| exceeds 70 deg, the ship is past the point of no return.
    if (!triggerCapsize && fabsf(shipRoll) > CAPSIZE_ANGLE)
    {
        triggerCapsize = true;
        capsizeTimer = 0.0f;
    }

    // ── 5. Capsize animation: roll to 90 deg ─────────────────────────────
    //   Ship smoothly rolls to 90 deg, input is frozen.
    if (triggerCapsize)
    {
        capsizeTimer += dt;
        float targetAngle = (shipRoll > 0.0f) ? CAPSIZE_TARGET : -CAPSIZE_TARGET;
        shipRoll += (targetAngle - shipRoll) * 2.0f * dt;
    }

    // ── 6. Apply draft to ship vertical position ─────────────────────────
    //   The hull geometry (drawCargoShip) is designed so that at shipHeave=0 the
    //   waterline (y=0 world) cuts the hull at ~36% height — a realistic default
    //   for a loaded cargo ship.  We therefore ONLY apply the DELTA draft change
    //   caused by cargo beyond the base ship mass, so that:
    //     • No cargo  → ship sits at correct default waterline (shipHeave ≈ wave)
    //     • Heavy cargo → ship sinks further, proportionally
    //
    //   BASE_DRAFT = 50 / (1.025 * 90) = 0.542 units (draft at empty ship mass)
    //   Extra sink  = (shipDraft - BASE_DRAFT) * 0.5   (half-draft scaling)
    static const float BASE_DRAFT = 50.0f / (RHO_WATER * WATERPLANE_AREA);
    float draftOffset = -(shipDraft - BASE_DRAFT) * 0.5f;
 

    // ── D1: Overload sinking ─────────────────────────────────────────────────
    //   When total mass exceeds 100 units the ship is overloaded.
    //   sinkingOffset accumulates over time — the excess mass drives the rate,
    //   so a heavily loaded ship sinks faster than a marginally overloaded one.
    //   Floor at -8.0 (fully submerged; no further descent needed visually).
    static const float OVERLOAD_THRESHOLD = 150.0f;  // base(50) + ~4 slots full = 210 max; 150 = clearly overloaded
    if (shipTotalMass > OVERLOAD_THRESHOLD)
    {
        triggerSinking = true;
        float excess   = shipTotalMass - OVERLOAD_THRESHOLD;
        // Sink rate: 0.03 units/s base + 0.0015 per excess ton
        sinkingOffset -= (0.03f + 0.0015f * excess) * dt;
        sinkingOffset  = glm::max(sinkingOffset, -8.0f);
    }

    shipHeave = waveTargetHeave + draftOffset + sinkingOffset;
}
