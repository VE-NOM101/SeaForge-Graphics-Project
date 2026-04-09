// ================================================================
// street_lamp.cpp  —  SeaForge Street Lamp
//
// Classic double-arm street lamp with cubic B-spline scrollwork,
// hanging lantern heads with semi-transparent glass, and emissive
// warm-yellow bulbs.
//
// Components:
//   1. Flared base with decorative rings and vertical fluting
//   2. Smooth cylindrical main pole
//   3. Horizontal connector bar at the top
//   4. Two mirrored cubic B-spline scrollwork arms (clamped endpoints)
//   5. Sphere joints at curve connection points
//   6. Hanging rods from the arms
//   7. Lantern heads (pyramidal cap + glass body + frame + bulb)
// ================================================================

#include "street_lamp.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ── Shared VAOs from main.cpp ────────────────────────────────────
extern unsigned int cubeVAO;
extern unsigned int cylVAO;
extern int          cylIndexCount;
extern unsigned int sphVAO;
extern int          sphIndexCount;
extern unsigned int coneVAO;
extern int          coneIndexCount;

// ── Shared helpers from main.cpp ────────────────────────────────
extern void setMaterial(Shader &s, glm::vec3 amb, glm::vec3 dif, glm::vec3 spc,
                        float shin, glm::vec3 emi);
extern void bindTex(Shader &s, GLuint tex);
extern glm::mat4 buildModel(glm::mat4 p, float px, float py, float pz,
                             float rx, float ry, float rz,
                             float sx, float sy, float sz);

// ── Primitive draw functions from main.cpp ──────────────────────
extern void drawCube(Shader &s, glm::mat4 p, float px, float py, float pz,
                     float rx, float ry, float rz, float sx, float sy, float sz,
                     glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float shin,
                     glm::vec3 emi, GLuint tex);
extern void drawCylinder(Shader &s, glm::mat4 p, float px, float py, float pz,
                         float rx, float ry, float rz, float sx, float sy, float sz,
                         glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float shin,
                         glm::vec3 emi, GLuint tex);
extern void drawSphere(Shader &s, glm::mat4 p, float px, float py, float pz,
                       float rx, float ry, float rz, float sx, float sy, float sz,
                       glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float shin,
                       glm::vec3 emi, GLuint tex);
extern void drawCone(Shader &s, glm::mat4 p, float px, float py, float pz,
                     float rx, float ry, float rz, float sx, float sy, float sz,
                     glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float shin,
                     glm::vec3 emi, GLuint tex);


// ── Iron material palette ────────────────────────────────────────
static const glm::vec3 IRON_AMB(0.08f, 0.08f, 0.10f);
static const glm::vec3 IRON_DIF(0.18f, 0.19f, 0.22f);
static const glm::vec3 IRON_SPC(0.35f, 0.35f, 0.38f);
static const float     IRON_SHIN = 48.0f;
static const glm::vec3 NO_EMI(0.0f);

// Warm yellow glow for the bulb emissive
static const glm::vec3 BULB_AMB(0.6f, 0.5f, 0.15f);
static const glm::vec3 BULB_DIF(1.0f, 0.92f, 0.55f);
static const glm::vec3 BULB_EMI(1.0f, 0.85f, 0.4f);

// Semi-transparent glass material
static const glm::vec3 GLASS_AMB(0.12f, 0.14f, 0.16f);
static const glm::vec3 GLASS_DIF(0.35f, 0.38f, 0.42f);
static const glm::vec3 GLASS_SPC(0.7f, 0.7f, 0.7f);


// ================================================================
// cubicBSplinePoint — uniform cubic B-spline evaluation
// ================================================================

glm::vec3 cubicBSplinePoint(const glm::vec3* P, int n, float t)
{
    int numSeg = n - 3;
    if (numSeg < 1) return P[0];

    float tf = t * numSeg;
    int seg = (int)tf;
    if (seg >= numSeg) seg = numSeg - 1;
    float u = tf - (float)seg;

    float u2 = u * u;
    float u3 = u2 * u;

    float b0 = (-u3 + 3.0f*u2 - 3.0f*u + 1.0f) / 6.0f;
    float b1 = ( 3.0f*u3 - 6.0f*u2 + 4.0f)      / 6.0f;
    float b2 = (-3.0f*u3 + 3.0f*u2 + 3.0f*u + 1.0f) / 6.0f;
    float b3 = u3 / 6.0f;

    return b0 * P[seg] + b1 * P[seg+1] + b2 * P[seg+2] + b3 * P[seg+3];
}


// ================================================================
// drawTubeAlongSpline — renders a tube along a cubic B-spline
// ================================================================

void drawTubeAlongSpline(Shader &s, glm::mat4 p, const glm::vec3* pts, int n,
                         float radius, int segments, glm::vec3 color)
{
    setMaterial(s, color * 0.4f, color, IRON_SPC, IRON_SHIN, NO_EMI);
    bindTex(s, 0);

    glm::vec3 prev = cubicBSplinePoint(pts, n, 0.0f);
    for (int i = 1; i <= segments; i++)
    {
        float t = (float)i / (float)segments;
        glm::vec3 cur = cubicBSplinePoint(pts, n, t);

        glm::vec3 dir = cur - prev;
        float len = glm::length(dir);
        if (len < 0.001f) { prev = cur; continue; }
        dir /= len;

        glm::mat4 model = glm::translate(p, (prev + cur) * 0.5f);

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        float d = glm::dot(up, dir);
        if (d < -0.9999f)
            model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1, 0, 0));
        else if (d < 0.9999f)
        {
            glm::vec3 axis = glm::normalize(glm::cross(up, dir));
            model = glm::rotate(model, acosf(glm::clamp(d, -1.0f, 1.0f)), axis);
        }

        // cylVAO: local radius=0.2, height=0.2
        model = glm::scale(model, glm::vec3(radius * 5.0f, len * 5.0f, radius * 5.0f));

        s.setMat4("model", model);
        glBindVertexArray(cylVAO);
        glDrawElements(GL_TRIANGLES, cylIndexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        prev = cur;
    }
}


// ================================================================
// drawLanternHead — one hanging lantern (cap + glass + frame + bulb)
// ================================================================

static void drawLanternHead(Shader &s, glm::mat4 p, float lx, float ly, float lz)
{
    // Scale values passed to drawCube/drawCone/drawSphere
    const float bodyW  = 0.28f;
    const float bodyH  = 0.32f;
    const float bodyD  = 0.28f;
    const float capH   = 0.18f;
    const float frameT = 0.025f;
    const float plateH = 0.03f;

    // Cube VAO spans -0.5..+0.5, so actual half-extents = scale * 0.5
    const float hw = bodyW * 0.5f;   // 0.14
    const float hh = bodyH * 0.5f;   // 0.16
    const float hd = bodyD * 0.5f;   // 0.14
    // Cone VAO spans -0.5..+0.5 in Y, so actual half-height = capH * 0.5
    const float hcap = capH * 0.5f;  // 0.09

    glm::mat4 lm = glm::translate(p, glm::vec3(lx, ly, lz));

    // ── Pyramidal cap (cone, apex up) — base sits on glass top ──
    float coneR = hw + 0.03f;
    drawCone(s, lm, 0, hh + hcap, 0,
             0, 0, 0,
             coneR * 2.0f, capH, coneR * 2.0f,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ── Small finial sphere on top ──
    float finialY = hh + capH + 0.04f;
    drawSphere(s, lm, 0, finialY, 0,
               0, 0, 0, 0.08f, 0.08f, 0.08f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ── Glowing glass body (emissive warm yellow, semi-transparent) ──
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    s.setFloat("alpha", 0.45f);
    drawCube(s, lm, 0, 0, 0,
             0, 0, 0,
             bodyW, bodyH, bodyD,
             BULB_AMB, BULB_DIF, glm::vec3(0.5f), 16.0f, BULB_EMI, 0);
    s.setFloat("alpha", 1.0f);
    glDisable(GL_BLEND);

    // ── Frame edges (4 vertical) — at actual cube corners ──
    for (float fx : {-hw, hw})
        for (float fz : {-hd, hd})
            drawCube(s, lm, fx, 0, fz,
                     0, 0, 0, frameT, bodyH + 0.01f, frameT,
                     IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Horizontal top rails — at actual cube top edge
    drawCube(s, lm, 0, hh, -hd, 0,0,0, hw, frameT, frameT,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm, 0, hh,  hd, 0,0,0, hw, frameT, frameT,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm, -hw, hh, 0, 0,0,0, frameT, frameT, hd,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm,  hw, hh, 0, 0,0,0, frameT, frameT, hd,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Horizontal bottom rails — at actual cube bottom edge
    drawCube(s, lm, 0, -hh, -hd, 0,0,0, hw, frameT, frameT,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm, 0, -hh,  hd, 0,0,0, hw, frameT, frameT,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm, -hw, -hh, 0, 0,0,0, frameT, frameT, hd,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawCube(s, lm,  hw, -hh, 0, 0,0,0, frameT, frameT, hd,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ── Bottom plate — just below glass body ──
    drawCube(s, lm, 0, -hh - plateH * 0.25f, 0,
             0, 0, 0, bodyW + 0.02f, plateH, bodyD + 0.02f,
             IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

}


// ================================================================
// drawStreetLamp — complete double-arm street lamp
// ================================================================

void drawStreetLamp(Shader &s, glm::mat4 p, float x, float y, float z, float rotY)
{
    glm::mat4 base = glm::translate(p, glm::vec3(x, y, z));
    if (rotY != 0.0f)
        base = glm::rotate(base, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));

    // ── Dimensions ──
    const float poleR     = 0.08f;
    const float poleTop   = 5.0f;
    const float baseH     = 0.90f;
    const float baseRbot  = 0.35f;
    const float baseRtop  = 0.12f;
    const float connLen   = 0.20f;
    const float connR     = 0.035f;
    const float rodLen    = 0.40f;
    const float rodR      = 0.015f;
    const float armR      = 0.030f;
    const float jointR    = 0.04f;

    // ──────────────────────────────────────────────────────────────
    // 1. DECORATIVE BASE
    // ──────────────────────────────────────────────────────────────
    drawCylinder(s, base, 0, 0.03f, 0,
                 0, 0, 0,
                 baseRbot * 5.0f, 0.3f, baseRbot * 5.0f,
                 IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    const int NUM_RINGS = 5;
    for (int i = 0; i < NUM_RINGS; i++)
    {
        float t = (float)i / (float)(NUM_RINGS - 1);
        float ringY = 0.06f + t * baseH;
        float ringR = baseRbot + (baseRtop - baseRbot) * t;
        float ringH = baseH / NUM_RINGS;
        drawCylinder(s, base, 0, ringY, 0,
                     0, 0, 0,
                     ringR * 5.0f, ringH * 5.0f, ringR * 5.0f,
                     IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    }

    drawCylinder(s, base, 0, baseH + 0.06f, 0,
                 0, 0, 0,
                 (baseRtop + 0.02f) * 5.0f, 0.20f, (baseRtop + 0.02f) * 5.0f,
                 IRON_AMB * 1.2f, IRON_DIF * 1.2f, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    const int FLUTES = 8;
    float fluteR = 0.015f;
    for (int i = 0; i < FLUTES; i++)
    {
        float ang = (float)i / FLUTES * 2.0f * 3.14159f;
        float fluteBaseR = (baseRbot + baseRtop) * 0.5f - 0.02f;
        float fx = cosf(ang) * fluteBaseR;
        float fz = sinf(ang) * fluteBaseR;
        drawCylinder(s, base, fx, baseH * 0.5f + 0.06f, fz,
                     0, 0, 0,
                     fluteR * 5.0f, baseH * 4.5f, fluteR * 5.0f,
                     IRON_AMB * 0.8f, IRON_DIF * 0.8f, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    }

    // ──────────────────────────────────────────────────────────────
    // 2. MAIN POLE
    // ──────────────────────────────────────────────────────────────
    float poleStartY = baseH + 0.10f;
    float poleHeight = poleTop - poleStartY;
    float poleMidY   = poleStartY + poleHeight * 0.5f;
    drawCylinder(s, base, 0, poleMidY, 0,
                 0, 0, 0,
                 poleR * 5.0f, poleHeight * 5.0f, poleR * 5.0f,
                 IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Decorative mid-section ring
    float midRingY = poleStartY + poleHeight * 0.35f;
    drawCylinder(s, base, 0, midRingY, 0,
                 0, 0, 0,
                 (poleR + 0.015f) * 5.0f, 0.25f, (poleR + 0.015f) * 5.0f,
                 IRON_AMB * 1.2f, IRON_DIF * 1.2f, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Top cap ring
    drawCylinder(s, base, 0, poleTop, 0,
                 0, 0, 0,
                 (poleR + 0.02f) * 5.0f, 0.30f, (poleR + 0.02f) * 5.0f,
                 IRON_AMB * 1.2f, IRON_DIF * 1.2f, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Sphere cap on top
    drawSphere(s, base, 0, poleTop + 0.04f, 0,
               0, 0, 0, 0.10f, 0.06f, 0.10f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ──────────────────────────────────────────────────────────────
    // 3. HORIZONTAL CONNECTOR BAR
    // ──────────────────────────────────────────────────────────────
    drawCylinder(s, base, 0, poleTop, 0,
                 0, 0, 90,
                 connR * 5.0f, connLen * 2.0f * 5.0f, connR * 5.0f,
                 IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ──────────────────────────────────────────────────────────────
    // 4. CUBIC B-SPLINE SCROLLWORK ARMS (clamped endpoints)
    // ──────────────────────────────────────────────────────────────
    // Triple first and last control points so the uniform B-spline
    // interpolates the endpoints (clamped knot behavior).
    // Shape: starts at connector end → arcs upward/outward → descends
    //        to the lamp hanging point.
    const int NUM_CP = 10;
    float armStartX = connLen;  // where connector ends
    float lampHangX = 1.05f;    // where lantern hangs (outermost X)
    float lampHangY = poleTop + 0.15f;  // final Y of arm tip

    glm::vec3 rightArm[NUM_CP] = {
        glm::vec3( armStartX,  poleTop,              0.0f),  // P0: clamped start
        glm::vec3( armStartX,  poleTop,              0.0f),  // P1: (triple)
        glm::vec3( armStartX,  poleTop,              0.0f),  // P2: (triple)
        glm::vec3( armStartX,  poleTop + 0.40f,      0.0f),  // P3: going up
        glm::vec3( 0.40f,      poleTop + 0.80f,      0.0f),  // P4: arc peak outward
        glm::vec3( 0.75f,      poleTop + 0.85f,      0.0f),  // P5: top of arc
        glm::vec3( 1.05f,      poleTop + 0.55f,      0.0f),  // P6: descending outward
        glm::vec3( lampHangX,  lampHangY,            0.0f),  // P7: clamped end
        glm::vec3( lampHangX,  lampHangY,            0.0f),  // P8: (triple)
        glm::vec3( lampHangX,  lampHangY,            0.0f),  // P9: (triple)
    };

    // LEFT arm: mirror across X
    glm::vec3 leftArm[NUM_CP];
    for (int i = 0; i < NUM_CP; i++)
    {
        leftArm[i] = rightArm[i];
        leftArm[i].x = -leftArm[i].x;
    }

    drawTubeAlongSpline(s, base, rightArm, NUM_CP, armR, 40, IRON_DIF);
    drawTubeAlongSpline(s, base, leftArm,  NUM_CP, armR, 40, IRON_DIF);

    // ── Sphere joints at connector ends ──
    drawSphere(s, base, connLen, poleTop, 0,
               0, 0, 0, jointR * 2.0f, jointR * 2.0f, jointR * 2.0f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawSphere(s, base, -connLen, poleTop, 0,
               0, 0, 0, jointR * 2.0f, jointR * 2.0f, jointR * 2.0f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Spheres at arm tips (lamp hanging points)
    drawSphere(s, base, lampHangX, lampHangY, 0,
               0, 0, 0, jointR * 2.0f, jointR * 2.0f, jointR * 2.0f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawSphere(s, base, -lampHangX, lampHangY, 0,
               0, 0, 0, jointR * 2.0f, jointR * 2.0f, jointR * 2.0f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // ──────────────────────────────────────────────────────────────
    // 5. HANGING RODS + LANTERN HEADS
    // ──────────────────────────────────────────────────────────────
    // Rods hang straight down from the arm tip
    float rodTopY = lampHangY;
    float rodBotY = rodTopY - rodLen;
    float rodMidY = (rodTopY + rodBotY) * 0.5f;

    // Right hanging rod
    drawCylinder(s, base, lampHangX, rodMidY, 0,
                 0, 0, 0,
                 rodR * 5.0f, rodLen * 5.0f, rodR * 5.0f,
                 IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    // Left hanging rod
    drawCylinder(s, base, -lampHangX, rodMidY, 0,
                 0, 0, 0,
                 rodR * 5.0f, rodLen * 5.0f, rodR * 5.0f,
                 IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Small sphere at rod-to-arm junction
    drawSphere(s, base, lampHangX, lampHangY, 0,
               0, 0, 0, 0.06f, 0.06f, 0.06f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);
    drawSphere(s, base, -lampHangX, lampHangY, 0,
               0, 0, 0, 0.06f, 0.06f, 0.06f,
               IRON_AMB, IRON_DIF, IRON_SPC, IRON_SHIN, NO_EMI, 0);

    // Lantern heads at bottom of rods
    // Glass body half-height = 0.16, cap half = 0.09, finial offset = 0.04
    float lanternCY = rodBotY - 0.38f;
    drawLanternHead(s, base, lampHangX, lanternCY, 0);
    drawLanternHead(s, base, -lampHangX, lanternCY, 0);
}
