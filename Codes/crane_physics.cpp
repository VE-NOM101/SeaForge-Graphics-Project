// ==================== B2: CRANE 2D SPHERICAL PENDULUM + SWING COLLISION ====================
//
// Physics model — 2D spherical pendulum:
//
//   The container hangs from the boom tip on a cable of length L.
//   It has two independent degrees of freedom:
//
//   1. LATERAL swing (θ, theta):
//      Driven by turret rotation (J/L keys).
//      When the turret accelerates, the cable attachment point moves laterally,
//      injecting energy perpendicular to the boom axis.
//      ODE: θ'' = -(g/L)·sin(θ) - b·θ' + a_turret/L
//      where a_turret is the centripetal acceleration of the boom tip.
//
//   2. LONGITUDINAL swing (φ, phi):
//      Driven by boom pitch changes (O/P keys).
//      When the boom tilts, the attachment point accelerates along the boom axis.
//      ODE: φ'' = -(g/L)·sin(φ) - b·φ' + a_pitch/L
//      where a_pitch is the boom-tip tangential acceleration from pitch rate.
//
//   Both are independent damped pendulums (coupling is negligible for small angles).
//
//   The driving acceleration is computed from Newton's 2nd law:
//   - Turret: boom tip traces a circle of radius R_arm around the turret axis.
//     Tangential acceleration = R_arm · d²(turretAngle)/dt² ≈ R_arm · Δω/Δt
//   - Pitch: boom tip traces an arc of radius L_boom around the pitch pivot.
//     Tangential acceleration = L_boom · d²(pitchAngle)/dt² ≈ L_boom · Δω_pitch/Δt
//
//   Collision: AABB check against cargo ship hull → elastic bounce + red flash.

#include "crane_physics.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ── Pendulum state ──────────────────────────────────────────────────────────
float pendulumTheta       = 0.0f;   // lateral swing (rad)
float pendulumOmega       = 0.0f;   // lateral angular velocity (rad/s)
float pendulumPhi         = 0.0f;   // longitudinal swing (rad)
float pendulumPhiOmega    = 0.0f;   // longitudinal angular velocity (rad/s)

// ── Collision state ─────────────────────────────────────────────────────────
bool  triggerCollision     = false;
float collisionFlashTimer  = 0.0f;

// ── World positions ─────────────────────────────────────────────────────────
glm::vec3 boomTipWorldPos  = glm::vec3(0.0f);
glm::vec3 hookWorldPos     = glm::vec3(0.0f);

// ── Physical constants ──────────────────────────────────────────────────────
static const float GRAVITY       = 9.81f;     // m/s²
float craneCableLength = 6.0f;   // dynamic cable length (changed during lower/lift phases)
static const float PEND_DAMPING  = 0.8f;      // viscous damping (air + bearing friction)

// Driving gain: how much boom-tip acceleration maps to pendulum driving.
// Physically: a_drive / L, but we use effective arm lengths.
static const float TURRET_ARM_LENGTH = 10.3f; // boom tip distance from turret axis (boom length)
static const float BOOM_LENGTH       = 10.3f; // boom tip distance from pitch pivot
static const float DRIVE_SCALE       = 1.0f;  // overall driving sensitivity

static const float COLLISION_BOUNCE  = -0.4f; // elastic bounce factor
static const float FLASH_DURATION    = 0.5f;  // red flash duration (seconds)
static const float MAX_SWING         = glm::radians(80.0f); // clamp angle

// ── Crane geometry (must match drawMobileCrane) ─────────────────────────────
static const float CRANE_BASE_X = -3.0f;
static const float CRANE_BASE_Z = -5.0f;
static const float CRANE_BASE_Y =  6.7f;  // turntable top Y
static const float MAST_TOP_Y   =  4.3f;  // boom pivot above turntable
static const float BOOM_TIP_Z   = 10.3f;  // tip offset in boom-local Z
static const float BOOM_TIP_Y   =  0.3f;  // tip offset in boom-local Y

// ── Ship collision AABB ─────────────────────────────────────────────────────
static const float SHIP_HALF_X  = 2.5f;
static const float SHIP_HALF_Z  = 9.0f;
static const float SHIP_Y_MIN   = -1.0f;
static const float SHIP_Y_MAX   = 3.0f;

// ═══════════════════════════════════════════════════════════════════════════
// Helper: integrate one pendulum axis (exact nonlinear ODE, semi-implicit Euler)
// ═══════════════════════════════════════════════════════════════════════════
static void integratePendulumAxis(float &angle, float &omega,
                                  float drivingAccel, float dt, float damping)
{
    // α = -(g/L)·sin(angle) - b·ω + a_drive/L
    // Using sin(angle) exactly — not the small-angle approximation.
    float alpha = -(GRAVITY / craneCableLength) * sinf(angle)
                  - damping * omega
                  + drivingAccel / craneCableLength;

    // Semi-implicit Euler (update velocity first, then position — better energy behavior)
    omega += alpha * dt;
    angle += omega * dt;

    // Clamp: cable can't wrap around the sheave
    if (angle > MAX_SWING) {
        angle = MAX_SWING;
        if (omega > 0.0f) omega *= -0.3f;  // bounce off limit
    }
    if (angle < -MAX_SWING) {
        angle = -MAX_SWING;
        if (omega < 0.0f) omega *= -0.3f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// updateCranePhysics — called every frame
// ═══════════════════════════════════════════════════════════════════════════
void updateCranePhysics(float craneArmAngle, float craneArmPitch,
                        float prevCraneAngle, float prevCranePitch,
                        float shipX, float shipZ, float dt, bool carrying)
{
    if (dt <= 0.0f || dt > 0.5f) return;

    // Physics constants depend on load:
    //   Empty hook (~100 kg): high effective damping b=c/m, high drive sensitivity.
    //   Loaded container (~15 t): low effective damping, moderate drive sensitivity.
    //   Period T=2π√(L/g) is mass-independent, so only damping and driving differ.
    const float dampingCoeff = carrying ? PEND_DAMPING : 3.5f;
    const float driveScale   = carrying ? DRIVE_SCALE  : 2.2f;

    // ── 1. Compute driving accelerations from crane motion ───────────────
    //
    // Turret rotation:
    //   The boom tip is at radius R = TURRET_ARM_LENGTH from the turret axis.
    //   When the turret rotates, the tip has tangential acceleration:
    //     a_tangential = R · d²θ_turret/dt²
    //   We approximate d²θ/dt² ≈ (ω_current - ω_prev) / dt
    //   But we only have angle deltas, so:
    //     ω_turret = Δangle / dt
    //     a_turret ≈ R · Δω / dt = R · (Δangle₁ - Δangle₀) / dt²
    //   For simplicity and stability, we use the angular velocity directly
    //   as the driving term (impulse-like):
    //     a_drive_lateral = R · ω_turret  (velocity coupling, not acceleration)
    //   This is physically the Coriolis-like pseudo-force in the rotating frame.

    float turretDeltaDeg = craneArmAngle - prevCraneAngle;
    float turretOmega    = glm::radians(turretDeltaDeg) / dt;  // rad/s
    float driveLateral   = TURRET_ARM_LENGTH * turretOmega * driveScale;

    // Boom pitch:
    //   The boom tip is at distance L_boom from the pitch pivot.
    //   When boom pitch changes, the tip accelerates along the boom axis:
    //     a_tangential = L_boom · d²(pitch)/dt²
    //   Same impulse coupling approach:
    float pitchDeltaDeg  = craneArmPitch - prevCranePitch;
    float pitchOmega     = glm::radians(pitchDeltaDeg) / dt;   // rad/s
    float driveLongitudinal = BOOM_LENGTH * pitchOmega * driveScale;

    // ── 2. Integrate both pendulum axes ──────────────────────────────────
    integratePendulumAxis(pendulumTheta, pendulumOmega,    driveLateral,       dt, dampingCoeff);
    integratePendulumAxis(pendulumPhi,   pendulumPhiOmega, driveLongitudinal,  dt, dampingCoeff);

    // ── 3. Compute boom tip and hook world positions ─────────────────────
    //   Replicate the crane's transform chain to find the boom tip in world space.

    float turretRad = glm::radians(craneArmAngle);
    float pitchRad  = glm::radians(craneArmPitch);

    // Boom tip in turret-local space
    glm::mat4 boomMat = glm::mat4(1.0f);
    boomMat = glm::translate(boomMat, glm::vec3(0.0f, MAST_TOP_Y, 0.0f));
    boomMat = glm::rotate(boomMat, pitchRad, glm::vec3(1, 0, 0));
    glm::vec4 tipLocal = boomMat * glm::vec4(0.0f, BOOM_TIP_Y, BOOM_TIP_Z, 1.0f);

    // Apply turret Y-rotation
    glm::mat4 turretMat = glm::rotate(glm::mat4(1.0f), turretRad, glm::vec3(0, 1, 0));
    glm::vec4 tipTurret = turretMat * tipLocal;

    boomTipWorldPos = glm::vec3(
        CRANE_BASE_X + tipTurret.x,
        CRANE_BASE_Y + tipTurret.y,
        CRANE_BASE_Z + tipTurret.z
    );

    // Hook position: cable hangs from boom tip, displaced by both pendulum angles.
    //
    // In the boom's local frame (before turret rotation):
    //   θ (theta) swings laterally → displaces in boom-local X
    //   φ (phi)   swings longitudinally → displaces in boom-local Z (along boom axis)
    //   Vertical hang: Y = -L · cos(θ) · cos(φ)
    //
    // We need to rotate these local displacements by the turret angle to get world coords.

    float localSwingX =  craneCableLength * sinf(pendulumTheta);                    // lateral
    float localSwingZ =  craneCableLength * sinf(pendulumPhi);                      // longitudinal
    float localHangY  = -craneCableLength * cosf(pendulumTheta) * cosf(pendulumPhi); // downward

    // Rotate lateral (X) and longitudinal (Z) by turret angle
    float worldSwingX =  localSwingX * cosf(turretRad) + localSwingZ * sinf(turretRad);
    float worldSwingZ = -localSwingX * sinf(turretRad) + localSwingZ * cosf(turretRad);

    hookWorldPos = glm::vec3(
        boomTipWorldPos.x + worldSwingX,
        boomTipWorldPos.y + localHangY,
        boomTipWorldPos.z + worldSwingZ
    );

    // ── 4. Collision detection with cargo ship ───────────────────────────
    float dx = hookWorldPos.x - shipX;
    float dz = hookWorldPos.z - shipZ;

    if (fabsf(dx) < SHIP_HALF_X &&
        fabsf(dz) < SHIP_HALF_Z &&
        hookWorldPos.y > SHIP_Y_MIN &&
        hookWorldPos.y < SHIP_Y_MAX)
    {
        if (!triggerCollision) {
            triggerCollision    = true;
            collisionFlashTimer = FLASH_DURATION;
            // Bounce both axes
            pendulumOmega    *= COLLISION_BOUNCE;
            pendulumPhiOmega *= COLLISION_BOUNCE;
        }
    }

    // ── 5. Collision flash timer ─────────────────────────────────────────
    if (collisionFlashTimer > 0.0f) {
        collisionFlashTimer -= dt;
        if (collisionFlashTimer <= 0.0f) {
            collisionFlashTimer = 0.0f;
            triggerCollision    = false;
        }
    }
}
