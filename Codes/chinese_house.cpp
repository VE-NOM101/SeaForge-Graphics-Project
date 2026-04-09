// ================================================================
// chinese_house.cpp  —  Stylized Traditional Chinese House (Pagoda)
//
// Two-story house with flat slab roof (floor 1) and tin gable roof (floor 2),
// dark wooden frame, white walls, entrance with stairs,
// on a raised multi-layer platform with trees and bushes.
// ================================================================

#include "chinese_house.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// ── Shared helpers from main.cpp ────────────────────────────────
extern void setMaterial(Shader &s, glm::vec3 amb, glm::vec3 dif, glm::vec3 spc,
                        float shin, glm::vec3 emi);
extern void bindTex(Shader &s, GLuint tex);

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

extern void drawTree(Shader &s, glm::mat4 p, float px, float py, float pz,
                     int depth, float len, float radius, int nMin, int nMax,
                     float lean, int seed);

// Textures from main.cpp
extern GLuint chineseRoofTex;   // chinese_house_roof_1.jpg
extern GLuint chineseRoof2Tex;  // chinese_house_roof_2.jpg
extern GLuint grassTex;        // grass.jpg
extern GLuint woodDeckTex;     // wood_deck.jpg (door)
extern GLuint cementTex;       // cement_tex.jpg (stairs/walkway)
extern GLuint chineseWallTex;  // chinese_house_wall.jpg (walls)

// ── No emissive ─────────────────────────────────────────────────
static const glm::vec3 NO_EMI(0.0f);

// ── Material palettes ───────────────────────────────────────────
// Roof tiles: warm reddish-orange
static const glm::vec3 ROOF_AMB(0.35f, 0.12f, 0.05f);
static const glm::vec3 ROOF_DIF(0.72f, 0.28f, 0.10f);
static const glm::vec3 ROOF_SPC(0.25f, 0.15f, 0.10f);
static const float     ROOF_SHIN = 16.0f;

// Dark wood beams/pillars
static const glm::vec3 WOOD_AMB(0.12f, 0.07f, 0.03f);
static const glm::vec3 WOOD_DIF(0.30f, 0.18f, 0.08f);
static const glm::vec3 WOOD_SPC(0.15f, 0.10f, 0.06f);
static const float     WOOD_SHIN = 24.0f;

// White/beige walls
static const glm::vec3 WALL_AMB(0.40f, 0.38f, 0.34f);
static const glm::vec3 WALL_DIF(0.90f, 0.88f, 0.82f);
static const glm::vec3 WALL_SPC(0.20f);
static const float     WALL_SHIN = 16.0f;

// Door (darker brown)
static const glm::vec3 DOOR_AMB(0.10f, 0.06f, 0.02f);
static const glm::vec3 DOOR_DIF(0.25f, 0.14f, 0.06f);

// Window glass (dark blue-gray)
static const glm::vec3 WIN_AMB(0.08f, 0.10f, 0.14f);
static const glm::vec3 WIN_DIF(0.18f, 0.22f, 0.30f);

// Platform gray
static const glm::vec3 PLAT_AMB(0.30f, 0.30f, 0.30f);
static const glm::vec3 PLAT_DIF(0.65f, 0.63f, 0.60f);

// Grass green
static const glm::vec3 GRASS_AMB(0.10f, 0.25f, 0.05f);
static const glm::vec3 GRASS_DIF(0.25f, 0.55f, 0.15f);

// Bush green
static const glm::vec3 BUSH_AMB(0.08f, 0.20f, 0.04f);
static const glm::vec3 BUSH_DIF(0.18f, 0.45f, 0.12f);

// Red-leaf tree foliage
static const glm::vec3 REDLEAF_AMB(0.30f, 0.06f, 0.04f);
static const glm::vec3 REDLEAF_DIF(0.70f, 0.15f, 0.10f);


// (Complex curved roof removed — using simple geometry instead)


// ================================================================
// drawChineseHouse — complete scene
// ================================================================
void drawChineseHouse(Shader &s, glm::mat4 p, float x, float y, float z)
{
    glm::mat4 base = glm::translate(p, glm::vec3(x, y, z));

    // ── PLATFORM / BASE ──────────────────────────────────────────
    // Bottom layer: light gray stone slab
    float platW = 14.0f, platD = 14.0f;
    drawCube(s, base, 0, -0.15f, 0, 0, 0, 0,
             platW, 0.30f, platD,
             PLAT_AMB, PLAT_DIF, glm::vec3(0.2f), 16, NO_EMI, 0);

    // Edge trim (darker border)
    glm::vec3 edgeA(0.22f), edgeD(0.35f);
    drawCube(s, base, 0, 0.02f, 0, 0, 0, 0,
             platW + 0.2f, 0.04f, platD + 0.2f,
             edgeA, edgeD, glm::vec3(0.15f), 8, NO_EMI, 0);

    // Top layer: green grass (with texture)
    drawCube(s, base, 0, 0.10f, 0, 0, 0, 0,
             platW - 0.6f, 0.10f, platD - 0.6f,
             GRASS_AMB, GRASS_DIF, glm::vec3(0.1f), 8, NO_EMI, grassTex);

    // Raised inner platform for the house (stone step)
    float innerW = 8.0f, innerD = 7.0f;
    drawCube(s, base, 0, 0.30f, -0.5f, 0, 0, 0,
             innerW, 0.20f, innerD,
             PLAT_AMB * 1.1f, PLAT_DIF * 1.05f, glm::vec3(0.2f), 16, NO_EMI, 0);

    // ── FLOOR 1 ──────────────────────────────────────────────────
    float f1Y   = 0.40f;     // base of floor 1
    float f1H   = 2.2f;      // wall height floor 1
    float f1W   = 6.0f;      // width (X)
    float f1D   = 5.0f;      // depth (Z)
    float f1CY  = f1Y + f1H * 0.5f; // center Y of floor 1 walls

    // Walls — textured panels (4 sides, with opening for door on +Z side)
    // Back wall (-Z)
    drawCube(s, base, 0, f1CY, -f1D * 0.5f, 0, 0, 0,
             f1W, f1H, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    // Left wall (-X)
    drawCube(s, base, -f1W * 0.5f, f1CY, 0, 0, 0, 0,
             0.12f, f1H, f1D,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    // Right wall (+X)
    drawCube(s, base, f1W * 0.5f, f1CY, 0, 0, 0, 0,
             0.12f, f1H, f1D,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    // Front wall (+Z) — two sections with door gap in center
    float doorW = 1.2f;
    float sideW = (f1W - doorW) * 0.5f;
    drawCube(s, base, -(doorW * 0.5f + sideW * 0.5f), f1CY, f1D * 0.5f, 0, 0, 0,
             sideW, f1H, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    drawCube(s, base,  (doorW * 0.5f + sideW * 0.5f), f1CY, f1D * 0.5f, 0, 0, 0,
             sideW, f1H, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    // Wall above door
    drawCube(s, base, 0, f1Y + f1H - 0.3f, f1D * 0.5f, 0, 0, 0,
             doorW + 0.1f, 0.6f, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);

    // ── Dark wooden frame / beams (floor 1) ──────────────────────
    float beamT = 0.10f; // beam thickness

    // Corner pillars (4 corners)
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            drawCube(s, base, sx * f1W * 0.5f, f1CY, sz * f1D * 0.5f,
                     0, 0, 0, 0.18f, f1H + 0.05f, 0.18f,
                     WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Horizontal beams top of floor 1
    float topY = f1Y + f1H;
    drawCube(s, base, 0, topY, f1D * 0.5f, 0, 0, 0,
             f1W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, topY, -f1D * 0.5f, 0, 0, 0,
             f1W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f1W * 0.5f, topY, 0, 0, 0, 0,
             beamT, beamT, f1D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f1W * 0.5f, topY, 0, 0, 0, 0,
             beamT, beamT, f1D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Bottom beams floor 1
    drawCube(s, base, 0, f1Y, f1D * 0.5f, 0, 0, 0,
             f1W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, f1Y, -f1D * 0.5f, 0, 0, 0,
             f1W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f1W * 0.5f, f1Y, 0, 0, 0, 0,
             beamT, beamT, f1D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f1W * 0.5f, f1Y, 0, 0, 0, 0,
             beamT, beamT, f1D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Mid-height horizontal beam (grid style)
    float midY = f1Y + f1H * 0.5f;
    drawCube(s, base, 0, midY, f1D * 0.5f, 0, 0, 0,
             f1W + 0.1f, beamT * 0.7f, beamT * 0.7f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, midY, -f1D * 0.5f, 0, 0, 0,
             f1W + 0.1f, beamT * 0.7f, beamT * 0.7f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f1W * 0.5f, midY, 0, 0, 0, 0,
             beamT * 0.7f, beamT * 0.7f, f1D + 0.1f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f1W * 0.5f, midY, 0, 0, 0, 0,
             beamT * 0.7f, beamT * 0.7f, f1D + 0.1f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Vertical divider beams on front/back walls
    for (int i = -1; i <= 1; i += 2)
    {
        float bx = i * f1W * 0.25f;
        drawCube(s, base, bx, f1CY, f1D * 0.5f, 0, 0, 0,
                 beamT * 0.7f, f1H, beamT * 0.7f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        drawCube(s, base, bx, f1CY, -f1D * 0.5f, 0, 0, 0,
                 beamT * 0.7f, f1H, beamT * 0.7f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    }
    // Vertical dividers on side walls
    for (int i = -1; i <= 1; i += 2)
    {
        float bz = i * f1D * 0.25f;
        drawCube(s, base, f1W * 0.5f, f1CY, bz, 0, 0, 0,
                 beamT * 0.7f, f1H, beamT * 0.7f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        drawCube(s, base, -f1W * 0.5f, f1CY, bz, 0, 0, 0,
                 beamT * 0.7f, f1H, beamT * 0.7f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    }

    // ── WINDOWS (floor 1) ────────────────────────────────────────
    // Two windows on left wall, two on right wall
    for (int sx = -1; sx <= 1; sx += 2)
        for (int i = -1; i <= 1; i += 2)
        {
            float wz = i * f1D * 0.25f;
            drawCube(s, base, sx * (f1W * 0.5f + 0.05f), f1CY + 0.2f, wz,
                     0, 0, 0, 0.04f, 0.7f, 0.5f,
                     WIN_AMB, WIN_DIF, glm::vec3(0.5f), 64, NO_EMI, 0);
            // Window grid frame (pushed further out to avoid z-fighting)
            drawCube(s, base, sx * (f1W * 0.5f + 0.08f), f1CY + 0.2f, wz,
                     0, 0, 0, 0.02f, 0.04f, 0.52f,
                     WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
            drawCube(s, base, sx * (f1W * 0.5f + 0.08f), f1CY + 0.2f, wz,
                     0, 0, 0, 0.02f, 0.72f, 0.04f,
                     WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        }
    // Two windows on back wall
    for (int i = -1; i <= 1; i += 2)
    {
        float wx = i * f1W * 0.25f;
        drawCube(s, base, wx, f1CY + 0.2f, -(f1D * 0.5f + 0.05f),
                 0, 0, 0, 0.5f, 0.7f, 0.04f,
                 WIN_AMB, WIN_DIF, glm::vec3(0.5f), 64, NO_EMI, 0);
        drawCube(s, base, wx, f1CY + 0.2f, -(f1D * 0.5f + 0.08f),
                 0, 0, 0, 0.52f, 0.04f, 0.02f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        drawCube(s, base, wx, f1CY + 0.2f, -(f1D * 0.5f + 0.08f),
                 0, 0, 0, 0.04f, 0.72f, 0.02f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    }

    // ── DOOR ─────────────────────────────────────────────────────
    float doorH = 1.8f;
    float doorZ = f1D * 0.5f + 0.12f; // pushed forward to avoid z-fighting
    drawCube(s, base, 0, f1Y + doorH * 0.5f, doorZ,
             0, 0, 0, doorW, doorH, 0.08f,
             DOOR_AMB, DOOR_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, woodDeckTex);
    // Door frame
    drawCube(s, base, 0, f1Y + doorH + 0.03f, doorZ + 0.01f,
             0, 0, 0, doorW + 0.15f, 0.06f, 0.06f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -doorW * 0.5f - 0.04f, f1Y + doorH * 0.5f, doorZ + 0.01f,
             0, 0, 0, 0.06f, doorH + 0.1f, 0.06f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base,  doorW * 0.5f + 0.04f, f1Y + doorH * 0.5f, doorZ + 0.01f,
             0, 0, 0, 0.06f, doorH + 0.1f, 0.06f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    // Center divider line on double door
    drawCube(s, base, 0, f1Y + doorH * 0.5f, doorZ + 0.05f,
             0, 0, 0, 0.03f, doorH, 0.02f,
             WOOD_AMB * 0.8f, WOOD_DIF * 0.8f, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // ── ENTRANCE STAIRS ──────────────────────────────────────────
    int nSteps = 4;
    float stepH = 0.40f / nSteps; // total rise = platform height (0.40)
    float stepD = 0.35f;
    for (int i = 0; i < nSteps; i++)
    {
        float sy = 0.15f + i * stepH + stepH * 0.5f;
        float sz = f1D * 0.5f + 0.5f + (nSteps - i) * stepD;
        drawCube(s, base, 0, sy, sz, 0, 0, 0,
                 doorW + 0.6f, stepH, stepD,
                 PLAT_AMB * 0.9f, PLAT_DIF * 0.95f, glm::vec3(0.2f), 16, NO_EMI, cementTex);
    }

    // ── MAIN ROOF (covers floor 1) — simple flat slab ──────────
    float roof1Y = topY + 0.15f;
    float roofOverhang = 0.8f;
    // Solid flat roof slab with overhang (textured)
    drawCube(s, base, 0, roof1Y, 0, 0, 0, 0,
             f1W + roofOverhang * 2.0f, 0.25f, f1D + roofOverhang * 2.0f,
             ROOF_AMB, ROOF_DIF, ROOF_SPC, ROOF_SHIN, NO_EMI, chineseRoof2Tex);
    // Thin edge trim on roof
    drawCube(s, base, 0, roof1Y - 0.10f, 0, 0, 0, 0,
             f1W + roofOverhang * 2.0f + 0.15f, 0.08f, f1D + roofOverhang * 2.0f + 0.15f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Entrance canopy (simple small flat slab above door, textured)
    drawCube(s, base, 0, f1Y + doorH + 0.25f, f1D * 0.5f + 0.5f, 0, 0, 0,
             doorW + 0.8f, 0.12f, 0.8f,
             ROOF_AMB, ROOF_DIF, ROOF_SPC, ROOF_SHIN, NO_EMI, chineseRoof2Tex);

    // ── FLOOR 2 ──────────────────────────────────────────────────
    float f2Y   = roof1Y + 0.3f;   // base of floor 2
    float f2H   = 1.6f;            // shorter second floor
    float f2W   = 4.0f;            // narrower
    float f2D   = 3.5f;
    float f2CY  = f2Y + f2H * 0.5f;

    // Floor 2 walls (all 4 sides, solid, textured)
    drawCube(s, base, 0, f2CY, -f2D * 0.5f, 0, 0, 0,
             f2W, f2H, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    drawCube(s, base, 0, f2CY, f2D * 0.5f, 0, 0, 0,
             f2W, f2H, 0.12f,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    drawCube(s, base, -f2W * 0.5f, f2CY, 0, 0, 0, 0,
             0.12f, f2H, f2D,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
    drawCube(s, base, f2W * 0.5f, f2CY, 0, 0, 0, 0,
             0.12f, f2H, f2D,
             WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);

    // Floor 2 corner pillars
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2)
            drawCube(s, base, sx * f2W * 0.5f, f2CY, sz * f2D * 0.5f,
                     0, 0, 0, 0.15f, f2H + 0.05f, 0.15f,
                     WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Floor 2 top beams
    float top2Y = f2Y + f2H;
    drawCube(s, base, 0, top2Y, f2D * 0.5f, 0, 0, 0,
             f2W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, top2Y, -f2D * 0.5f, 0, 0, 0,
             f2W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f2W * 0.5f, top2Y, 0, 0, 0, 0,
             beamT, beamT, f2D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f2W * 0.5f, top2Y, 0, 0, 0, 0,
             beamT, beamT, f2D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Floor 2 bottom beams
    drawCube(s, base, 0, f2Y, f2D * 0.5f, 0, 0, 0,
             f2W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, f2Y, -f2D * 0.5f, 0, 0, 0,
             f2W + 0.2f, beamT, beamT,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f2W * 0.5f, f2Y, 0, 0, 0, 0,
             beamT, beamT, f2D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f2W * 0.5f, f2Y, 0, 0, 0, 0,
             beamT, beamT, f2D + 0.2f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Mid-height beam floor 2
    float mid2Y = f2Y + f2H * 0.5f;
    drawCube(s, base, 0, mid2Y, f2D * 0.5f, 0, 0, 0,
             f2W + 0.1f, beamT * 0.7f, beamT * 0.7f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, mid2Y, -f2D * 0.5f, 0, 0, 0,
             f2W + 0.1f, beamT * 0.7f, beamT * 0.7f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, f2W * 0.5f, mid2Y, 0, 0, 0, 0,
             beamT * 0.7f, beamT * 0.7f, f2D + 0.1f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, -f2W * 0.5f, mid2Y, 0, 0, 0, 0,
             beamT * 0.7f, beamT * 0.7f, f2D + 0.1f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Floor 2 side windows (one per side wall)
    for (int sx = -1; sx <= 1; sx += 2)
    {
        drawCube(s, base, sx * (f2W * 0.5f + 0.07f), f2CY + 0.15f, 0,
                 0, 0, 0, 0.04f, 0.55f, 0.45f,
                 WIN_AMB, WIN_DIF, glm::vec3(0.5f), 64, NO_EMI, 0);
        drawCube(s, base, sx * (f2W * 0.5f + 0.11f), f2CY + 0.15f, 0,
                 0, 0, 0, 0.02f, 0.04f, 0.47f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        drawCube(s, base, sx * (f2W * 0.5f + 0.11f), f2CY + 0.15f, 0,
                 0, 0, 0, 0.02f, 0.57f, 0.04f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    }
    // Back window floor 2
    drawCube(s, base, 0, f2CY + 0.15f, -(f2D * 0.5f + 0.07f),
             0, 0, 0, 0.45f, 0.55f, 0.04f,
             WIN_AMB, WIN_DIF, glm::vec3(0.5f), 64, NO_EMI, 0);
    drawCube(s, base, 0, f2CY + 0.15f, -(f2D * 0.5f + 0.11f),
             0, 0, 0, 0.47f, 0.04f, 0.02f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawCube(s, base, 0, f2CY + 0.15f, -(f2D * 0.5f + 0.11f),
             0, 0, 0, 0.04f, 0.57f, 0.02f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // Front windows floor 2 — two windows side by side
    for (int i = -1; i <= 1; i += 2)
    {
        float wx = i * f2W * 0.22f;
        float wz = f2D * 0.5f + 0.07f;
        drawCube(s, base, wx, f2CY + 0.15f, wz,
                 0, 0, 0, 0.40f, 0.55f, 0.04f,
                 WIN_AMB, WIN_DIF, glm::vec3(0.5f), 64, NO_EMI, 0);
        drawCube(s, base, wx, f2CY + 0.15f, wz + 0.04f,
                 0, 0, 0, 0.42f, 0.04f, 0.02f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
        drawCube(s, base, wx, f2CY + 0.15f, wz + 0.04f,
                 0, 0, 0, 0.04f, 0.57f, 0.02f,
                 WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    }

    // ── UPPER ROOF (covers floor 2) — two-part tin shade (gable) ─
    float roof2Y = top2Y + 0.15f;
    float tinW    = f2W * 0.5f + 0.6f;  // half-width of each tin panel
    float tinD    = f2D + 1.2f;          // depth with overhang
    float tinH    = 0.08f;               // tin thickness
    float tiltAng = 34.0f;               // tilt angle in degrees (increased to cover gable ends)
    float rad     = tiltAng * 3.14159f / 180.0f;
    float ridgeY  = roof2Y + tinW * sinf(rad);

    // Set texture scale to 2x repeat for roof panels
    s.setVec2("texScale", glm::vec2(2.0f, 2.0f));

    // Left tin panel — tilts inward (up toward ridge)
    drawCube(s, base, -tinW * 0.5f * cosf(rad),
             roof2Y + tinW * 0.5f * sinf(rad), 0,
             0, 0, tiltAng,
             tinW, tinH, tinD,
             ROOF_AMB, ROOF_DIF, ROOF_SPC, ROOF_SHIN, NO_EMI, chineseRoofTex);
    // Right tin panel — tilts inward (up toward ridge)
    drawCube(s, base,  tinW * 0.5f * cosf(rad),
             roof2Y + tinW * 0.5f * sinf(rad), 0,
             0, 0, -tiltAng,
             tinW, tinH, tinD,
             ROOF_AMB, ROOF_DIF, ROOF_SPC, ROOF_SHIN, NO_EMI, chineseRoofTex);

    // Reset texture scale
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    // Ridge beam along the top where two panels meet
    drawCube(s, base, 0, ridgeY + 0.02f, 0, 0, 0, 0,
             0.12f, 0.12f, tinD + 0.1f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // ── GABLE FILLS — triangular wall fills on front and back ────
    // Fill the triangular gap between the top of floor 2 walls and the roof ridge
    // using horizontal strips that narrow toward the ridge peak
    {
        float gableBase  = f2W;                   // width at bottom
        float gableH     = ridgeY - top2Y;        // height of triangle
        int   nStrips    = 8;
        float stripH     = gableH / nStrips;
        float wallThick  = 0.12f;

        for (int i = 0; i < nStrips; i++)
        {
            float yBot  = top2Y + i * stripH;
            float yCtr  = yBot + stripH * 0.5f;
            // Width narrows linearly from gableBase at bottom to 0 at ridge
            float frac  = (float)(i + 0.5f) / nStrips;
            float w     = gableBase * (1.0f - frac);

            // Front gable (+Z)
            drawCube(s, base, 0, yCtr, f2D * 0.5f, 0, 0, 0,
                     w, stripH, wallThick,
                     WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
            // Back gable (-Z)
            drawCube(s, base, 0, yCtr, -f2D * 0.5f, 0, 0, 0,
                     w, stripH, wallThick,
                     WALL_AMB, WALL_DIF, WALL_SPC, WALL_SHIN, NO_EMI, chineseWallTex);
        }
    }

    // ── Ledge between floor 1 roof and floor 2 walls ─────────────
    drawCube(s, base, 0, f2Y - 0.05f, 0, 0, 0, 0,
             f2W + 0.4f, 0.08f, f2D + 0.4f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);

    // ── SURROUNDINGS: Trees (varied colors) ────────────────────
    // Dark green fractal tree
    drawTree(s, base, -5.5f, 0.15f, -5.5f, 3, 1.2f, 0.10f, 2, 3, 30.0f, 10);
    // Light green fractal tree
    drawTree(s, base,  5.5f, 0.15f, -4.5f, 3, 1.4f, 0.12f, 2, 3, 25.0f, 20);
    // Yellow-green fractal tree
    drawTree(s, base, -5.5f, 0.15f,  4.5f, 3, 1.0f, 0.08f, 2, 3, 35.0f, 30);
    // Medium green fractal tree
    drawTree(s, base,  5.5f, 0.15f,  5.0f, 3, 1.3f, 0.11f, 2, 3, 28.0f, 40);

    // Ornamental sphere trees with varied foliage colors
    // Light green canopy — front-right
    glm::vec3 ltGreenA(0.12f, 0.28f, 0.06f), ltGreenD(0.35f, 0.65f, 0.20f);
    drawCube(s, base, 4.0f, 0.15f + 0.6f, 5.5f, 0, 0, 0,
             0.08f, 1.2f, 0.08f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawSphere(s, base, 4.0f, 0.15f + 1.5f, 5.5f, 0, 0, 0,
               1.0f, 0.9f, 1.0f,
               ltGreenA, ltGreenD, glm::vec3(0.15f), 12, NO_EMI, 0);

    // Red-leaf canopy — back-left
    drawCube(s, base, -4.5f, 0.15f + 0.5f, -5.0f, 0, 0, 0,
             0.07f, 1.0f, 0.07f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawSphere(s, base, -4.5f, 0.15f + 1.3f, -5.0f, 0, 0, 0,
               0.85f, 0.8f, 0.85f,
               REDLEAF_AMB, REDLEAF_DIF, glm::vec3(0.15f), 12, NO_EMI, 0);

    // Yellow-orange autumn canopy — front-left
    glm::vec3 autumnA(0.30f, 0.18f, 0.04f), autumnD(0.70f, 0.45f, 0.10f);
    drawCube(s, base, -3.5f, 0.15f + 0.4f, 5.8f, 0, 0, 0,
             0.06f, 0.8f, 0.06f,
             WOOD_AMB, WOOD_DIF, WOOD_SPC, WOOD_SHIN, NO_EMI, 0);
    drawSphere(s, base, -3.5f, 0.15f + 1.1f, 5.8f, 0, 0, 0,
               0.7f, 0.65f, 0.7f,
               autumnA, autumnD, glm::vec3(0.15f), 12, NO_EMI, 0);

    // ── SURROUNDINGS: Bushes (varied greens) ─────────────────────
    float bushY = 0.15f + 0.20f;
    // Dark green bushes
    drawSphere(s, base,  2.5f, bushY, 6.0f, 0, 0, 0, 0.50f, 0.35f, 0.50f,
               BUSH_AMB, BUSH_DIF, glm::vec3(0.1f), 8, NO_EMI, 0);
    drawSphere(s, base, -2.0f, bushY, 6.0f, 0, 0, 0, 0.45f, 0.32f, 0.45f,
               BUSH_AMB, BUSH_DIF, glm::vec3(0.1f), 8, NO_EMI, 0);
    drawSphere(s, base, -5.0f, bushY, 0.0f, 0, 0, 0, 0.45f, 0.33f, 0.45f,
               BUSH_AMB, BUSH_DIF, glm::vec3(0.1f), 8, NO_EMI, 0);
    // Light green bushes
    glm::vec3 ltBushA(0.10f, 0.28f, 0.06f), ltBushD(0.25f, 0.60f, 0.18f);
    drawSphere(s, base,  1.5f, bushY, 6.2f, 0, 0, 0, 0.40f, 0.30f, 0.40f,
               ltBushA, ltBushD, glm::vec3(0.1f), 8, NO_EMI, 0);
    drawSphere(s, base,  5.0f, bushY, 0.0f, 0, 0, 0, 0.55f, 0.38f, 0.55f,
               ltBushA, ltBushD, glm::vec3(0.1f), 8, NO_EMI, 0);
    // Yellow-green bushes
    glm::vec3 ylBushA(0.15f, 0.22f, 0.04f), ylBushD(0.35f, 0.50f, 0.12f);
    drawSphere(s, base, -5.8f, bushY, 3.0f, 0, 0, 0, 0.38f, 0.28f, 0.38f,
               ylBushA, ylBushD, glm::vec3(0.1f), 8, NO_EMI, 0);
    drawSphere(s, base,  5.8f, bushY,-3.0f, 0, 0, 0, 0.42f, 0.30f, 0.42f,
               ylBushA, ylBushD, glm::vec3(0.1f), 8, NO_EMI, 0);

    // Walkway path from stairs to platform edge
    drawCube(s, base, 0, 0.16f, f1D * 0.5f + 2.5f, 0, 0, 0,
             doorW + 0.4f, 0.04f, 3.5f,
             PLAT_AMB * 0.85f, PLAT_DIF * 0.9f, glm::vec3(0.15f), 8, NO_EMI, cementTex);
}
