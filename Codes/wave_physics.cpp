// ==================== B1: WAVE WATER MESH + WAVE-SHIP INTERACTION ====================
// Gerstner wave water grid mesh, CPU-side wave height sampling,
// pitch/roll/heave for cargo ship, ancient boat (with rowing), patrol ship, buoys.

#include "wave_physics.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// B6: patrol ship dynamic position (defined in autopilot.cpp)
extern float patrolX;
extern float patrolZ;
extern float patrolHeading;
#include <vector>
#include <cmath>

// ── External globals from ship_physics.cpp ──────────────────────────────────
// We need shipHeading to rotate wave sample points along the ship's local axes.
// (Avoids including ship_physics.h which would create circular dependency.)
extern float shipHeading;

// ── Wave constants (must match shader values exactly) ───────────────────────
// Wave 1: travels along +X
const WaveParams WAVE1 = { 1.2f, 0.15f, 1.5f, 0.0f, glm::vec2(1.0f, 0.0f) };
// Wave 2: travels along +Z
const WaveParams WAVE2 = { 0.8f, 0.08f, 1.1f, 1.2f, glm::vec2(0.0f, 1.0f) };

// ── Cargo ship wave-response state ──────────────────────────────────────────
float shipX        = 8.0f;
float shipPitch    = 0.0f;
float shipRoll     = 0.0f;
float shipHeave    = 0.0f;

float pitchDamp      = 2.5f;
float rollDamp       = 2.0f;
float naturalRollFreq = 1.4f;

// B4: wave-computed targets (consumed by ship_physics.cpp metacentric ODE)
float waveTargetRoll  = 0.0f;
float waveTargetHeave = 0.0f;

// ── Ancient boat state ──────────────────────────────────────────────────────
float boatX     = 15.0f;
float boatZ     = 8.0f;
float boatYaw   = glm::radians(30.0f);   // heading (radians), matches initial 30 deg
float boatPitch = 0.0f;
float boatRoll  = 0.0f;
float boatHeave = 0.0f;
float boatVelX  = 0.0f;
float boatVelZ  = 0.0f;
float boatAngVel = 0.0f;
float leftOarPhase  = 0.0f;   // 0 = idle, 0..1 = mid-stroke
float rightOarPhase = 0.0f;
bool  leftOarActive  = false;
bool  rightOarActive = false;
bool  boatReverseMode = false;
float leftOarTheta  = 0.4363f;   // catch angle ≈ 25° (radians)
float rightOarTheta = 0.4363f;
float leftOarOmega  = 0.0f;
float rightOarOmega = 0.0f;

// ── Patrol ship wave-response state ─────────────────────────────────────────
float patrolPitch = 0.0f;
float patrolRoll  = 0.0f;
float patrolHeave = 0.0f;

// ── Buoy wave-response state ────────────────────────────────────────────────
static const int NUM_BUOYS = 7;
static const float buoyPos[7][2] = {
    {18,6}, {26,-3}, {29,-12}, {23,-21}, {14,9}, {5,5}, {30,-5}
};
float buoyHeave[7] = {};
float buoyTiltX[7] = {};
float buoyTiltZ[7] = {};

// ── Water mesh GPU handles ──────────────────────────────────────────────────
unsigned int waterVAO = 0, waterVBO = 0, waterEBO = 0;
int waterIndexCount = 0;

// ═══════════════════════════════════════════════════════════════════════════
// createWaterMesh
// ═══════════════════════════════════════════════════════════════════════════
void createWaterMesh(int gridX, int gridZ, float worldW, float worldD)
{
    int vertsX = gridX + 1;
    int vertsZ = gridZ + 1;
    int vertCount = vertsX * vertsZ;

    std::vector<float> verts;
    verts.reserve(vertCount * 8);

    float halfW = worldW * 0.5f;
    float halfD = worldD * 0.5f;

    for (int iz = 0; iz < vertsZ; iz++)
    {
        float tz = (float)iz / gridZ;
        float wz = -halfD + tz * worldD;
        for (int ix = 0; ix < vertsX; ix++)
        {
            float tx = (float)ix / gridX;
            float wx = -halfW + tx * worldW;

            verts.push_back(wx);
            verts.push_back(-0.35f);
            verts.push_back(wz);
            verts.push_back(0.0f);
            verts.push_back(1.0f);
            verts.push_back(0.0f);
            verts.push_back(tx);
            verts.push_back(tz);
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(gridX * gridZ * 6);

    for (int iz = 0; iz < gridZ; iz++)
    {
        for (int ix = 0; ix < gridX; ix++)
        {
            int topLeft  = iz * vertsX + ix;
            int topRight = topLeft + 1;
            int botLeft  = topLeft + vertsX;
            int botRight = botLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(botLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(botLeft);
            indices.push_back(botRight);
        }
    }

    waterIndexCount = (int)indices.size();

    glGenVertexArrays(1, &waterVAO);
    glGenBuffers(1, &waterVBO);
    glGenBuffers(1, &waterEBO);

    glBindVertexArray(waterVAO);

    glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// ═══════════════════════════════════════════════════════════════════════════
// waveAt — CPU-side Gerstner wave height query (matches shader math exactly).
// ═══════════════════════════════════════════════════════════════════════════
float waveAt(float wx, float wz, float t)
{
    float phase1 = WAVE1.k * (WAVE1.dir.x * wx + WAVE1.dir.y * wz) - WAVE1.w * t + WAVE1.phi;
    float y1 = WAVE1.A * sinf(phase1);

    float phase2 = WAVE2.k * (WAVE2.dir.x * wx + WAVE2.dir.y * wz) - WAVE2.w * t + WAVE2.phi;
    float y2 = WAVE2.A * sinf(phase2);

    return y1 + y2;
}

// ═══════════════════════════════════════════════════════════════════════════
// Generic 4-point hull wave sampling → pitch, roll, heave
// yawRad: heading angle so we sample along the vessel's local axes
// ═══════════════════════════════════════════════════════════════════════════
static void computeHullWaveResponse(
    float posX, float posZ, float yawRad,
    float halfLen, float halfWid,
    float dampP, float dampR, float dampH,
    float dt, float time,
    float &pitch, float &roll, float &heave)
{
    // Vessel forward axis in world space
    float fwdX = -sinf(yawRad), fwdZ = -cosf(yawRad);
    // Vessel right axis (starboard)
    float rgtX =  cosf(yawRad), rgtZ = -sinf(yawRad);

    float hBow   = waveAt(posX + fwdX * halfLen, posZ + fwdZ * halfLen, time);
    float hStern = waveAt(posX - fwdX * halfLen, posZ - fwdZ * halfLen, time);
    float hPort  = waveAt(posX - rgtX * halfWid, posZ - rgtZ * halfWid, time);
    float hStarb = waveAt(posX + rgtX * halfWid, posZ + rgtZ * halfWid, time);
    float hCenter = waveAt(posX, posZ, time);

    float targetPitch = atan2f(hBow - hStern, halfLen * 2.0f) * 0.4f;
    float targetRoll  = atan2f(hPort - hStarb, halfWid * 2.0f) * 0.4f;
    float targetHeave = hCenter * 0.6f;

    pitch += (targetPitch - pitch) * dampP * dt;
    roll  += (targetRoll  - roll)  * dampR * dt;
    heave += (targetHeave - heave) * dampH * dt;
}

// ═══════════════════════════════════════════════════════════════════════════
// updateWavePhysics — cargo ship wave sampling (heading-aware)
// ═══════════════════════════════════════════════════════════════════════════
// IMPORTANT: Sample bow/stern/port/starboard points ROTATED by shipHeading,
// so pitch and roll are computed in ship-local coordinates.  Without this,
// a turned ship samples the wrong wave slope → pitch diverges ("spin bug").
void updateWavePhysics(float shipZ, float dt, float time)
{
    const float halfLen = 9.0f;
    const float halfWid = 2.5f;

    // Ship forward axis in world space (heading 0 = +Z)
    float fwdX =  sinf(shipHeading);
    float fwdZ =  cosf(shipHeading);
    // Ship right axis (starboard) — 90° CW from forward
    float rgtX =  cosf(shipHeading);
    float rgtZ = -sinf(shipHeading);

    // Sample wave height at heading-rotated hull extremes
    float hBow   = waveAt(shipX + fwdX * halfLen, shipZ + fwdZ * halfLen, time);
    float hStern = waveAt(shipX - fwdX * halfLen, shipZ - fwdZ * halfLen, time);
    float hPort  = waveAt(shipX - rgtX * halfWid, shipZ - rgtZ * halfWid, time);
    float hStarb = waveAt(shipX + rgtX * halfWid, shipZ + rgtZ * halfWid, time);
    float hCenter = waveAt(shipX, shipZ, time);

    // Target pitch/roll from wave slope (scaled × 0.4 for gentle response)
    float targetPitch = atan2f(hBow - hStern, halfLen * 2.0f) * 0.4f;
    float targetRoll  = atan2f(hPort - hStarb, halfWid * 2.0f) * 0.4f;

    // Roll resonance amplification near natural frequency
    float resonanceFactor = 1.0f / (1.0f + fabsf(WAVE1.w - naturalRollFreq));
    targetRoll *= (1.0f + 0.8f * resonanceFactor);

    // Smooth pitch toward target (exponential ease)
    shipPitch += (targetPitch - shipPitch) * pitchDamp * dt;

    // B4: expose wave roll target for metacentric ODE (ship_physics.cpp drives shipRoll)
    waveTargetRoll = targetRoll;

    // B4: smooth wave heave target (ship_physics.cpp adds draft offset to get shipHeave)
    float targetHeaveRaw = hCenter * 0.6f;
    waveTargetHeave += (targetHeaveRaw - waveTargetHeave) * 3.0f * dt;
}

// ═══════════════════════════════════════════════════════════════════════════
// Oar Physics — Lever + Fluid Drag Model (physically accurate)
// ═══════════════════════════════════════════════════════════════════════════
//
// The oar is a rigid lever pivoting at the oarlock (on the gunwale).
//   Inboard:  handle end, length L_IN from pivot — rower applies force here
//   Outboard: blade end,  length L_OUT from pivot — water drag acts here
//
// Physics chain (per oar, per frame):
//   1. Rower pulls handle  →  torque τ_hand about oarlock
//   2. Blade sweeps through water  →  drag F_d = ½ρCdA|v|v on blade
//   3. Drag creates opposing torque  →  τ_drag = F_d × L_OUT
//   4. Net torque  →  angular acceleration  α = τ_net / I_oar
//   5. Blade drag reaction at oarlock  →  propels boat (Newton's 3rd law)
//
// The propulsive force on the boat = blade drag reaction, decomposed:
//   Forward component:  F_fwd = F_drag × cos(θ)   (along boat heading)
//   Lateral component:  creates yaw torque for steering
//
// Stroke phases (phase 0→1, controls blade dip animation):
//   Catch    [0.00–0.12]: blade enters water, drag ramps up
//   Drive    [0.12–0.60]: full pull, maximum thrust
//   Finish   [0.60–0.75]: blade exits water, forces diminish
//   Recovery [0.75–1.00]: blade airborne, oar returns to catch angle

// ── Oar lever geometry ──────────────────────────────────────────────────────
static const float L_IN          = 0.75f;    // handle to oarlock (inboard), units
static const float L_OUT         = 1.75f;    // oarlock to blade center (outboard)

// ── Oar angular dynamics ────────────────────────────────────────────────────
// Moment of inertia about oarlock (off-center pivot of a rod):
//   I = m × (L_in² + L_in×L_out + L_out²) / 3
static const float OAR_MASS      = 2.5f;
static const float OAR_INERTIA   = OAR_MASS * (L_IN*L_IN + L_IN*L_OUT + L_OUT*L_OUT) / 3.0f;
static const float HAND_TORQUE   = 100.0f;   // rower hand torque magnitude (N·m, game units)
static const float BLADE_DRAG_K  = 0.28f;    // ½ρCdA — blade hydrodynamic drag coefficient
static const float OAR_FRICTION  = 0.5f;     // oarlock friction damping (N·m·s/rad)

// ── Oar angle limits ────────────────────────────────────────────────────────
static const float OAR_CATCH_ANGLE  = glm::radians(25.0f);   // start-of-stroke angle (rad)
static const float OAR_FINISH_ANGLE = glm::radians(-25.0f);  // end-of-stroke angle (rad)

// ── Recovery (blade out of water) ───────────────────────────────────────────
static const float RECOVERY_RATE = 10.0f;    // exponential return speed to catch angle
static const float RECOVERY_DAMP = 0.05f;    // angular velocity damping factor per frame

// ── Boat dynamics ───────────────────────────────────────────────────────────
static const float BOAT_MASS     = 1.8f;     // boat + rower effective mass
static const float BOAT_DRAG     = 0.7f;     // linear hull drag coefficient
static const float BOAT_ANG_DRAG = 3.5f;     // yaw angular drag coefficient
static const float OARLOCK_X     = 0.65f;    // oarlock lateral offset from centerline

// ── Stroke timing ───────────────────────────────────────────────────────────
static const float OAR_STROKE_RATE = 2.5f;   // full cycles per second

// Blade immersion factor from stroke phase (0=dry, 1=fully submerged)
static float bladeImmersion(float phase)
{
    if (phase <= 0.0f)  return 0.0f;
    if (phase <  0.12f) return phase / 0.12f;                           // catch: entering water
    if (phase <= 0.60f) return 1.0f;                                    // drive: fully submerged
    if (phase <= 0.75f) return 1.0f - (phase - 0.60f) / 0.15f;        // finish: exiting water
    return 0.0f;                                                        // recovery: airborne
}

// ═══════════════════════════════════════════════════════════════════════════
// updateBoatAndOthers — rowing input + wave physics for all non-cargo vessels
// ═══════════════════════════════════════════════════════════════════════════
void updateBoatAndOthers(GLFWwindow* window, float dt, float time)
{
    // ── 1. Rowing input ─────────────────────────────────────────────────────
    // T = left (port) oar, G = right (starboard) oar
    // Ctrl+T / Ctrl+G = reverse rowing (boat moves backward)
    bool ctrlHeld = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    boatReverseMode = ctrlHeld;

    bool leftKey  = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    bool rightKey = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;

    // Start new stroke when key pressed and oar idle
    if (leftKey  && leftOarPhase  <= 0.0f) leftOarActive  = true;
    if (rightKey && rightOarPhase <= 0.0f) rightOarActive = true;

    // Advance stroke phase (controls dip animation + stroke timing)
    if (leftOarActive) {
        leftOarPhase += OAR_STROKE_RATE * dt;
        if (leftOarPhase >= 1.0f) {
            leftOarPhase = 0.0f;
            leftOarActive = leftKey;  // continue if key still held
        }
    }
    if (rightOarActive) {
        rightOarPhase += OAR_STROKE_RATE * dt;
        if (rightOarPhase >= 1.0f) {
            rightOarPhase = 0.0f;
            rightOarActive = rightKey;
        }
    }

    // ── 2. Per-oar lever + drag physics ─────────────────────────────────────
    // Boat forward direction (bow is -Z in local space)
    float fwdX = -sinf(boatYaw);
    float fwdZ = -cosf(boatYaw);

    float totalFwdForce  = 0.0f;
    float totalYawTorque = 0.0f;

    for (int oarIdx = 0; oarIdx < 2; oarIdx++)
    {
        float* theta  = (oarIdx == 0) ? &leftOarTheta  : &rightOarTheta;
        float* omega  = (oarIdx == 0) ? &leftOarOmega  : &rightOarOmega;
        float  phase  = (oarIdx == 0) ? leftOarPhase    : rightOarPhase;
        bool   active = (oarIdx == 0) ? leftOarActive   : rightOarActive;
        float  sideSign = (oarIdx == 0) ? -1.0f : 1.0f;  // -1=port, +1=starboard

        float immersion = bladeImmersion(phase);

        if (immersion > 0.01f && active) {
            // ── BLADE IN WATER: lever dynamics + hydrodynamic drag ──────

            // Hand torque (rower pulls handle → drives oar in -θ direction)
            // Negative τ = sweep from catch (+25°) toward finish (-25°)
            float tauHand = -HAND_TORQUE;
            if (boatReverseMode) tauHand = HAND_TORQUE;  // reverse: +θ sweep
            tauHand *= immersion;  // ramps during catch/finish transitions

            // Blade tangential velocity: v_blade = ω × L_OUT
            float vBlade = (*omega) * L_OUT;

            // Water drag on blade: F_d = -½ρCdA × |v| × v (opposes motion)
            // Scaled by immersion (partial drag during catch/finish)
            float Fdrag = -BLADE_DRAG_K * fabsf(vBlade) * vBlade * immersion;

            // Drag torque on oar about oarlock: τ_drag = F_drag × L_OUT
            float tauDrag = Fdrag * L_OUT;

            // Net torque: hand + drag + friction
            float tauNet = tauHand + tauDrag - OAR_FRICTION * (*omega);

            // Angular acceleration: α = τ / I
            float alpha = tauNet / OAR_INERTIA;
            *omega += alpha * dt;
            *theta += (*omega) * dt;

            // ── Force on boat (transmitted through oarlock pivot) ──
            // Blade drag opposes blade motion → propels boat via rigid lever.
            // Forward component of drag: F_drag × cos(θ)
            float Fforward  = Fdrag * cosf(*theta);

            totalFwdForce += Fforward;

            // Yaw torque from off-center force application:
            //   Port oar → turns starboard (yaw decreases)
            //   Starboard oar → turns port (yaw increases)
            //   Both equal → torques cancel → straight line
            totalYawTorque += sideSign * Fforward * OARLOCK_X;

        } else {
            // ── BLADE OUT OF WATER: recovery return to catch angle ───────
            float targetAngle = OAR_CATCH_ANGLE;
            if (boatReverseMode) targetAngle = -OAR_CATCH_ANGLE;

            // Exponential blend back to catch position
            *theta += (targetAngle - *theta) * RECOVERY_RATE * dt;
            *omega *= RECOVERY_DAMP;  // kill residual angular velocity
        }

        // Clamp oar angle and zero velocity at limits
        float thetaMin = OAR_FINISH_ANGLE - glm::radians(5.0f);
        float thetaMax = OAR_CATCH_ANGLE  + glm::radians(5.0f);
        if (*theta <= thetaMin) { *theta = thetaMin; if (*omega < 0.0f) *omega = 0.0f; }
        if (*theta >= thetaMax) { *theta = thetaMax; if (*omega > 0.0f) *omega = 0.0f; }
    }

    // ── 3. Apply accumulated forces to boat ─────────────────────────────────
    boatVelX   += fwdX * totalFwdForce / BOAT_MASS * dt;
    boatVelZ   += fwdZ * totalFwdForce / BOAT_MASS * dt;
    boatAngVel += totalYawTorque / BOAT_MASS * dt;

    // Hull drag (linear velocity damping)
    float dragFactor = 1.0f - BOAT_DRAG * dt;
    if (dragFactor < 0.0f) dragFactor = 0.0f;
    boatVelX   *= dragFactor;
    boatVelZ   *= dragFactor;
    boatAngVel *= (1.0f - BOAT_ANG_DRAG * dt);

    // Integrate position and heading
    boatX   += boatVelX * dt;
    boatZ   += boatVelZ * dt;
    boatYaw += boatAngVel * dt;

    // ── 2. Ancient boat wave response ───────────────────────────────────────
    // Half-extents: hull half-length ~3.0, half-width ~0.7
    computeHullWaveResponse(boatX, boatZ, boatYaw,
                            3.0f, 0.7f,
                            3.0f, 2.5f, 4.0f,
                            dt, time,
                            boatPitch, boatRoll, boatHeave);

    // ── 3. Patrol ship wave response ────────────────────────────────────────
    // B6: use dynamic patrol position/heading from autopilot globals
    computeHullWaveResponse(patrolX, patrolZ, patrolHeading,
                            7.0f, 3.5f,
                            2.0f, 1.8f, 3.0f,
                            dt, time,
                            patrolPitch, patrolRoll, patrolHeave);

    // ── 4. Buoys — simple single-point wave sampling with tilt ──────────────
    for (int i = 0; i < NUM_BUOYS; i++) {
        float bx = buoyPos[i][0], bz = buoyPos[i][1];
        float eps = 0.3f;  // sampling offset for tilt

        float hC = waveAt(bx, bz, time);
        float hR = waveAt(bx + eps, bz, time);
        float hF = waveAt(bx, bz + eps, time);

        // Direct heave (buoys are light, follow water surface closely)
        buoyHeave[i] += (hC * 0.9f - buoyHeave[i]) * 5.0f * dt;

        // Tilt from wave slope
        float targetTiltX = atan2f(hF - hC, eps) * 0.3f;
        float targetTiltZ = atan2f(hR - hC, eps) * 0.3f;
        buoyTiltX[i] += (targetTiltX - buoyTiltX[i]) * 4.0f * dt;
        buoyTiltZ[i] += (targetTiltZ - buoyTiltZ[i]) * 4.0f * dt;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// drawWaterMesh
// ═══════════════════════════════════════════════════════════════════════════
void drawWaterMesh(Shader &s, glm::mat4 parentTransform, float time)
{
    s.use();

    s.setVec3("material.ambient",  glm::vec3(0.04f, 0.12f, 0.3f));
    s.setVec3("material.diffuse",  glm::vec3(0.08f, 0.25f, 0.5f));
    s.setVec3("material.specular", glm::vec3(0.3f, 0.4f, 0.55f));
    s.setFloat("material.shininess", 64.0f);
    s.setVec3("material.emissive", glm::vec3(0.0f));
    s.setBool("useTexture", false);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    s.setInt("useWave", 1);
    s.setFloat("time", time);

    glm::mat4 model = parentTransform;
    s.setMat4("model", model);

    glBindVertexArray(waterVAO);
    glDrawElements(GL_TRIANGLES, waterIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    s.setInt("useWave", 0);
}
