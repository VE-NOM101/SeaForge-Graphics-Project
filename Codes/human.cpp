// ================================================================
// human.cpp  —  SeaForge Human System
//
// Static humanoid figure at a fixed position on the dock.
// Hierarchical transform rig, cowboy hat (Bezier SOR).
// NO movement, NO physics, NO boat riding.
// ================================================================

#include "human.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <vector>

// ── Shared VAOs from main.cpp ────────────────────────────────────
extern unsigned int cubeVAO;
extern unsigned int cylVAO;
extern int          cylIndexCount;
extern unsigned int sphVAO;
extern int          sphIndexCount;

// ── Cowboy hat — Bezier surface of revolution ────────────────────
static unsigned int hatVAO = 0, hatVBO = 0, hatEBO = 0;
static int          hatIndexCount = 0;

// Cubic Bezier evaluation
static glm::vec2 cubicBezier(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float t)
{
    float u = 1.0f - t;
    return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
}

static void initCowboyHatMesh()
{
    // Profile curve in (radius, height) space — revolved around Y axis.
    // 3 cubic Bezier segments: crown top → brim tip (curled up)
    glm::vec2 s1p0(0.00f,  0.18f);
    glm::vec2 s1p1(0.06f,  0.18f);
    glm::vec2 s1p2(0.12f,  0.16f);
    glm::vec2 s1p3(0.14f,  0.08f);

    glm::vec2 s2p0 = s1p3;
    glm::vec2 s2p1(0.16f,  0.02f);
    glm::vec2 s2p2(0.18f, -0.01f);
    glm::vec2 s2p3(0.22f, -0.02f);

    glm::vec2 s3p0 = s2p3;
    glm::vec2 s3p1(0.28f, -0.03f);
    glm::vec2 s3p2(0.34f, -0.02f);
    glm::vec2 s3p3(0.38f,  0.02f);

    const int PROFILE_PTS = 30;
    const int SLICES      = 24;

    std::vector<glm::vec2> profile;
    for (int i = 0; i <= PROFILE_PTS; i++)
    {
        float tGlobal = (float)i / PROFILE_PTS;
        float segF = tGlobal * 3.0f;
        int seg = (int)segF;
        if (seg > 2) seg = 2;
        float t = segF - seg;

        glm::vec2 pt;
        if (seg == 0)      pt = cubicBezier(s1p0, s1p1, s1p2, s1p3, t);
        else if (seg == 1)  pt = cubicBezier(s2p0, s2p1, s2p2, s2p3, t);
        else               pt = cubicBezier(s3p0, s3p1, s3p2, s3p3, t);
        profile.push_back(pt);
    }

    std::vector<float> verts;
    std::vector<unsigned int> inds;

    for (int i = 0; i <= PROFILE_PTS; i++)
    {
        float r = profile[i].x;
        float y = profile[i].y;

        glm::vec2 tang;
        if (i == 0)                   tang = profile[1] - profile[0];
        else if (i == PROFILE_PTS)    tang = profile[i] - profile[i-1];
        else                          tang = profile[i+1] - profile[i-1];
        glm::vec2 profNorm = glm::normalize(glm::vec2(tang.y, -tang.x));

        for (int j = 0; j <= SLICES; j++)
        {
            float theta = (float)j / SLICES * 2.0f * 3.14159265f;
            float cosT  = cosf(theta);
            float sinT  = sinf(theta);

            float px = r * cosT;
            float py = y;
            float pz = r * sinT;

            float nx = profNorm.x * cosT;
            float ny = profNorm.y;
            float nz = profNorm.x * sinT;

            float u = (float)j / SLICES;
            float v = (float)i / PROFILE_PTS;

            verts.insert(verts.end(), {px, py, pz, nx, ny, nz, u, v});
        }
    }

    int ring = SLICES + 1;
    for (int i = 0; i < PROFILE_PTS; i++)
    {
        for (int j = 0; j < SLICES; j++)
        {
            int c = i * ring + j;
            int n = (i + 1) * ring + j;
            inds.push_back(c);     inds.push_back(n);     inds.push_back(c + 1);
            inds.push_back(c + 1); inds.push_back(n);     inds.push_back(n + 1);
        }
    }

    hatIndexCount = (int)inds.size();

    glGenVertexArrays(1, &hatVAO);
    glGenBuffers(1, &hatVBO);
    glGenBuffers(1, &hatEBO);
    glBindVertexArray(hatVAO);

    glBindBuffer(GL_ARRAY_BUFFER, hatVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hatEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// ── Fixed human position ─────────────────────────────────────────
static const float HUMAN_X      = -8.0f;
static const float HUMAN_Y      =  0.30f;   // dock top
static const float HUMAN_Z      =  0.0f;
static const float HUMAN_FACING =  0.0f;    // facing +Z

// ── Public API ───────────────────────────────────────────────────
void initHuman()
{
    printf("[Human] Static figure placed at (%.1f, %.1f) on main dock.\n", HUMAN_X, HUMAN_Z);
}

// ================================================================
//  DRAWING — hierarchical transforms
// ================================================================

static void hMat(Shader& s, glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float sh)
{
    s.setVec3("material.ambient",    amb);
    s.setVec3("material.diffuse",    dif);
    s.setVec3("material.specular",   spc);
    s.setFloat("material.shininess", sh);
    s.setVec3("material.emissive",   glm::vec3(0.0f));
    s.setBool("useTexture", false);
}

static void hCube(Shader& s, const glm::mat4& par,
                  glm::vec3 pos, float rx, float ry, float rz,
                  float sx, float sy, float sz,
                  glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float sh)
{
    hMat(s, amb, dif, spc, sh);
    glm::mat4 M = glm::translate(par, pos);
    if (rx != 0) M = glm::rotate(M, glm::radians(rx), glm::vec3(1,0,0));
    if (ry != 0) M = glm::rotate(M, glm::radians(ry), glm::vec3(0,1,0));
    if (rz != 0) M = glm::rotate(M, glm::radians(rz), glm::vec3(0,0,1));
    M = glm::scale(M, glm::vec3(sx, sy, sz));
    s.setMat4("model", M);
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ── Colour palette ───────────────────────────────────────────────
namespace C {
    glm::vec3 skinA  = glm::vec3(0.55f, 0.34f, 0.20f);
    glm::vec3 skinD  = glm::vec3(0.80f, 0.55f, 0.35f);
    glm::vec3 skinS  = glm::vec3(0.15f, 0.10f, 0.08f);
    glm::vec3 hairA  = glm::vec3(0.06f, 0.04f, 0.02f);
    glm::vec3 hairD  = glm::vec3(0.12f, 0.08f, 0.04f);
    glm::vec3 hairS  = glm::vec3(0.04f);
    glm::vec3 vestA  = glm::vec3(0.50f, 0.20f, 0.04f);
    glm::vec3 vestD  = glm::vec3(0.85f, 0.38f, 0.08f);
    glm::vec3 vestS  = glm::vec3(0.20f);
    glm::vec3 shirtA = glm::vec3(0.30f, 0.30f, 0.32f);
    glm::vec3 shirtD = glm::vec3(0.55f, 0.55f, 0.58f);
    glm::vec3 shirtS = glm::vec3(0.10f);
    glm::vec3 pantsA = glm::vec3(0.18f, 0.19f, 0.22f);
    glm::vec3 pantsD = glm::vec3(0.35f, 0.37f, 0.42f);
    glm::vec3 pantsS = glm::vec3(0.10f);
    glm::vec3 bootA  = glm::vec3(0.35f, 0.12f, 0.04f);
    glm::vec3 bootD  = glm::vec3(0.65f, 0.22f, 0.08f);
    glm::vec3 bootS  = glm::vec3(0.25f);
    glm::vec3 soleA  = glm::vec3(0.08f, 0.07f, 0.06f);
    glm::vec3 soleD  = glm::vec3(0.15f, 0.14f, 0.12f);
    glm::vec3 soleS  = glm::vec3(0.08f);
}

// ── drawHuman ────────────────────────────────────────────────────
void drawHuman(Shader& s)
{
    s.setBool("useTexture", false);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    // ── Proportions ───────────────────────────────────────────────
    const float HIP_Y      = 0.95f;
    const float UL_H       = 0.40f;
    const float LL_H       = 0.38f;
    const float UL_W       = 0.15f;
    const float LL_W       = 0.12f;
    const float BOOT_H     = 0.16f;
    const float BOOT_W     = 0.16f;
    const float BOOT_D     = 0.23f;
    const float TORSO_H    = 0.55f;
    const float TORSO_Y    = HIP_Y + TORSO_H * 0.5f;
    const float SHOULDER_Y = HIP_Y + TORSO_H - 0.07f;
    const float UA_H       = 0.36f;
    const float FA_H       = 0.30f;
    const float UA_W       = 0.13f;
    const float FA_W       = 0.11f;
    const float NECK_Y     = HIP_Y + TORSO_H;
    const float NECK_H     = 0.10f;
    const float HEAD_H     = 0.30f;
    const float HEAD_CY    = NECK_Y + NECK_H + HEAD_H * 0.5f;

    // Static idle pose: slight natural knee bend, arms relaxed at sides
    const float lHipA  = 0.0f,  rHipA  = 0.0f;
    const float lKneeA = 5.0f,  rKneeA = 5.0f;
    const float lToeA  = 0.0f,  rToeA  = 0.0f;
    const float lArmA  = 0.0f,  rArmA  = 0.0f;
    const float lArmY  = 0.0f,  rArmY  = 0.0f;
    const float lForeA = -12.0f, rForeA = -12.0f;

    // ── Root matrix at fixed position ─────────────────────────────
    glm::mat4 root = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(HUMAN_X, HUMAN_Y, HUMAN_Z));
    root = glm::rotate(root, HUMAN_FACING, glm::vec3(0, 1, 0));

    // ────────────────────────────────────────────────────────────
    //  LEGS + BOOTS
    // ────────────────────────────────────────────────────────────
    auto drawLeg = [&](float xOff, float hipA, float kneeA, float toeA)
    {
        glm::mat4 hip = glm::translate(root, glm::vec3(xOff, HIP_Y, 0.0f));
        hip = glm::rotate(hip, glm::radians(hipA), glm::vec3(1, 0, 0));

        hCube(s, hip, glm::vec3(0, -UL_H * 0.5f, 0), 0,0,0, UL_W, UL_H, UL_W,
              C::pantsA, C::pantsD, C::pantsS, 12);

        glm::mat4 knee = glm::translate(hip, glm::vec3(0, -UL_H, 0));
        knee = glm::rotate(knee, glm::radians(kneeA), glm::vec3(1, 0, 0));

        hCube(s, knee, glm::vec3(0, -LL_H * 0.5f, 0), 0,0,0, LL_W, LL_H, LL_W,
              C::pantsA, C::pantsD, C::pantsS, 12);

        glm::mat4 ankle = glm::translate(knee, glm::vec3(0, -LL_H, 0));
        ankle = glm::rotate(ankle, glm::radians(-toeA), glm::vec3(1, 0, 0));

        hCube(s, ankle, glm::vec3(0, -BOOT_H * 0.5f, 0.03f), 0,0,0,
              BOOT_W, BOOT_H, BOOT_D, C::bootA, C::bootD, C::bootS, 24);
        hCube(s, ankle, glm::vec3(0, -BOOT_H - 0.008f, 0.03f), 0,0,0,
              BOOT_W + 0.02f, 0.025f, BOOT_D + 0.04f,
              C::soleA, C::soleD, C::soleS, 8);
    };

    drawLeg(-0.20f, lHipA, lKneeA, lToeA);
    drawLeg( 0.20f, rHipA, rKneeA, rToeA);

    // ────────────────────────────────────────────────────────────
    //  TORSO
    // ────────────────────────────────────────────────────────────
    hCube(s, root, glm::vec3(0, TORSO_Y, 0), 0,0,0, 0.50f, TORSO_H, 0.26f,
          C::shirtA, C::shirtD, C::shirtS, 16);
    hCube(s, root, glm::vec3(-0.14f, TORSO_Y, 0), 0,0,0, 0.20f, TORSO_H * 0.92f, 0.28f,
          C::vestA, C::vestD, C::vestS, 20);
    hCube(s, root, glm::vec3( 0.14f, TORSO_Y, 0), 0,0,0, 0.20f, TORSO_H * 0.92f, 0.28f,
          C::vestA, C::vestD, C::vestS, 20);
    hCube(s, root, glm::vec3(0, HIP_Y + TORSO_H - 0.04f, 0), 0,0,0, 0.52f, 0.06f, 0.28f,
          C::vestA, C::vestD, C::vestS, 16);
    hCube(s, root, glm::vec3(0, HIP_Y + 0.06f, 0), 0,0,0, 0.52f, 0.06f, 0.28f,
          C::soleA, C::soleD, C::soleS, 16);

    // ────────────────────────────────────────────────────────────
    //  ARMS
    // ────────────────────────────────────────────────────────────
    auto drawArm = [&](float xOff, float flexion, float abduction, float elbowBend)
    {
        glm::mat4 sh = glm::translate(root, glm::vec3(xOff, SHOULDER_Y, 0));
        if (abduction != 0)
            sh = glm::rotate(sh, glm::radians(abduction), glm::vec3(0, 1, 0));
        sh = glm::rotate(sh, glm::radians(flexion), glm::vec3(1, 0, 0));

        hCube(s, sh, glm::vec3(0, -UA_H * 0.5f, 0), 0,0,0, UA_W, UA_H, UA_W,
              C::shirtA, C::shirtD, C::shirtS, 16);

        glm::mat4 elbow = glm::translate(sh, glm::vec3(0, -UA_H, 0));
        elbow = glm::rotate(elbow, glm::radians(elbowBend), glm::vec3(1, 0, 0));

        hCube(s, elbow, glm::vec3(0, -FA_H * 0.5f, 0), 0,0,0, FA_W, FA_H, FA_W,
              C::skinA, C::skinD, C::skinS, 20);
        hCube(s, elbow, glm::vec3(0, -FA_H - 0.01f, 0), 0,0,0,
              FA_W + 0.02f, 0.04f, FA_W + 0.02f,
              C::shirtA, C::shirtD, C::shirtS, 12);
    };

    drawArm(-0.32f, lArmA, lArmY, lForeA);
    drawArm( 0.32f, rArmA, rArmY, rForeA);

    // ────────────────────────────────────────────────────────────
    //  NECK + HEAD + HAIR + FACE + COWBOY HAT
    // ────────────────────────────────────────────────────────────
    hCube(s, root, glm::vec3(0, NECK_Y + NECK_H * 0.5f, 0), 0,0,0, 0.12f, NECK_H, 0.12f,
          C::skinA, C::skinD, C::skinS, 20);

    hCube(s, root, glm::vec3(0, HEAD_CY, 0), 0,0,0, 0.31f, HEAD_H, 0.27f,
          C::skinA, C::skinD, C::skinS, 24);

    // Eyes
    float eyeZ = 0.135f;
    glm::vec3 eyeAmb(0.06f, 0.04f, 0.02f),  eyeDif(0.10f, 0.05f, 0.02f),  eyeSpc(0.05f);
    glm::vec3 irisAmb(0.30f, 0.10f, 0.05f), irisDif(0.50f, 0.18f, 0.08f), irisSpc(0.1f);
    hCube(s, root, glm::vec3(-0.07f, HEAD_CY + 0.04f, eyeZ),          0,0,0, 0.055f, 0.045f, 0.01f,  eyeAmb,  eyeDif,  eyeSpc,  4);
    hCube(s, root, glm::vec3( 0.07f, HEAD_CY + 0.04f, eyeZ),          0,0,0, 0.055f, 0.045f, 0.01f,  eyeAmb,  eyeDif,  eyeSpc,  4);
    hCube(s, root, glm::vec3(-0.07f, HEAD_CY + 0.04f, eyeZ + 0.002f), 0,0,0, 0.030f, 0.028f, 0.008f, irisAmb, irisDif, irisSpc, 8);
    hCube(s, root, glm::vec3( 0.07f, HEAD_CY + 0.04f, eyeZ + 0.002f), 0,0,0, 0.030f, 0.028f, 0.008f, irisAmb, irisDif, irisSpc, 8);

    // Nose
    glm::vec3 noseAmb(0.50f, 0.30f, 0.18f), noseDif(0.75f, 0.50f, 0.32f), noseSpc(0.08f);
    hCube(s, root, glm::vec3(0.0f, HEAD_CY + 0.01f, eyeZ + 0.04f), 0,0,0,
          0.04f, 0.06f, 0.05f, noseAmb, noseDif, noseSpc, 12);

    // Mouth
    glm::vec3 mouthAmb(0.25f, 0.08f, 0.06f), mouthDif(0.45f, 0.12f, 0.10f), mouthSpc(0.04f);
    hCube(s, root, glm::vec3(0.0f, HEAD_CY - 0.08f, eyeZ + 0.005f), 0,0,0,
          0.09f, 0.025f, 0.008f, mouthAmb, mouthDif, mouthSpc, 6);

    // Hair
    hCube(s, root, glm::vec3(0, HEAD_CY - 0.02f, -0.07f), 0,0,0, 0.33f, 0.27f, 0.06f,
          C::hairA, C::hairD, C::hairS, 4);
    hCube(s, root, glm::vec3(-0.16f, HEAD_CY, 0), 0,0,0, 0.04f, 0.26f, 0.22f,
          C::hairA, C::hairD, C::hairS, 4);
    hCube(s, root, glm::vec3( 0.16f, HEAD_CY, 0), 0,0,0, 0.04f, 0.26f, 0.22f,
          C::hairA, C::hairD, C::hairS, 4);

    // ── Cowboy hat (Bezier surface of revolution) ─────────────────
    if (hatVAO == 0) initCowboyHatMesh();  // lazy init

    float hatBase = HEAD_CY + HEAD_H * 0.5f + 0.06f;
    glm::mat4 hatM = glm::translate(root, glm::vec3(0.0f, hatBase, 0.0f));
    hatM = glm::scale(hatM, glm::vec3(0.95f, 1.0f, 0.95f));

    hMat(s, glm::vec3(0.55f, 0.42f, 0.25f),
            glm::vec3(0.82f, 0.65f, 0.38f),
            glm::vec3(0.30f), 32.0f);
    s.setMat4("model", hatM);
    glBindVertexArray(hatVAO);
    glDrawElements(GL_TRIANGLES, hatIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Hat band
    glm::vec3 bandAmb(0.12f, 0.06f, 0.02f), bandDif(0.22f, 0.10f, 0.04f), bandSpc(0.05f);
    hCube(s, root, glm::vec3(0.0f, hatBase + 0.06f, 0.0f), 0,0,0,
          0.155f, 0.025f, 0.155f, bandAmb, bandDif, bandSpc, 8);
}
