#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION        // stb_image: single-header image loader for textures
#include <glad/glad.h>                  // OpenGL function loader
#include <GLFW/glfw3.h>                 // Window creation & input handling
#include <glm/glm.hpp>                  // GLM math: vectors, matrices
#include <glm/gtc/matrix_transform.hpp> // GLM: perspective, translate, rotate, scale
#include <glm/gtc/type_ptr.hpp>         // GLM: value_ptr for passing to OpenGL
#include <glm/gtx/string_cast.hpp>      // GLM: to_string for debug printing
#include "shader.h"                     // Shader class: compile, link, set uniforms
#include "basic_camera.h"               // BasicCamera class: WASD movement, mouse look
#include "stb_image.h"                  // stb_image: loads JPG/PNG into raw pixel data
#include "wave_physics.h"               // B1: Gerstner wave water mesh + ship interaction
#include "crane_physics.h"              // B2: Crane damped pendulum + swing collision
#include "ship_physics.h"               // B3: Ship drag/inertia + turning; B4: Buoyancy + stability
#include "autopilot.h"                  // B6: Ship autopilot (cubic B-spline + PID)
#include "street_lamp.h"                // Street lamp: B-spline scrollwork + lantern heads
#include "chinese_house.h"              // Chinese house: pagoda with B-spline curved roofs
#include "arch_bridge.h"                // Arch bridge: cubic B-spline + ruled surface deck
#include "human.h"                      // Human: controllable figure with AABB collision + water death

// -- Standard Library --
#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
using namespace std;

// ==================== FORWARD DECLARATIONS ====================
// GLFW callbacks (window resize, scroll, mouse movement)
void framebuffer_size_callback(GLFWwindow *w, int width, int height);
void scroll_callback(GLFWwindow *w, double xo, double yo);
void mouse_callback(GLFWwindow *w, double xIn, double yIn);
void processInput(GLFWwindow *w); // Keyboard input handler
void printControls();             // Print controls to console

// Primitive creation (generates VAO/VBO/EBO) and drawing functions
// Each draw function takes: shader, parent matrix, position(px,py,pz), rotation(rx,ry,rz),
//   scale(sx,sy,sz), material colors(ambient,diffuse,specular), shininess, emissive, optional texture
void generateAncientBoatMesh();   // A2: builds Bezier lofted hull VAO (called before main loop)
void generateOarMesh();           // A2: builds B-spline extruded oar VAO
void createCube();
void drawCube(Shader &s, glm::mat4 p, float px = 0, float py = 0, float pz = 0, float rx = 0, float ry = 0, float rz = 0, float sx = 1, float sy = 1, float sz = 1, glm::vec3 amb = glm::vec3(0.5f), glm::vec3 dif = glm::vec3(0.5f), glm::vec3 spc = glm::vec3(0.5f), float shin = 32.0f, glm::vec3 emi = glm::vec3(0.0f), GLuint tex = 0);
void createCylinder(float r, float h, int sec, int stk);
void drawCylinder(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 amb = glm::vec3(0.5f), glm::vec3 dif = glm::vec3(0.5f), glm::vec3 spc = glm::vec3(0.5f), float shin = 32.0f, glm::vec3 emi = glm::vec3(0.0f), GLuint tex = 0);
void createSphere(float radius, int sectors, int stacks);
void drawSphere(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 amb = glm::vec3(0.5f), glm::vec3 dif = glm::vec3(0.5f), glm::vec3 spc = glm::vec3(0.5f), float shin = 32.0f, glm::vec3 emi = glm::vec3(0.0f), GLuint tex = 0);
void createCone(float radius, float height, int sectors);
void drawCone(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 amb = glm::vec3(0.5f), glm::vec3 dif = glm::vec3(0.5f), glm::vec3 spc = glm::vec3(0.5f), float shin = 32.0f, glm::vec3 emi = glm::vec3(0.0f), GLuint tex = 0);
void createWedge(); // Ship bow: pyramid with rectangular base + apex point
void drawWedge(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx = 1, float sy = 1, float sz = 1, glm::vec3 amb = glm::vec3(0.5f), glm::vec3 dif = glm::vec3(0.5f), glm::vec3 spc = glm::vec3(0.5f), float shin = 32.0f, glm::vec3 emi = glm::vec3(0.0f), GLuint tex = 0);

// Scene component drawing functions (each builds a complex object from primitives)
void drawWater(Shader &s, glm::mat4 p);           // Semi-transparent ocean plane
void drawDock(Shader &s, glm::mat4 p);            // Concrete platform with bollards and road markings
void drawSeaPortBuilding(Shader &s, glm::mat4 p); // 2-story building with control tower and antenna
void drawContainerYard(Shader &s, glm::mat4 p);   // 3x4 grid of stacked shipping containers (textured)
void drawMobileCrane(Shader &s, glm::mat4 p);     // Tracked mobile crane with rotating arm
void drawCargoShip(Shader &s, glm::mat4 p);       // Cargo ship: hull, wedge bow, bridge, containers
void drawLighthouse(Shader &s, glm::mat4 p);      // Red/white striped lighthouse with disco ball light
void drawBuoys(Shader &s, glm::mat4 p);           // 7 navigation buoys in the water
void drawMooringRopes(Shader &s, glm::mat4 p);    // Catmull-Rom spline ropes: bollards → ship cleats

// Curve helpers (A1/A2, reused by B2 spline cable)
glm::vec3 catmullRomPoint(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t);
void drawRopeSegment(Shader &s, glm::mat4 p, glm::vec3 a, glm::vec3 b, float thickness);
void drawAncientBoat(Shader &s, glm::mat4 p);     // A2: Ancient boat — Bezier lofted hull
void drawFractalBranch(Shader &s, glm::mat4 mat, int depth, float len, float radius, int nMin, int nMax, float lean); // A3
void drawTree(Shader& s, glm::mat4 p, float px, float py, float pz, int depth, float len, float radius, int nMin, int nMax, float lean, int seed); // A3
void generatePatrolShipHull(); // A4: builds ruled-surface upper hull VAO
void drawPatrolShip(Shader &s, glm::mat4 p); // A4: harbor patrol/rescue vessel
void drawPatrolShipPath(Shader &s, glm::mat4 p); // B6: visualize autopilot B-spline path
void drawHuman(Shader &s);                       // Human figure (human.cpp)

// ==================== GLOBAL VARIABLES ====================

// Window dimensions
unsigned int SCR_WIDTH = 1200, SCR_HEIGHT = 800;

// Mouse tracking for first-person camera
bool firstMouse = true;
float lastX = 600.0f, lastY = 400.0f;

// Camera: starts at position (25,18,25) looking toward origin
glm::vec3 V = glm::vec3(0.0f, 1.0f, 0.0f); // World up vector
BasicCamera basic_camera(25.0f, 18.0f, 25.0f, 0.0f, 0.0f, 0.0f, V);
float deltaTime = 0.0f, lastFrame = 0.0f; // Frame timing for smooth movement

unsigned int cubeVAO = 0, cubeVBO = 0, cubeEBO = 0;
unsigned int cylVAO = 0, cylVBO = 0, cylEBO = 0;
int cylIndexCount = 0;
unsigned int sphVAO = 0, sphVBO = 0, sphEBO = 0;
int sphIndexCount = 0;
unsigned int coneVAO = 0, coneVBO = 0, coneEBO = 0;
int coneIndexCount = 0;
unsigned int wedgeVAO = 0, wedgeVBO = 0, wedgeEBO = 0;
int wedgeIndexCount = 0;

// Interactive state variables
bool isPerspective = true;    // Toggle between perspective and orthographic projection
float shipZ = 0.0f;           // Cargo ship z-position (I/K keys to move)
float craneArmAngle = 0.0f;   // Mobile crane arm rotation (J/L keys)
float craneArmPitch = -20.0f; // Mobile crane arm pitch angle (O/P keys)
float prevCraneAngle = 0.0f;  // B2: previous frame turret angle for pendulum driving
float prevCranePitch = -20.0f; // B2: previous frame boom pitch for longitudinal pendulum

// Lighting toggle flags (keys 1/2/3 and 5/6/7)
bool dirLightOn = true, pointLightOn = true, spotLightOn = false;
bool ambientOn = true, diffuseOn = true, specularOn = true;

// Render mode: 0=material only (M), 1=texture only (N), 2=blended (8)
int renderMode = 0;
bool useGouraud = false; // false=Phong (default), true=Gouraud (key 4 to toggle)

GLuint containerTex[5] = {0}, brickTex = 0, woodDeckTex = 0;
GLuint bouyTex = 0, concreteRoofTex = 0, discoBallTex = 0;
GLuint coneTex = 0;              // Lighthouse cone roof texture
GLuint mooringBollardTex = 0;    // Mooring bollard post texture (horizontal strips)
GLuint mooringBollardTopTex = 0; // Mooring bollard flat top texture
GLuint shipBodyTex = 0;          // Ship hull body texture

// A2: Ancient boat (Bezier lofted hull)
unsigned int ancientBoatVAO = 0, ancientBoatVBO = 0, ancientBoatEBO = 0;
int ancientBoatIndexCount = 0;
GLuint ancientBoatTex = 0; // "ancient_boat_wood.jpg" — optional, 0 = material color only
// A2: Oar (B-spline extruded mesh)
unsigned int oarVAO = 0, oarVBO = 0, oarEBO = 0;
int oarIndexCount = 0;

// A3: Fractal tree textures (optional — 0 = material color fallback)
GLuint treeBarkTex = 0;        // "tree_bark.jpg"
GLuint treeLeavesTotalTex = 0; // "tree_leaves.jpg"

// A4: Harbor patrol/rescue ship — ruled-surface hull (upper orange panels)
unsigned int patrolHullVAO = 0, patrolHullVBO = 0, patrolHullEBO = 0;
int patrolHullIndexCount = 0;
GLuint patrolBodyTex = 0;  // Patrol_ship_body.jpg — hull surface
GLuint patrolWallTex = 0;  // Patrol_ship_wall.jpg — bridge + aft house walls
GLuint patrolGlassTex = 0; // Patrol_ship_glass.jpg — windows / portholes
GLuint patrolRoofTex = 0;  // Patrol_ship_roof.jpg  — bridge + aft house roofs
GLuint chineseRoofTex = 0;  // chinese_house_roof_1.jpg — tin shade roof
GLuint chineseRoof2Tex = 0; // chinese_house_roof_2.jpg — flat slab roof
GLuint grassTex = 0;       // grass.jpg — grass ground texture
GLuint cementTex = 0;      // cement_tex.jpg — stairs/walkway
GLuint chineseWallTex = 0; // chinese_house_wall.jpg — house walls
GLuint bridgeSurfTex  = 0;
GLuint metalTex       = 0;

const float PI = 3.14159265f;

// B6: Patrol ship autopilot instance
Autopilot patrolAutopilot;

// ── B7: Crane Loading Operation ──────────────────────────────────────────────
// Two-part one-shot simulation (runs only once total):
//
//   PART 1 — Press T:  Crane picks container from container yard.
//     IDLE → GOTO_PICKUP  (turret rotates to yard, boom aims down)
//          → LOWER_HOOK   (0.5s settle pause)
//          → ATTACH       (container vanishes from yard, appears on hook)
//          → LIFT         (boom raises, container hangs from cable)
//          → WAIT_DELIVERY (crane holds, container swings — awaits Part 2)
//
//   PART 2 — Press U:  Crane swings to ship and places container in slot 0.
//     WAIT_DELIVERY → GOTO_SHIP  (turret rotates to ship port-outer end)
//                  → LOWER_SLOT  (boom lowers onto deck)
//                  → DETACH      (container placed in slot 0, disappears from hook)
//                  → RETURN      (crane back to default pose)
//                  → COMPLETE    (flag set, cannot re-run)
//
// Target geometry:
//   Crane base:   (-3, ?, -5) world
//   Pickup:       yard row 0 col 2 stack 1 → world (-10.4, 2.1, -2)
//                 angle = atan2f(dx=−7.4, dz=3.0) ≈ −68°
//   Drop slot 0:  port-outer, ship-local (−1.8, deck, −7.5)
//                 world ≈ (6.2, ?, −7.5) at default ship pose
//                 angle = atan2f(dx=9.2, dz=−2.5) ≈ 105°
//                 This is the OPPOSITE (far-aft port) end from current heading,
//                 physically reachable (distance 9.5 < boom reach 10.24).

enum CraneOpState {
    COP_IDLE = 0,
    COP_GOTO_PICKUP,
    COP_LOWER_HOOK,
    COP_ATTACH,
    COP_LIFT,
    COP_WAIT_DELIVERY,  // holds here after Part 1 — awaits U key for Part 2
    COP_GOTO_SHIP,
    COP_LOWER_SLOT,
    COP_DETACH,
    COP_RETURN,
    COP_COMPLETE
};
CraneOpState craneOpState        = COP_IDLE;
bool         craneOpDone         = false;  // set after completion; prevents re-run
bool         craneCarrying       = false;  // true → draw transit container at hookWorldPos
bool         yardPickedCol[4]    = {false,false,false,false}; // per-column top container hidden
int          cranePickupSlot     = 0;     // activeSlot captured at V press time
float        cranePickupAngle    = -68.0f; // computed per-slot yard pickup angle (degrees)
float        craneDeliveryAngle  = 105.0f; // computed per-slot ship delivery angle (degrees)
float        craneOpTimer        = 0.0f;  // dwell timer for ATTACH/DETACH states
bool         capsizePrinted      = false; // reset on Backspace
bool         sinkingPrinted      = false; // reset on Backspace
int          activeYardCol       = 0;     // active port yard column for crane pickup (Tab cycles 0-3)
int          craneDeliverySlot   = 0;     // ship slot to deliver to (captured from activeSlot at V press)
int          cranePickupTexIdx   = 1;     // texture index of the container currently being carried

// Crane angle computed as: atan2f(target_world_X - crane_X, target_world_Z - crane_Z)
// where crane is at world (-3, ?, -5).
static const float COP_ANGLE_PICKUP  = -68.0f;  // toward yard row 0 col 2 (world -10.4, -2)
static const float COP_PITCH_PICKUP  =   8.0f;  // boom angled so hook reaches ~3.5 m height
static const float COP_PITCH_LIFT    =  -5.0f;  // raised boom for safe swing transit
static const float COP_ANGLE_SHIP    = 105.0f;  // toward ship slot 0 port-outer (world 6.2, -7.5)
static const float COP_PITCH_SHIP    =   8.0f;  // boom lowered for deck placement
static const float COP_ANGLE_REST    =   0.0f;  // rest: boom points toward harbour (+Z)
static const float COP_PITCH_REST    = -20.0f;  // rest pitch
static const float COP_ANG_SPEED     =  28.0f;  // degrees/s — slow enough to look realistic
static const float COP_PITCH_SPEED   =  18.0f;  // degrees/s
static const float COP_DWELL_TIME    =   1.4f;  // seconds pause at attach / detach


GLuint loadTexture(const char *path)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // Set texture wrapping to repeat and filtering to linear with mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // Load image data using stb_image (flipped vertically for OpenGL's bottom-left origin)
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path, &w, &h, &ch, 0);
    if (data)
    {
        GLenum fmt = (ch == 4) ? GL_RGBA : (ch == 3) ? GL_RGB
                                                     : GL_RED;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Loaded texture: " << path << endl;
    }
    else
    {
        cout << "Failed to load texture: " << path << endl;
        glDeleteTextures(1, &tex);
        tex = 0;
    }
    stbi_image_free(data);
    return tex;
}

// Layout locations: 0=position, 1=normal, 2=texCoord
void setupVAO8(unsigned int &vao, unsigned int &vbo, unsigned int &ebo, float *v, int vBytes, unsigned int *idx, int iBytes)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vBytes, v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, iBytes, idx, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2); // texCoord
    glBindVertexArray(0);
}

// ==================== PRIMITIVES (8-float stride: pos3+norm3+uv2) ====================
void createCube()
{
    float v[] = {
        // Front +Z
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0, 0, 0.5f, -0.5f, 0.5f, 0, 0, 1, 1, 0, 0.5f, 0.5f, 0.5f, 0, 0, 1, 1, 1, -0.5f, 0.5f, 0.5f, 0, 0, 1, 0, 1,
        // Back -Z
        -0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0, 0.5f, -0.5f, -0.5f, 0, 0, -1, 0, 0, 0.5f, 0.5f, -0.5f, 0, 0, -1, 0, 1, -0.5f, 0.5f, -0.5f, 0, 0, -1, 1, 1,
        // Left -X
        -0.5f, 0.5f, 0.5f, -1, 0, 0, 1, 1, -0.5f, 0.5f, -0.5f, -1, 0, 0, 0, 1, -0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0, -0.5f, -0.5f, 0.5f, -1, 0, 0, 1, 0,
        // Right +X
        0.5f, 0.5f, 0.5f, 1, 0, 0, 0, 1, 0.5f, 0.5f, -0.5f, 1, 0, 0, 1, 1, 0.5f, -0.5f, -0.5f, 1, 0, 0, 1, 0, 0.5f, -0.5f, 0.5f, 1, 0, 0, 0, 0,
        // Top +Y
        -0.5f, 0.5f, 0.5f, 0, 1, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 1, 0, 1, 1, 0.5f, 0.5f, -0.5f, 0, 1, 0, 1, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0, 0, 0,
        // Bottom -Y
        -0.5f, -0.5f, 0.5f, 0, -1, 0, 0, 0, 0.5f, -0.5f, 0.5f, 0, -1, 0, 1, 0, 0.5f, -0.5f, -0.5f, 0, -1, 0, 1, 1, -0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 1};
    unsigned int idx[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};
    setupVAO8(cubeVAO, cubeVBO, cubeEBO, v, sizeof(v), idx, sizeof(idx));
}

void createWedge()
{
    glm::vec3 BL(-0.5f, -0.5f, -0.5f), BR(0.5f, -0.5f, -0.5f), TR(0.5f, 0.5f, -0.5f), TL(-0.5f, 0.5f, -0.5f), TIP(0.0f, 0.5f, 0.5f);
    auto calcN = [](glm::vec3 a, glm::vec3 b, glm::vec3 c)
    { return glm::normalize(glm::cross(b - a, c - a)); };
    // Face normals (outward-facing): Back(-Z), Top(up+forward), Bottom(down+forward), Left(-X tilt), Right(+X tilt)
    glm::vec3 nB = calcN(BL, TR, BR), nT = calcN(TL, TIP, TR), nBo = calcN(BR, TIP, BL), nL = calcN(BL, TIP, TL), nR = calcN(TR, TIP, BR);
    float v[] = {
        BL.x,
        BL.y,
        BL.z,
        nB.x,
        nB.y,
        nB.z,
        0,
        0,
        BR.x,
        BR.y,
        BR.z,
        nB.x,
        nB.y,
        nB.z,
        1,
        0,
        TR.x,
        TR.y,
        TR.z,
        nB.x,
        nB.y,
        nB.z,
        1,
        1,
        TL.x,
        TL.y,
        TL.z,
        nB.x,
        nB.y,
        nB.z,
        0,
        1,
        TL.x,
        TL.y,
        TL.z,
        nT.x,
        nT.y,
        nT.z,
        0,
        0,
        TR.x,
        TR.y,
        TR.z,
        nT.x,
        nT.y,
        nT.z,
        1,
        0,
        TIP.x,
        TIP.y,
        TIP.z,
        nT.x,
        nT.y,
        nT.z,
        0.5f,
        1,
        BR.x,
        BR.y,
        BR.z,
        nBo.x,
        nBo.y,
        nBo.z,
        0,
        0,
        BL.x,
        BL.y,
        BL.z,
        nBo.x,
        nBo.y,
        nBo.z,
        1,
        0,
        TIP.x,
        TIP.y,
        TIP.z,
        nBo.x,
        nBo.y,
        nBo.z,
        0.5f,
        1,
        BL.x,
        BL.y,
        BL.z,
        nL.x,
        nL.y,
        nL.z,
        0,
        0,
        TL.x,
        TL.y,
        TL.z,
        nL.x,
        nL.y,
        nL.z,
        1,
        0,
        TIP.x,
        TIP.y,
        TIP.z,
        nL.x,
        nL.y,
        nL.z,
        0.5f,
        1,
        TR.x,
        TR.y,
        TR.z,
        nR.x,
        nR.y,
        nR.z,
        0,
        0,
        BR.x,
        BR.y,
        BR.z,
        nR.x,
        nR.y,
        nR.z,
        1,
        0,
        TIP.x,
        TIP.y,
        TIP.z,
        nR.x,
        nR.y,
        nR.z,
        0.5f,
        1,
    };
    unsigned int idx[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    wedgeIndexCount = 18;
    setupVAO8(wedgeVAO, wedgeVBO, wedgeEBO, v, sizeof(v), idx, sizeof(idx));
}

void createCylinder(float radius, float height, int sectors, int stacks)
{
    vector<float> verts;
    vector<unsigned int> inds;
    float halfH = height / 2.0f, stackStep = height / stacks, secStep = 2.0f * PI / sectors;
    for (int i = 0; i <= stacks; i++)
    {
        float y = -halfH + i * stackStep;
        float v = (float)i / stacks;
        for (int j = 0; j <= sectors; j++)
        {
            float t = j * secStep, x = radius * cos(t), z = radius * sin(t);
            float u = (float)j / sectors;
            verts.insert(verts.end(), {x, y, z, x / radius, 0, z / radius, u, v});
        }
    }
    int ring = sectors + 1;
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < sectors; j++)
        {
            int c = i * ring + j, n = (i + 1) * ring + j;
            inds.push_back(c);
            inds.push_back(n);
            inds.push_back(c + 1);
            inds.push_back(c + 1);
            inds.push_back(n);
            inds.push_back(n + 1);
        }
    int tc = verts.size() / 8;
    verts.insert(verts.end(), {0, halfH, 0, 0, 1, 0, 0.5f, 0.5f});
    int tr = verts.size() / 8;
    for (int j = 0; j <= sectors; j++)
    {
        float t = j * secStep;
        float u = 0.5f + 0.5f * cos(t), vv = 0.5f + 0.5f * sin(t);
        verts.insert(verts.end(), {radius * cos(t), halfH, radius * sin(t), 0, 1, 0, u, vv});
    }
    for (int j = 0; j < sectors; j++)
    {
        inds.push_back(tc);
        inds.push_back(tr + j);
        inds.push_back(tr + j + 1);
    }
    int bc = verts.size() / 8;
    verts.insert(verts.end(), {0, -halfH, 0, 0, -1, 0, 0.5f, 0.5f});
    int br = verts.size() / 8;
    for (int j = 0; j <= sectors; j++)
    {
        float t = j * secStep;
        float u = 0.5f + 0.5f * cos(t), vv = 0.5f + 0.5f * sin(t);
        verts.insert(verts.end(), {radius * cos(t), -halfH, radius * sin(t), 0, -1, 0, u, vv});
    }
    for (int j = 0; j < sectors; j++)
    {
        inds.push_back(bc);
        inds.push_back(br + j + 1);
        inds.push_back(br + j);
    }
    cylIndexCount = (int)inds.size();
    glGenVertexArrays(1, &cylVAO);
    glGenBuffers(1, &cylVBO);
    glGenBuffers(1, &cylEBO);
    glBindVertexArray(cylVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cylVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void createSphere(float radius, int sectors, int stacks)
{
    vector<float> verts;
    vector<unsigned int> inds;

    float secStep = 2.0f * PI / sectors;
    float stkStep = PI / stacks; // 0 → PI

    // ----- Generate Vertices -----
    for (int i = 0; i <= stacks; i++)
    {
        float phi = i * stkStep; // 0 → PI  (from +Y to -Y)

        float y = radius * cos(phi);
        float r = radius * sin(phi); // horizontal ring radius

        for (int j = 0; j <= sectors; j++)
        {
            float theta = j * secStep; // 0 → 2PI

            float x = r * cos(theta);
            float z = r * sin(theta);

            float u = (float)j / sectors;
            float v = (float)i / stacks;

            // position (3) + normal (3) + texcoord (2)
            verts.insert(verts.end(),
                         {x, y, z,
                          x / radius, y / radius, z / radius,
                          u, v});
        }
    }

    // ----- Generate Indices -----
    int ring = sectors + 1;

    for (int i = 0; i < stacks; i++)
    {
        for (int j = 0; j < sectors; j++)
        {
            int c = i * ring + j;
            int n = (i + 1) * ring + j;

            inds.push_back(c);
            inds.push_back(n);
            inds.push_back(c + 1);

            inds.push_back(c + 1);
            inds.push_back(n);
            inds.push_back(n + 1);
        }
    }

    sphIndexCount = (int)inds.size();

    glGenVertexArrays(1, &sphVAO);
    glGenBuffers(1, &sphVBO);
    glGenBuffers(1, &sphEBO);

    glBindVertexArray(sphVAO);

    glBindBuffer(GL_ARRAY_BUFFER, sphVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(float),
                 verts.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 inds.size() * sizeof(unsigned int),
                 inds.data(),
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void createCone(float radius, float height, int sectors)
{
    vector<float> verts;
    vector<unsigned int> inds;
    float halfH = height / 2.0f, secStep = 2.0f * PI / sectors, slope = radius / height;
    int tipIdx = 0;
    verts.insert(verts.end(), {0, halfH, 0, 0, 1, 0, 0.5f, 1.0f});
    int baseStart = verts.size() / 8;
    for (int j = 0; j <= sectors; j++)
    {
        float t = j * secStep, x = radius * cos(t), z = radius * sin(t);
        float nx = cos(t), nz = sin(t), ny = slope;
        float len = sqrt(nx * nx + ny * ny + nz * nz);
        float u = (float)j / sectors;
        verts.insert(verts.end(), {x, -halfH, z, nx / len, ny / len, nz / len, u, 0.0f});
    }
    for (int j = 0; j < sectors; j++)
    {
        inds.push_back(tipIdx);
        inds.push_back(baseStart + j + 1);
        inds.push_back(baseStart + j);
    }
    int bc = verts.size() / 8;
    verts.insert(verts.end(), {0, -halfH, 0, 0, -1, 0, 0.5f, 0.5f});
    int br = verts.size() / 8;
    for (int j = 0; j <= sectors; j++)
    {
        float t = j * secStep;
        float u = 0.5f + 0.5f * cos(t), vv = 0.5f + 0.5f * sin(t);
        verts.insert(verts.end(), {radius * cos(t), -halfH, radius * sin(t), 0, -1, 0, u, vv});
    }
    for (int j = 0; j < sectors; j++)
    {
        inds.push_back(bc);
        inds.push_back(br + j + 1);
        inds.push_back(br + j);
    }
    coneIndexCount = (int)inds.size();
    glGenVertexArrays(1, &coneVAO);
    glGenBuffers(1, &coneVBO);
    glGenBuffers(1, &coneEBO);
    glBindVertexArray(coneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, coneVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, coneEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

// ==================== DRAW HELPER FUNCTIONS ====================
// setMaterial: uploads ambient, diffuse, specular, shininess, emissive to the active shader
void setMaterial(Shader &s, glm::vec3 amb, glm::vec3 dif, glm::vec3 spc, float shin, glm::vec3 emi)
{
    s.use();
    s.setVec3("material.ambient", amb);
    s.setVec3("material.diffuse", dif);
    s.setVec3("material.specular", spc);
    s.setFloat("material.shininess", shin);
    s.setVec3("material.emissive", emi);
}
// buildModel: constructs a model matrix from parent transform + translate + rotate(XYZ) + scale
glm::mat4 buildModel(glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz)
{
    glm::mat4 m = glm::translate(p, glm::vec3(px, py, pz));
    m = glm::rotate(m, glm::radians(rx), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(ry), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rz), glm::vec3(0, 0, 1));
    return glm::scale(m, glm::vec3(sx, sy, sz));
}
// bindTex: if a valid texture handle is provided, binds it to GL_TEXTURE0 and enables texture sampling;
//          otherwise disables texture sampling so the shader uses material colors only.
//          NOTE: texScaleU is set once per frame (not here) to avoid overriding per-object UV scaling.
void bindTex(Shader &s, GLuint tex)
{
    if (tex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        s.setInt("texture1", 0);
        s.setBool("useTexture", true);
    }
    else
    {
        s.setBool("useTexture", false);
    }
}

// Each drawXxx function calls setMaterial + bindTex + buildModel, then draws the primitive.
// Pass tex=0 (default) for no texture, or a valid GLuint texture handle for textured rendering.
void drawCube(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 a, glm::vec3 d, glm::vec3 sp, float sh, glm::vec3 e, GLuint tex)
{
    setMaterial(s, a, d, sp, sh, e);
    bindTex(s, tex);
    s.setMat4("model", buildModel(p, px, py, pz, rx, ry, rz, sx, sy, sz));
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void drawCylinder(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 a, glm::vec3 d, glm::vec3 sp, float sh, glm::vec3 e, GLuint tex)
{
    setMaterial(s, a, d, sp, sh, e);
    bindTex(s, tex);
    s.setMat4("model", buildModel(p, px, py, pz, rx, ry, rz, sx, sy, sz));
    glBindVertexArray(cylVAO);
    glDrawElements(GL_TRIANGLES, cylIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void drawSphere(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 a, glm::vec3 d, glm::vec3 sp, float sh, glm::vec3 e, GLuint tex)
{
    setMaterial(s, a, d, sp, sh, e);
    bindTex(s, tex);
    s.setMat4("model", buildModel(p, px, py, pz, rx, ry, rz, sx, sy, sz));
    glBindVertexArray(sphVAO);
    glDrawElements(GL_TRIANGLES, sphIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void drawCone(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 a, glm::vec3 d, glm::vec3 sp, float sh, glm::vec3 e, GLuint tex)
{
    setMaterial(s, a, d, sp, sh, e);
    bindTex(s, tex);
    s.setMat4("model", buildModel(p, px, py, pz, rx, ry, rz, sx, sy, sz));
    glBindVertexArray(coneVAO);
    glDrawElements(GL_TRIANGLES, coneIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
void drawWedge(Shader &s, glm::mat4 p, float px, float py, float pz, float rx, float ry, float rz, float sx, float sy, float sz, glm::vec3 a, glm::vec3 d, glm::vec3 sp, float sh, glm::vec3 e, GLuint tex)
{
    setMaterial(s, a, d, sp, sh, e);
    bindTex(s, tex);
    s.setMat4("model", buildModel(p, px, py, pz, rx, ry, rz, sx, sy, sz));
    glBindVertexArray(wedgeVAO);
    glDrawElements(GL_TRIANGLES, wedgeIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ==================== MAIN FUNCTION ====================
int main()
{
    // --- GLFW / GLAD Initialization ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // OpenGL 3.3
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "SeaForge - 3D Sea Port", NULL, NULL);
    if (!window)
    {
        cout << "Failed to create window" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetScrollCallback(window, scroll_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed GLAD" << endl;
        return -1;
    }
    glEnable(GL_DEPTH_TEST);

    // --- Load BOTH shader pairs ---
    // Phong: per-fragment lighting (more accurate specular highlights)
    // Gouraud: per-vertex lighting (faster, smoother gradients, less accurate speculars)
    Shader phongShader("vertexShaderPhong.vs", "fragmentShaderPhong.fs");
    Shader gouraudShader("vertexShaderGouraud.vs", "fragmentShaderGouraud.fs");

    // --- Create all 5 primitive shapes (generates GPU buffers) ---
    createCube();
    createCylinder(0.2f, 0.2f, 36, 5);
    createSphere(0.5f, 36, 18);
    createCone(0.5f, 1.0f, 36);
    createWedge();
    generateAncientBoatMesh(); // A2: Bezier lofted hull VAO
    generateOarMesh();         // A2: B-spline extruded oar VAO
    generatePatrolShipHull();  // A4: ruled-surface upper hull panels
    generateBridgeMesh();      // Arch bridge: cubic B-spline ruled-surface deck
    createWaterMesh(100, 100, 70.0f, 70.0f); // B1: animated Gerstner wave grid

    // Load textures (all .jpg files in project root directory)
    const char *contFiles[] = {"container_1.jpg", "container_2.jpg", "container_3.jpg", "container_4.jpg", "container_5.jpg"};
    for (int i = 0; i < 5; i++)
        containerTex[i] = loadTexture(contFiles[i]);
    brickTex = loadTexture("wall_brick.jpg");
    woodDeckTex = loadTexture("wood_deck.jpg");
    bouyTex = loadTexture("bouy.jpg");
    // Set buoy texture wrapping to MIRRORED_REPEAT for seamless sphere UV mapping
    if (bouyTex)
    {
        glBindTexture(GL_TEXTURE_2D, bouyTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    }
    concreteRoofTex = loadTexture("concrete_roof.jpg");
    discoBallTex = loadTexture("disco_ball.jpg");
    coneTex = loadTexture("cone.jpg");
    // Set cone texture wrapping to MIRRORED_REPEAT so stripes wrap seamlessly around cone
    if (coneTex)
    {
        glBindTexture(GL_TEXTURE_2D, coneTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    }
    mooringBollardTex = loadTexture("mooring_bollards.jpg");
    // Set mooring bollard texture to REPEAT for continuous horizontal strips
    if (mooringBollardTex)
    {
        glBindTexture(GL_TEXTURE_2D, mooringBollardTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }
    mooringBollardTopTex = loadTexture("mooring_bollards_top.jpg");
    shipBodyTex    = loadTexture("ship_body.jpg");
    ancientBoatTex     = loadTexture("ancient_boat_wood.jpg"); // optional — 0 = material only
    treeBarkTex        = loadTexture("tree_bark.jpg");         // A3: optional
    treeLeavesTotalTex = loadTexture("tree_leaves.jpg");       // A3: optional
    patrolBodyTex  = loadTexture("Patrol_ship_body.jpg");      // A4: patrol hull
    patrolWallTex  = loadTexture("Patrol_ship_wall.jpg");      // A4: patrol superstructure walls
    patrolGlassTex = loadTexture("Patrol_ship_glass.jpg");     // A4: patrol windows
    patrolRoofTex  = loadTexture("Patrol_ship_roof.jpg");      // A4: patrol roofs
    chineseRoofTex  = loadTexture("chinese_house_roof_1.jpg"); // Chinese house tin roof
    chineseRoof2Tex = loadTexture("chinese_house_roof_2.jpg"); // Chinese house flat roof
    grassTex       = loadTexture("grass.jpg");                // Grass ground
    cementTex      = loadTexture("cement_tex.jpg");           // Stairs/walkway
    chineseWallTex = loadTexture("chinese_house_wall.jpg");  // Chinese house walls
    bridgeSurfTex  = loadTexture("bridge_surface.jpg");      // bridge surface
    metalTex       = loadTexture("metal.jpg");               // street lamps

    initHuman();     // Human system — static figure on main dock
    printControls();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // default fill mode
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);
        updateShipMovement(window, deltaTime);              // B3: velocity-based ship movement + turning with pivot
        updateWavePhysics(shipZ, deltaTime, currentFrame);  // B1: cargo ship wave interaction
        updateShipBuoyancy(deltaTime);                      // B4: Archimedes draft + metacentric roll + capsize
        patrolAutopilot.update(deltaTime);                   // B6: autopilot drives patrol ship
        // Human is a static decorative figure — no update needed

        // B7: Crane Loading Operation — two-part state machine
        // Only runs in active states (not IDLE, WAIT_DELIVERY, or COMPLETE).
        if (craneOpState != COP_IDLE &&
            craneOpState != COP_WAIT_DELIVERY &&
            craneOpState != COP_COMPLETE)
        {
            auto lerpA = [](float cur, float tgt, float spd, float dtL) -> float {
                float diff = tgt - cur;
                float step = spd * dtL;
                if (fabsf(diff) <= step) return tgt;
                return cur + (diff > 0 ? step : -step);
            };
            bool angOk, pitchOk;
            switch (craneOpState) {

            // ── PART 1: Pickup from yard ──────────────────────────────────────
            case COP_GOTO_PICKUP:
                // Rotate turret toward the yard column for cranePickupSlot
                craneArmAngle = lerpA(craneArmAngle, cranePickupAngle, COP_ANG_SPEED,   deltaTime);
                craneArmPitch = lerpA(craneArmPitch, COP_PITCH_PICKUP, COP_PITCH_SPEED, deltaTime);
                angOk   = fabsf(craneArmAngle - cranePickupAngle) < 1.5f;
                pitchOk = fabsf(craneArmPitch - COP_PITCH_PICKUP) < 1.5f;
                if (angOk && pitchOk) { craneOpState = COP_LOWER_HOOK; }
                break;

            case COP_LOWER_HOOK: {
                // Extend cable until hook reaches the top of the yard container.
                // Yard container top: r=0, st=1 → world Y = 0.55+1.1+1.1/2 = 2.20
                const float yardContTopY = 2.20f;
                float targetLen = glm::clamp(boomTipWorldPos.y - yardContTopY, 1.0f, 12.0f);
                float cDiff = targetLen - craneCableLength;
                float cStep = 2.5f * deltaTime;
                craneCableLength = (fabsf(cDiff) <= cStep) ? targetLen : craneCableLength + (cDiff > 0 ? cStep : -cStep);
                // Transition when hook is within 0.2 m of container top
                if (fabsf(hookWorldPos.y - yardContTopY) < 0.2f)
                    { craneOpState = COP_ATTACH; craneOpTimer = COP_DWELL_TIME; }
                break;
            }

            case COP_ATTACH:
                // Dwell: crane has latched onto container
                craneOpTimer -= deltaTime;
                if (craneOpTimer <= 0.0f) {
                    yardPickedCol[cranePickupSlot] = true;  // hide that column's top container
                    craneCarrying                  = true;  // container now hangs from hook
                    craneOpState                   = COP_LIFT;
                    printf("[Crane] Part 1: Container attached. Lifting...\n");
                }
                break;

            case COP_LIFT:
                // Raise boom + retract cable back to transit length (6 m)
                craneArmPitch = lerpA(craneArmPitch, COP_PITCH_LIFT, COP_PITCH_SPEED, deltaTime);
                {
                    float cDiff = 6.0f - craneCableLength;
                    float cStep = 2.5f * deltaTime;
                    craneCableLength = (fabsf(cDiff) <= cStep) ? 6.0f : craneCableLength + (cDiff > 0 ? cStep : -cStep);
                }
                if (fabsf(craneArmPitch - COP_PITCH_LIFT) < 1.0f && fabsf(craneCableLength - 6.0f) < 0.2f) {
                    craneOpState = COP_WAIT_DELIVERY;
                    printf("[Crane] Part 1 complete: container lifted and held. Press U to deliver to ship.\n");
                }
                break;

            // ── PART 2: Deliver to ship slot 0 (port-outer, opposite/far-aft end) ─
            case COP_GOTO_SHIP:
                // Swing turret toward the computed ship slot angle
                craneArmAngle = lerpA(craneArmAngle, craneDeliveryAngle, COP_ANG_SPEED, deltaTime);
                if (fabsf(craneArmAngle - craneDeliveryAngle) < 1.5f) { craneOpState = COP_LOWER_SLOT; }
                break;

            case COP_LOWER_SLOT: {
                // Lower boom + extend cable to reach top of ship slot stack.
                // Ship slot top world Y = shipHeave + baseY + markerH + numStacked*contH
                // baseY = hullY+hullH/2+0.04+0.04 = 0.3+1.1+0.08 = 1.48; markerH=0.08; contH=1.1
                craneArmPitch = lerpA(craneArmPitch, COP_PITCH_SHIP, COP_PITCH_SPEED, deltaTime);
                int numStacked = glm::min((int)(containerWeights[craneDeliverySlot] / 5.0f), 5);
                float slotTopY = shipHeave + 1.56f + numStacked * 1.1f;
                float hookTargetY = slotTopY + 1.1f; // Hook holds top of container
                float targetLen = glm::clamp(boomTipWorldPos.y - hookTargetY, 1.0f, 12.0f);
                float cDiff = targetLen - craneCableLength;
                float cStep = 2.5f * deltaTime;
                craneCableLength = (fabsf(cDiff) <= cStep) ? targetLen : craneCableLength + (cDiff > 0 ? cStep : -cStep);
                bool pitchOk  = fabsf(craneArmPitch - COP_PITCH_SHIP) < 1.0f;
                bool hookReached = fabsf(hookWorldPos.y - hookTargetY) < 0.2f;
                if (pitchOk && hookReached)
                    { craneOpState = COP_DETACH; craneOpTimer = COP_DWELL_TIME; }
                break;
            }

            case COP_DETACH:
                // Dwell: release container into the matching ship slot
                craneOpTimer -= deltaTime;
                if (craneOpTimer <= 0.0f) {
                    craneCarrying = false;
                    containerWeights[craneDeliverySlot] = glm::min(containerWeights[craneDeliverySlot] + 5.0f, 40.0f);
                    printf("[Crane] Part 2: Container placed in ship slot %d. Weight: %.0f t\n",
                           craneDeliverySlot, containerWeights[craneDeliverySlot]);
                    craneOpState = COP_RETURN;
                }
                break;

            case COP_RETURN:
                // Return crane to default rest pose + retract cable
                craneArmAngle = lerpA(craneArmAngle, COP_ANGLE_REST,  COP_ANG_SPEED,   deltaTime);
                craneArmPitch = lerpA(craneArmPitch, COP_PITCH_REST,  COP_PITCH_SPEED, deltaTime);
                {
                    float cDiff = 6.0f - craneCableLength;
                    float cStep = 2.5f * deltaTime;
                    craneCableLength = (fabsf(cDiff) <= cStep) ? 6.0f : craneCableLength + (cDiff > 0 ? cStep : -cStep);
                }
                angOk   = fabsf(craneArmAngle - COP_ANGLE_REST)  < 1.5f;
                pitchOk = fabsf(craneArmPitch - COP_PITCH_REST)  < 1.5f;
                if (angOk && pitchOk && fabsf(craneCableLength - 6.0f) < 0.2f) {
                    craneOpState = COP_IDLE;  // always return to idle; user picks next action
                    if (containerWeights[craneDeliverySlot] >= 40.0f)
                        printf("[Crane] Ship slot %d full (40 t). Press C to pick another slot.\n", craneDeliverySlot);
                    else
                        printf("[Crane] Ready. Slot %d: %.0f/40 t. Press V to load more.\n",
                               craneDeliverySlot, containerWeights[craneDeliverySlot]);
                }
                break;

            default: break;
            }
        }

        updateBoatAndOthers(window, deltaTime, currentFrame); // B1: boat rowing + all vessel waves
        updateCranePhysics(craneArmAngle, craneArmPitch, prevCraneAngle, prevCranePitch, shipX, shipZ, deltaTime, craneCarrying); // B2: 2D spherical pendulum + collision
        prevCraneAngle = craneArmAngle; // B2: store for next frame's delta
        prevCranePitch = craneArmPitch; // B2: store for next frame's pitch delta
        basic_camera.lookAt = basic_camera.eye + basic_camera.Front;

        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // --- Select active shader based on user toggle (key 4) ---
        Shader &ourShader = useGouraud ? gouraudShader : phongShader;

        // --- Projection: perspective (default) or orthographic ---
        float aspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        glm::mat4 projection = isPerspective ? glm::perspective(glm::radians(basic_camera.Zoom), aspect, 0.1f, 200.0f)
                                             : glm::ortho(-10 * aspect, 10 * aspect, -10.0f, 10.0f, -100.0f, 100.0f);
        glm::mat4 view = basic_camera.createViewMatrix();

        // --- Upload common uniforms to the active shader ---
        ourShader.use();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);
        ourShader.setVec3("viewPos", basic_camera.eye);
        ourShader.setBool("ambientOn", ambientOn);
        ourShader.setBool("diffuseOn", diffuseOn);
        ourShader.setBool("specularOn", specularOn);
        ourShader.setBool("directLightOn", dirLightOn);
        ourShader.setBool("pointLightOn", pointLightOn);
        ourShader.setBool("spotLightOn", spotLightOn);
        ourShader.setInt("renderMode", renderMode);           // 0=material, 1=texture, 2=blended
        ourShader.setFloat("time", currentFrame);              // B1: wave time
        ourShader.setInt("useWave", 0);                        // B1: off by default, drawWaterMesh enables it
        ourShader.setVec2("texScale", glm::vec2(1.0f, 1.0f)); // Default: no texture scaling (overridden per-object)
        
        ourShader.setVec3("globalAmbient", glm::vec3(0.3f, 0.3f, 0.3f));
        // --- Directional light (sun) ---
        ourShader.setVec3("dirLight.direction", -0.3f, -1.0f, -0.5f);
        ourShader.setVec3("dirLight.ambient", 0.4f, 0.4f, 0.45f);
        ourShader.setVec3("dirLight.diffuse", 0.9f, 0.85f, 0.8f);
        ourShader.setVec3("dirLight.specular", 1, 1, 1);

        // --- 8 Point lights: 4 street lamps + 4 arch bridge lanterns ---
        // Street lamps: base at y=0, pole top at y=5.0, lanterns hang at ~y=4.28
        glm::vec3 pCol = pointLightOn ? glm::vec3(0.65, 0.55f, 0.65f) : glm::vec3(0);
        glm::vec3 plPos[9] = {
            glm::vec3(-10.0f, 5.0f, 5.0f),    // 0: Front of SeaPort building
            glm::vec3(-1.0f, 5.0f, 10.0f),    // 1: Road-right, south dock
            glm::vec3(0.0f, 5.0f,-10.0f),    // 2: L-arm entrance
            glm::vec3(-16.0f, 5.0f, -7.0f),    // 3: Road-left, mid dock
            glm::vec3(-2.0f,3.0f,0.0f), glm::vec3(-2.0f,3.0f,0.0f), glm::vec3(-2.0f,3.0f,0.0f), glm::vec3(-2.0f,3.0f,0.0f), // 4-7: bridge lanterns (filled below)
            glm::vec3(27.0f, 5.0f, -18.0f)
        };

        getBridgeLightPositions(&plPos[4]); // Fill slots 4-7 with bridge lantern positions
        for (int j = 0; j < 9; j++)
        {
            string n = to_string(j);
            ourShader.setVec3("pointLights[" + n + "].position", plPos[j]);
            ourShader.setFloat("pointLights[" + n + "].constant", 1.0f);
            ourShader.setFloat("pointLights[" + n + "].linear", j < 4 ? 0.045f : 0.09f);
            ourShader.setFloat("pointLights[" + n + "].quadratic", j < 4 ? 0.0075f : 0.032f);
            ourShader.setVec3("pointLights[" + n + "].ambient", pCol * 0.1f);
            ourShader.setVec3("pointLights[" + n + "].diffuse", pCol);
            ourShader.setVec3("pointLights[" + n + "].specular", pCol);

        }
        // --- Spot light (player flashlight, follows camera) ---
        ourShader.setVec3("spotLight.position", basic_camera.eye);
        ourShader.setVec3("spotLight.direction", basic_camera.Front);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(13.0f)));
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.014f);
        ourShader.setFloat("spotLight.quadratic", 0.0007f);
        ourShader.setVec3("spotLight.ambient", 0.2f, 0.2f, 0);
        ourShader.setVec3("spotLight.diffuse", 1, 1, 0);
        ourShader.setVec3("spotLight.specular", 1, 1, 0);

        ourShader.setBool("useObjectColor", false);
        ourShader.setFloat("alpha", 1.0f);

        // --- Draw all scene components ---
        glm::mat4 I(1.0f); // Identity matrix as root parent transform
        drawWaterMesh(ourShader, I, currentFrame); // B1: animated Gerstner wave grid
        drawDock(ourShader, I);
        drawHuman(ourShader);                // Human figure
        drawMooringRopes(ourShader, I);  // A1: Catmull-Rom ropes, updates with shipZ
        drawSeaPortBuilding(ourShader, I);
        drawContainerYard(ourShader, I);
    
        drawMobileCrane(ourShader, I);
        drawCargoShip(ourShader, I);
        drawLighthouse(ourShader, I);
        drawBuoys(ourShader, I);
        drawAncientBoat(ourShader, I); // A2
        drawPatrolShip(ourShader, I);  // A4: orange harbor patrol vessel
        drawPatrolShipPath(ourShader, I); // B6: visualize autopilot B-spline path + control points
        // Street lamps beside the road (base at ground level)
        drawStreetLamp(ourShader, I, -10.0f, 0.0f, 5.0f, 90.0f);   // Front of SeaPort building (rotated 90)
        drawStreetLamp(ourShader, I,  -1.0f, 0.0f, 10.0f);        // Road-right, south dock
        drawStreetLamp(ourShader, I,  0.0f, 0.0f,-10.0f);         // L-arm entrance
        drawStreetLamp(ourShader, I, -16.0f, 0.0f, -7.0f, 90.0f); // Road-left, mid dock (rotated 90)
        drawStreetLamp(ourShader, I, 27.0f, 0.0f, -18.0f); // Chinese house

        // Chinese house on opposite side of harbor (across the water)
        drawChineseHouse(ourShader, I, 22.0f, 0.0f, -22.0f);

        // Arch bridge connecting dock to Chinese house island
        drawArchBridge(ourShader, I,-2.0f,0.0f,0.0f);

        // A3: 3 trees — each with distinct branching character
        // Tree 1: fixed 3 branches/node, 45° lean, depth 4  (balanced oak)

        drawTree(ourShader, I, -10.0f, 0.3f, -10.0f, 4, 2.0f, 0.18f, 3, 3, 45.0f, 1);
        // Tree 2: random 2–4 branches/node, 50° lean, depth 4, taller  (wild/spreading)
        drawTree(ourShader, I, -19.0f, 0.3f,  -4.0f, 4, 2.6f, 0.22f, 2, 4, 50.0f, 42);
        // Tree 3: random 2–3 branches/node, 20° lean, depth 5  (narrow/pine-like)
        drawTree(ourShader, I, -11.0f, 0.3f,   9.0f, 5, 1.7f, 0.13f, 2, 3, 20.0f, 7);

        //// --- Coordinate axes at origin: Red=X, Green=Y, Blue=Z ---
        //glm::vec3 ax(0); float axL=10.0f, axT=0.05f;
        //drawCube(ourShader,I, axL/2,0,0, 0,0,0, axL,axT,axT, ax,glm::vec3(1,0,0),ax,1, glm::vec3(1,0,0)); // X red
        //drawCube(ourShader,I, 0,axL/2,0, 0,0,0, axT,axL,axT, ax,glm::vec3(0,1,0),ax,1, glm::vec3(0,1,0)); // Y green
        //drawCube(ourShader,I, 0,0,axL/2, 0,0,0, axT,axT,axL, ax,glm::vec3(0,0,1),ax,1, glm::vec3(0,0,1)); // Z blue
        
        // B4: Capsize overlay — darken scene when ship capsizes
        if (triggerCapsize)
        {
            // Draw a dark semi-transparent overlay (fullscreen quad via cube at camera)
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            ourShader.setFloat("alpha", 0.5f);
            glm::mat4 capBill(1.0f);
            capBill[0] = glm::vec4(basic_camera.Right,  0.0f);
            capBill[1] = glm::vec4(basic_camera.Up,     0.0f);
            capBill[2] = glm::vec4(-basic_camera.Front, 0.0f);
            capBill[3] = glm::vec4(basic_camera.eye + basic_camera.Front * 0.5f, 1.0f);
            drawCube(ourShader, capBill, 0, 0, 0, 0, 0, 0, 5, 5, 0.01f,
                     glm::vec3(0.1f, 0.0f, 0.0f), glm::vec3(0.2f, 0.0f, 0.0f),
                     glm::vec3(0), 1, glm::vec3(0));
            ourShader.setFloat("alpha", 1.0f);
            glDisable(GL_BLEND);

            // Print capsize message once
            if (!capsizePrinted)
            {
                cout << "\n!!! CAPSIZED !!! Ship has exceeded 70 deg roll.\n" << endl;
                capsizePrinted = true;
            }
        }

        // D1: Overload sinking overlay — blue tint when ship is overloaded
        if (triggerSinking)
        {
            // Pulse alpha between 0.25 and 0.45 so the tint flickers urgently
            float pulse = 0.35f + 0.10f * sinf(currentFrame * 3.0f);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            ourShader.setFloat("alpha", pulse);

            // Billboard matrix: face always toward the camera regardless of view direction.
            // Column vectors = camera basis axes; translation = 0.5 units in front of eye.
            glm::mat4 bill(1.0f);
            bill[0] = glm::vec4(basic_camera.Right,  0.0f);   // local X = camera right
            bill[1] = glm::vec4(basic_camera.Up,     0.0f);   // local Y = camera up
            bill[2] = glm::vec4(-basic_camera.Front, 0.0f);   // local Z points at camera
            bill[3] = glm::vec4(basic_camera.eye + basic_camera.Front * 0.5f, 1.0f);
            drawCube(ourShader, bill, 0, 0, 0, 0, 0, 0, 5, 5, 0.01f,
                     glm::vec3(0.0f, 0.02f, 0.12f), glm::vec3(0.0f, 0.05f, 0.25f),
                     glm::vec3(0), 1, glm::vec3(0));
            ourShader.setFloat("alpha", 1.0f);
            glDisable(GL_BLEND);

            // Print sinking warning once
            if (!sinkingPrinted)
            {
                cout << "\n!!! OVERLOADED — SINKING !!! Total mass: " << shipTotalMass << " t  (threshold: 100 t)\n" << endl;
                sinkingPrinted = true;
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents(); // Present frame and process window events
    }
    // --- Cleanup: delete all OpenGL buffer objects ---
    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteVertexArrays(1, &cylVAO);
    glDeleteBuffers(1, &cylVBO);
    glDeleteBuffers(1, &cylEBO);
    glDeleteVertexArrays(1, &sphVAO);
    glDeleteBuffers(1, &sphVBO);
    glDeleteBuffers(1, &sphEBO);
    glDeleteVertexArrays(1, &coneVAO);
    glDeleteBuffers(1, &coneVBO);
    glDeleteBuffers(1, &coneEBO);
    glDeleteVertexArrays(1, &wedgeVAO);
    glDeleteBuffers(1, &wedgeVBO);
    glDeleteBuffers(1, &wedgeEBO);
    glDeleteVertexArrays(1, &ancientBoatVAO);
    glDeleteBuffers(1, &ancientBoatVBO);
    glDeleteBuffers(1, &ancientBoatEBO);
    glDeleteVertexArrays(1, &patrolHullVAO);
    glDeleteBuffers(1, &patrolHullVBO);
    glDeleteBuffers(1, &patrolHullEBO);
    glDeleteVertexArrays(1, &waterVAO); // B1
    glDeleteBuffers(1, &waterVBO);
    glDeleteBuffers(1, &waterEBO);
    glfwTerminate();
    return 0;
}

// ==================== INPUT HANDLING ====================
// Processes all keyboard input every frame.
// Uses GLFW's polling model: GLFW_PRESS = key is currently held down.
void processInput(GLFWwindow *window)
{
    // --- Exit (Escape key) ---
    // Note: Ship turning uses comma(,)/period(.) keys (see ship_physics.cpp B3)
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // --- Camera movement (WASD + R for down, V for up, E for up alt, Q to quit) ---
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        basic_camera.MoveForward(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        basic_camera.MoveBackward(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        basic_camera.MoveLeft(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        basic_camera.MoveRight(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        basic_camera.MoveUp(deltaTime);
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        basic_camera.MoveDown(deltaTime);

    // --- Camera rotation (X=pitch, Y=yaw, Z=roll) ---
    float rot = basic_camera.RotationSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        basic_camera.AddPitch(rot);
    if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
        basic_camera.AddYaw(rot);
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        basic_camera.AddRoll(rot);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        basic_camera.OrbitAroundLookAt(deltaTime);

    // --- V key: Reset camera to default position ---
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS)
    {
        basic_camera.eye = glm::vec3(25, 18, 25);
        basic_camera.lookAt = glm::vec3(0);
        basic_camera.Yaw = -135;
        basic_camera.Pitch = -35;
        basic_camera.Roll = 0;
    }

    // --- Preset camera views (arrow keys) ---
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        isPerspective = false;
        basic_camera.eye = glm::vec3(1, 1, 1);
        basic_camera.Yaw = -135;
        basic_camera.Pitch = -35.264f;
        basic_camera.Roll = 0;
        basic_camera.update();
    } // Isometric
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        isPerspective = false;
        basic_camera.eye = glm::vec3(0, 30, 0);
        basic_camera.Yaw = -90;
        basic_camera.Pitch = -89;
        basic_camera.Roll = 0;
        basic_camera.update();
    } // Top-down
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    {
        isPerspective = false;
        basic_camera.eye = glm::vec3(0, 5, 30);
        basic_camera.Yaw = -90;
        basic_camera.Pitch = 0;
        basic_camera.Roll = 0;
        basic_camera.update();
    } // Front
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    {
        isPerspective = true;
        basic_camera.eye = glm::vec3(8, 5, shipZ + 15);
        basic_camera.Yaw = -90;
        basic_camera.Pitch = -5;
        basic_camera.Roll = 0;
        basic_camera.update();
    } // Ship follow

    // --- Ship controls (I/K for thrust, ,/. for turning) handled by ship_physics.cpp (B3) ---
    // --- Crane controls ---
    float craneSpeed = 40.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        craneArmAngle += craneSpeed; // Crane rotate left
    if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        craneArmAngle -= craneSpeed; // Crane rotate right
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS)
    {
        craneArmPitch += craneSpeed;
        if (craneArmPitch > 10)
            craneArmPitch = 10;
    } // Crane arm up
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        craneArmPitch -= craneSpeed;
        if (craneArmPitch < -60)
            craneArmPitch = -60;
    } // Crane arm down

    // --- B5: Cargo weight distribution ( [ =add, ] =remove, C=cycle ship slot, Tab=cycle yard col) ---
    // Press-once debounce so each keypress adds exactly one step.
    // [: +5 t to active ship slot (max 40 t/slot)
    // ]: -5 t from active ship slot (min 0 t)
    // C: cycle active SHIP slot 0→1→2→3→0  (yellow marker on ship deck)
    // Tab: cycle active YARD column 0→1→2→3→0  (yellow slab on yard column top)
    // Console prints current slot weights and GM so the user can monitor stability.
    static bool kBrktL = false, kBrktR = false, kC_slot = false;
    if (!triggerCapsize) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS && !kBrktL) {
            if (containerWeights[activeSlot] < 40.0f) {
                containerWeights[activeSlot] = glm::min(containerWeights[activeSlot] + 5.0f, 40.0f);
                printf("[B5] Slot %d weight: %.0f t  |  Slots: [%.0f, %.0f, %.0f, %.0f] t  |  Total: %.0f t  |  GM: %.3f\n",
                    activeSlot, containerWeights[activeSlot],
                    containerWeights[0], containerWeights[1], containerWeights[2], containerWeights[3],
                    shipTotalMass, currentGM);
            } else {
                printf("[B5] Slot %d already at max (40 t).\n", activeSlot);
            }
            kBrktL = true;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_RELEASE) kBrktL = false;

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS && !kBrktR) {
            if (containerWeights[activeSlot] > 0.0f) {
                containerWeights[activeSlot] = glm::max(containerWeights[activeSlot] - 5.0f, 0.0f);
                printf("[B5] Slot %d weight: %.0f t  |  Slots: [%.0f, %.0f, %.0f, %.0f] t  |  Total: %.0f t  |  GM: %.3f\n",
                    activeSlot, containerWeights[activeSlot],
                    containerWeights[0], containerWeights[1], containerWeights[2], containerWeights[3],
                    shipTotalMass, currentGM);
            } else {
                printf("[B5] Slot %d already empty (0 t).\n", activeSlot);
            }
            kBrktR = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_RELEASE) kBrktR = false;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !kC_slot) {
        activeSlot = (activeSlot + 1) % 4;
        const char* slotNames[] = {"Port-Outer", "Port-Inner", "Starb-Inner", "Starb-Outer"};
        printf("[Ship Slot] Active ship slot: %d (%s)  Weight: %.0f t  (deck yellow marker)\n",
               activeSlot, slotNames[activeSlot], containerWeights[activeSlot]);
        kC_slot = true;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) kC_slot = false;

    // Tab: cycle active YARD column for crane pickup (yellow slab indicator on yard)
    static bool kTab_yard = false;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS && !kTab_yard) {
        activeYardCol = (activeYardCol + 1) % 4;
        printf("[Yard Col] Active yard column: %d  (yellow slab indicator; V=pick from here)\n", activeYardCol);
        kTab_yard = true;
    }
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_RELEASE) kTab_yard = false;

    // --- B7 Crane: Part 1 — V key: pick up container from yard ---
    static bool kV_crane = false;
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && !kV_crane) {
        if (craneOpState == COP_IDLE) {
            if (yardPickedCol[activeYardCol]) {
                printf("[Crane] Yard column %d is inactive (top container already moved). Tab to select another column.\n", activeYardCol);
            } else if (containerWeights[activeSlot] >= 40.0f) {
                printf("[Crane] Ship slot %d is full (40 t). Press C to pick another ship slot.\n", activeSlot);
            } else {
                // Pickup column = active yard col (Tab key); delivery slot = active ship slot (C key)
                cranePickupSlot  = activeYardCol;
                craneDeliverySlot = activeSlot;
                // Compute texture of the top container in this yard column
                int topSt = yardPickedCol[cranePickupSlot] ? 0 : 1;
                cranePickupTexIdx = (0 + cranePickupSlot + topSt) % 5;
                // Pickup angle: crane base at (-3, -5); yard col X = -14 + col*1.2, z ≈ -2
                float colX = -14.0f + cranePickupSlot * 1.2f;
                cranePickupAngle = glm::degrees(atan2f(colX - (-3.0f), (-2.0f) - (-5.0f)));
                // Delivery angle: toward the selected ship slot (ship at world X=shipX, local slotX)
                float slotWorldX = shipX + containerSlotX[craneDeliverySlot];
                craneDeliveryAngle = glm::degrees(atan2f(slotWorldX - (-3.0f), -7.5f - (-5.0f)));
                craneOpState = COP_GOTO_PICKUP;
                printf("[Crane] Picking from yard col %d → ship slot %d (%.0f/40 t).\n",
                       cranePickupSlot, craneDeliverySlot, containerWeights[craneDeliverySlot]);
            }
        } else if (craneOpState == COP_WAIT_DELIVERY) {
            printf("[Crane] Container held. Press U to deliver to ship.\n");
        } else if (craneOpState == COP_COMPLETE) {
            printf("[Crane] Crane done. Change slot with C and press V again.\n");
        } else {
            printf("[Crane] Crane busy — wait for current operation to finish.\n");
        }
        kV_crane = true;
    }
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_RELEASE) kV_crane = false;

    // --- B7 Crane: Part 2 — U key: deliver container to ship slot 0 ---
    static bool kU_crane = false;
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS && !kU_crane) {
        if (craneOpState == COP_WAIT_DELIVERY) {
            craneOpState = COP_GOTO_SHIP;
            printf("[Crane] Part 2 started: swinging to ship slot 0 (port-outer, far-aft end).\n");
        } else if (craneOpState == COP_IDLE) {
            printf("[Crane] Press V first to pick up a container from the yard.\n");
        } else if (craneOpState == COP_COMPLETE) {
            printf("[Crane] Slot 0 full (40 t). Nothing to deliver.\n");
        } else {
            printf("[Crane] Crane busy — wait until Part 1 completes.\n");
        }
        kU_crane = true;
    }
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_RELEASE) kU_crane = false;

    // --- B6: Autopilot toggle (H key) ---
    static bool kH_auto = false;
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && !kH_auto) {
        if (!patrolAutopilot.enabled) {
            // Teleport patrol ship to start waypoint (open water)
            patrolAutopilot.currentT = 0.0f;
            patrolAutopilot.integral = 0.0f;
            patrolAutopilot.prevError = 0.0f;
            glm::vec3 startPos = patrolAutopilot.getSplinePoint(0.0f);
            patrolX = startPos.x;
            patrolZ = startPos.z;
            // Point heading toward next waypoint
            glm::vec3 nextPos = patrolAutopilot.getSplinePoint(0.1f);
            glm::vec3 dir = glm::normalize(nextPos - startPos);
            patrolHeading = atan2f(dir.x, dir.z) + 3.14159265f; // +PI: bow faces -Z locally
            patrolVelocityZ = 0.0f;
            patrolAngularVel = 0.0f;
            patrolAutopilot.enabled = true;
            printf("[B6] Autopilot: ENGAGED — patrol ship departing from open water.\n");
        } else {
            patrolAutopilot.enabled = false;
            patrolVelocityZ = 0.0f;
            patrolAngularVel = 0.0f;
            printf("[B6] Autopilot: DISENGAGED — patrol ship holding position.\n");
        }
        kH_auto = true;
    }
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE) kH_auto = false;

    // --- Backspace: full simulation reset (D1 sinking recovery + crane + cargo) ---
    static bool kBksp = false;
    if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS && !kBksp) {
        // Reset ship physics
        shipVelocityZ   = 0.0f;
        shipAngularVel  = 0.0f;
        shipRollVelocity = 0.0f;
        shipRoll        = 0.0f;
        shipPitchForContainers = 0.0f;
        shipPitchVelocity      = 0.0f;
        shipX           = 8.0f;
        shipZ           = 0.0f;
        shipHeading     = 0.0f;
        // Reset D1 sinking
        triggerSinking  = false;
        sinkingOffset   = 0.0f;
        sinkingPrinted  = false;
        // Reset capsize
        triggerCapsize  = false;
        capsizeTimer    = 0.0f;
        capsizePrinted  = false;
        // Reset cargo weights
        for (int i = 0; i < 4; i++) containerWeights[i] = 5.0f;
        activeSlot      = 0;
        activeYardCol   = 0;
        craneDeliverySlot = 0;
        cranePickupTexIdx = 1;
        // Reset crane state
        craneOpState    = COP_IDLE;
        craneOpDone     = false;
        craneCarrying   = false;
        craneOpTimer    = 0.0f;
        for (int i = 0; i < 4; i++) yardPickedCol[i] = false;
        craneArmAngle   = 0.0f;
        craneArmPitch   = -20.0f;
        craneCableLength = 6.0f;
        // Reset pendulum
        pendulumTheta   = 0.0f;
        pendulumOmega   = 0.0f;
        pendulumPhi     = 0.0f;
        pendulumPhiOmega = 0.0f;
        printf("[RESET] Simulation reset to initial state.\n");
        kBksp = true;
    }
    if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_RELEASE) kBksp = false;

    // --- Lighting toggles (press-once using static flags to prevent repeat) ---
    // Keys 1/2/3: toggle directional/point/spot light sources
    // Keys 5/6/7: toggle ambient/diffuse/specular lighting components
    static bool k1 = 0, k2 = 0, k3 = 0, k5 = 0, k6 = 0, k7 = 0;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !k1)
    {
        dirLightOn = !dirLightOn;
        k1 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE)
        k1 = 0;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !k2)
    {
        pointLightOn = !pointLightOn;
        k2 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE)
        k2 = 0;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !k3)
    {
        spotLightOn = !spotLightOn;
        k3 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE)
        k3 = 0;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS && !k5)
    {
        ambientOn = !ambientOn;
        k5 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_RELEASE)
        k5 = 0;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS && !k6)
    {
        diffuseOn = !diffuseOn;
        k6 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_RELEASE)
        k6 = 0;
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS && !k7)
    {
        specularOn = !specularOn;
        k7 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_RELEASE)
        k7 = 0;

    // --- Render mode toggles (M=material, N=texture, 8=blended) ---
    static bool kM = 0, kN = 0, k8 = 0, k4 = 0;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !kM)
    {
        renderMode = 0;
        cout << "Mode: Material" << endl;
        kM = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE)
        kM = 0;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !kN)
    {
        renderMode = 1;
        cout << "Mode: Texture" << endl;
        kN = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE)
        kN = 0;
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS && !k8)
    {
        renderMode = 2;
        cout << "Mode: Blended" << endl;
        k8 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_RELEASE)
        k8 = 0;

    // --- Shader toggle: key 4 switches between Phong and Gouraud ---
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !k4)
    {
        useGouraud = !useGouraud;
        cout << "Shader: " << (useGouraud ? "Gouraud" : "Phong") << endl;
        k4 = 1;
    }
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE)
        k4 = 0;
}

// ==================== GLFW CALLBACKS ====================
// mouse_callback: first-person mouse look (updates camera yaw/pitch)
void mouse_callback(GLFWwindow *w, double xIn, double yIn)
{
    float x = (float)xIn, y = (float)yIn;
    if (firstMouse)
    {
        lastX = x;
        lastY = y;
        firstMouse = false;
    }
    basic_camera.ProcessMouseMovement(x - lastX, lastY - y);
    lastX = x;
    lastY = y;
}
void framebuffer_size_callback(GLFWwindow *w, int width, int height)
{
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    glViewport(0, 0, width, height);
}                                                                                                         // Window resize
void scroll_callback(GLFWwindow *w, double xo, double yo) { basic_camera.ProcessMouseScroll((float)yo); } // Mouse wheel zoom

// ==================== SCENE DRAWING FUNCTIONS ====================
// Each function draws a complete scene object by composing multiple primitives.
// Parameters: s=active shader, p=parent transformation matrix (usually identity).

// --- drawWater: Opaque ocean surface ---
// Fully opaque (alpha=1.0) so water goes completely dark when all lights are off
void drawWater(Shader &s, glm::mat4 p)
{
    drawCube(s, p, 10, -0.35f, 0, 0, 0, 0, 70, 0.12f, 70, // Large ocean plane
             glm::vec3(0.04f, 0.12f, 0.3f), glm::vec3(0.08f, 0.25f, 0.5f), glm::vec3(0.3f, 0.4f, 0.55f), 64.0f);
    drawCube(s, p, 2, -0.28f, 0, 0, 0, 0, 6, 0.08f, 22, // Harbor channel (slightly lighter blue)
             glm::vec3(0.06f, 0.18f, 0.35f), glm::vec3(0.12f, 0.32f, 0.55f), glm::vec3(0.3f, 0.4f, 0.5f), 48.0f);
}

//drawDock — L-shaped dock (main platform + perpendicular arm along crane side)
void drawDock(Shader &s, glm::mat4 p)
{
    glm::vec3 conc(0.45f, 0.43f, 0.4f), concD(0.55f, 0.53f, 0.5f), sp(0.2f);

    // ── Main dock platform (unchanged) ──────────────────────────────────────
    drawCube(s, p, -10, 0, 0, 0, 0, 0, 22, 0.6f, 22, conc * 0.6f, concD, sp, 16);                      // Main dock platform
    drawCube(s, p, 1.0f, 0.4f, 0, 0, 0, 0, 0.4f, 0.2f, 22, glm::vec3(0.25f), glm::vec3(0.3f), sp, 16); // Dock edge (waterside)
    drawCube(s, p, -5, 0.32f, 0, 0, 0, 0, 8, 0.02f, 20, glm::vec3(0.15f), glm::vec3(0.2f), sp, 8);     // Road surface
    glm::vec3 wh(0.9f);
    for (int i = -4; i < 5; i++) // White lane markings
        drawCube(s, p, -5, 0.34f, i * 2.2f, 0, 0, 0, 0.12f, 0.02f, 0.9f, wh * 0.5f, wh, glm::vec3(0.3f), 8);
    glm::vec3 yl(0.8f, 0.7f, 0.1f);
    drawCube(s, p, -5, 0.34f, 0, 0, 0, 0, 0.08f, 0.025f, 20, yl * 0.4f, yl, glm::vec3(0.2f), 8); // Yellow center line
    // Bollards along main dock waterside edge
    for (int i = -4; i <= 4; i += 2)
    {
        glm::vec3 bolW(0.7f);
        s.setVec2("texScale", glm::vec2(1.0f, 3.0f));
        drawCylinder(s, p, 0.5f, 0.55f, i * 2.2f, 0, 0, 0, 1.2f, 2.8f, 1.2f, bolW * 0.5f, bolW, glm::vec3(0.4f), 32, glm::vec3(0), mooringBollardTex);
        s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
        glm::vec3 capW(0.75f);
        drawCylinder(s, p, 0.5f, 0.85f, i * 2.2f, 0, 0, 0, 1.8f, 0.5f, 1.8f, capW * 0.5f, capW, glm::vec3(0.4f), 32, glm::vec3(0), mooringBollardTopTex);
    }

    // ── L-arm: extends from -Z edge of main dock, going further -Z ──
    // Platform slab: centre at (-2, 0, -23), size 6 × 0.6 × 24
    // Spans X ∈ [-5, 1], Z ∈ [-35, -11] — flush with main dock at Z = -11
    drawCube(s, p, -2.0f, 0, -23.0f, 0, 0, 0, 6.0f, 0.6f, 24.0f, conc * 0.6f, concD, sp, 16); // L-arm slab

    // Edge cap strips — along the three open-water edges of the arm
    // Far -Z edge (bottom end)
    drawCube(s, p, -2.0f, 0.4f, -35.2f, 0, 0, 0, 6.0f, 0.2f, 0.4f, glm::vec3(0.25f), glm::vec3(0.3f), sp, 16);
    // +X edge (right side, open water)
    drawCube(s, p, 1.2f, 0.4f, -23.0f, 0, 0, 0, 0.4f, 0.2f, 24.0f, glm::vec3(0.25f), glm::vec3(0.3f), sp, 16);
    // -X edge (left side)
    drawCube(s, p, -5.2f, 0.4f, -23.0f, 0, 0, 0, 0.4f, 0.2f, 24.0f, glm::vec3(0.25f), glm::vec3(0.3f), sp, 16);

    // Road surface on arm (running along Z)
    drawCube(s, p, -2.0f, 0.32f, -23.0f, 0, 0, 0, 4.0f, 0.02f, 22.0f, glm::vec3(0.15f), glm::vec3(0.2f), sp, 8);

    // White lane markings on arm (perpendicular dashes along Z axis)
    for (int i = -8; i <= 2; i++)
        drawCube(s, p, -2.0f, 0.34f, -13.0f + i * 2.2f, 0, 0, 0, 0.12f, 0.02f, 0.9f, wh * 0.5f, wh, glm::vec3(0.3f), 8);

    // Yellow centre line on arm (along Z)
    drawCube(s, p, -2.0f, 0.34f, -23.0f, 0, 0, 0, 0.08f, 0.025f, 22.0f, yl * 0.4f, yl, glm::vec3(0.2f), 8);
}

//drawSeaPortBuilding
void drawSeaPortBuilding(Shader &s, glm::mat4 p)
{
    glm::vec3 wall(0.9f, 0.95f, 1.0f), wallD(0.9f, 0.95f, 1.0f), sp(0.3f);
    glm::vec3 roof(0.4f, 0.42f, 0.45f), gl(0.3f, 0.5f, 0.7f);

    // Floor 1: brick-textured walls
    drawCube(s, p, -16, 1.2f, 7, 0, 0, 0, 7, 2.4f, 7, wall * 0.5f, wallD, sp, 32, glm::vec3(0), brickTex);
    // Ledge between floor 1 and floor 2
    drawCube(s, p, -16, 2.5f, 7, 0, 0, 0, 7.2f, 0.15f, 7.2f, glm::vec3(0.3f), glm::vec3(0.35f), sp, 16);
    // Floor 2: brick-textured walls
    drawCube(s, p, -16, 3.5f, 7, 0, 0, 0, 6.5f, 1.8f, 6.5f, wall * 0.55f, wallD * 0.95f, sp, 32, glm::vec3(0), brickTex);
    // Upper ledge (between floor 2 and roof)
    drawCube(s, p, -16, 4.5f, 7, 0, 0, 0, 6.7f, 0.12f, 6.7f, glm::vec3(0.3f), glm::vec3(0.35f), sp, 16);

    // Main roof: concrete texture, texScale=(3,3) for realistic tiling
    glm::vec3 roofTex(0.85f);
    s.setVec2("texScale", glm::vec2(3.0f, 3.0f));
    drawCube(s, p, -16, 4.65f, 7, 0, 0, 0, 7, 0.3f, 7, roofTex * 0.5f, roofTex, sp, 16, glm::vec3(0), concreteRoofTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    // Control tower body (sits on main roof)
    drawCube(s, p, -16, 5.8f, 7, 0, 0, 0, 2.5f, 2.0f, 2.5f, wall * 0.5f, wallD, sp, 32);

    // Tower glass windows (4 sides)
    for (int side = 0; side < 4; side++)
    {
        float wx = (side == 0) ? -14.7f : (side == 1) ? -17.3f : -16.0f;
        float wz = (side < 2) ? 7.0f : (side == 2) ? 8.3f : 5.7f;
        float wsx = (side < 2) ? 0.05f : 2.2f, wsz = (side < 2) ? 2.2f : 0.05f;
        drawCube(s, p, wx, 5.8f, wz, 0, 0, 0, wsx, 1.2f, wsz, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    }

    // Tower roof: concrete texture
    s.setVec2("texScale", glm::vec2(3.0f, 3.0f));
    drawCube(s, p, -16, 6.95f, 7, 0, 0, 0, 2.8f, 0.2f, 2.8f, roofTex * 0.5f, roofTex, sp, 16, glm::vec3(0), concreteRoofTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    // Ground-floor windows (4 windows along one side)
    for (int i = 0; i < 4; i++)
        drawCube(s, p, -12.40f, 1.0f, 5.5f + i * 1.0f, 0, 0, 0, 0.05f, 0.9f, 0.6f, gl * 0.3f, gl, glm::vec3(0.8f), 64);

    // Building sign (red panel on front)
    drawCube(s, p, -16, 4.0f, 10.52f, 0, 0, 0, 4.5f, 0.7f, 0.1f,
             glm::vec3(0.6f, 0.05f, 0.05f), glm::vec3(0.85f, 0.1f, 0.1f), glm::vec3(0.5f), 32);

    // Fence posts along side
    for (float zz = 4; zz <= 10; zz += 1.5f)
        drawCube(s, p, -12.45f, 2.8f, zz, 0, 0, 0, 0.05f, 0.5f, 0.05f, glm::vec3(0.2f), glm::vec3(0.3f), sp, 16);
    // Fence horizontal rail
    drawCube(s, p, -12.45f, 3.0f, 7, 0, 0, 0, 0.05f, 0.05f, 6, glm::vec3(0.2f), glm::vec3(0.3f), sp, 16);
}

//drawContainerYard:
void drawContainerYard(Shader &s, glm::mat4 p)
{
    float sx = -14, sz = -2; // Starting position of container grid
    // 3 rows x 4 cols x 2 stacks — col maps to activeSlot (0..3)
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++)
        {
            // Count how many stacks are visible in this column's row 0
            // (top container of row 0 is the one the crane picks)
            for (int st = 0; st < 2; st++)
            {
                // Hide top container of row 0 in the active column once picked
                if (yardPickedCol[c] && r == 0 && st == 1) continue;

                int ci = (r + c + st) % 5;
                float cx = sx + c * 1.2f, cy = 0.55f + st * 1.1f, cz = sz - r * 1.8f;
                drawCube(s, p, cx, cy, cz, 0, 0, 0, 1.0f, 1.1f, 1.6f,
                         glm::vec3(0.3f), glm::vec3(0.5f), glm::vec3(0.15f), 12,
                         glm::vec3(0), containerTex[ci]); // Container body

                // Top slab: yellow only if this is the un-picked top of the active column.
                // Inactive (already-picked) columns never get the yellow indicator.
                bool isPickableTop = (r == 0 && st == 1 && !yardPickedCol[c]);
                bool isActive      = (c == activeYardCol);
                if (isPickableTop && isActive) {
                    // Bright yellow — this column's top container is ready to pick
                    drawCube(s, p, cx, cy + 0.57f, cz, 0, 0, 0, 1.02f, 0.04f, 1.62f,
                             glm::vec3(0.5f, 0.45f, 0.0f), glm::vec3(0.95f, 0.85f, 0.0f),
                             glm::vec3(0.6f), 64);
                } else {
                    drawCube(s, p, cx, cy + 0.57f, cz, 0, 0, 0, 1.02f, 0.04f, 1.62f,
                             glm::vec3(0.2f), glm::vec3(0.35f), glm::vec3(0.1f), 8);
                }
            }
        }
}



// --- drawMobileCrane: Large port mobile crane (matching reference image) ---
// 4 A-frame support legs with cross-bracing, rotating turret with cab, counterweight,
// long boom arm (pivots up/down with O/P), cable and hook/spreader.
// ALL components above turntable share a single hierarchical matrix (turret) so they rotate together.
void drawMobileCrane(Shader &s, glm::mat4 p)
{
    glm::vec3 bl(0.08f,0.18f,0.65f), blD(0.12f,0.28f,0.82f);    // Blue frame color
    glm::vec3 yl(0.75f,0.55f,0.08f), ylD(0.9f,0.7f,0.12f);      // Yellow/orange boom color
    glm::vec3 sp(0.4f);
    glm::vec3 dk(0.15f), dkD(0.22f);                              // Dark steel
    glm::vec3 gl(0.2f,0.35f,0.5f);                                // Glass
    glm::vec3 cw(0.5f,0.12f,0.08f);                               // Counterweight red
    float cx=-3, cz=-5;

    // === BASE PLATFORM ===
    drawCube(s,p, cx,0.25f,cz, 0,0,0, 5.0f,0.5f,5.0f, dk*0.5f,dkD,sp,16);  // Heavy steel platform

    // === 4 SUPPORT LEGS (A-frame style with cross-bracing) ===
    for(float lx : {cx-2.0f, cx+2.0f})
        for(float lz : {cz-2.0f, cz+2.0f}) {
            // Main vertical leg
            drawCube(s,p, lx,3.5f,lz, 0,0,0, 0.4f,6.5f,0.4f, bl*0.4f,blD,sp,32);
            // Foot plate
            drawCube(s,p, lx,0.05f,lz, 0,0,0, 0.9f,0.1f,0.9f, bl*0.3f,blD*0.7f,sp,16);
        }
    // Horizontal cross-beams between legs (at top of legs, front and back pairs)
    drawCube(s,p, cx,6.5f,cz-2.0f, 0,0,0, 4.4f,0.3f,0.3f, bl*0.4f,blD,sp,16);
    drawCube(s,p, cx,6.5f,cz+2.0f, 0,0,0, 4.4f,0.3f,0.3f, bl*0.4f,blD,sp,16);
    // Side cross-beams
    drawCube(s,p, cx-2.0f,6.5f,cz, 0,0,0, 0.3f,0.3f,4.4f, bl*0.4f,blD,sp,16);
    drawCube(s,p, cx+2.0f,6.5f,cz, 0,0,0, 0.3f,0.3f,4.4f, bl*0.4f,blD,sp,16);
    // Diagonal bracing (X pattern on front and back — spans base to cross-beams)
    drawCube(s,p, cx,3.5f,cz-2.0f, 0,0,25, 0.12f,6.5f,0.12f, bl*0.3f,blD*0.7f,sp,16);
    drawCube(s,p, cx,3.5f,cz-2.0f, 0,0,-25, 0.12f,6.5f,0.12f, bl*0.3f,blD*0.7f,sp,16);
    drawCube(s,p, cx,3.5f,cz+2.0f, 0,0,25, 0.12f,6.5f,0.12f, bl*0.3f,blD*0.7f,sp,16);
    drawCube(s,p, cx,3.5f,cz+2.0f, 0,0,-25, 0.12f,6.5f,0.12f, bl*0.3f,blD*0.7f,sp,16);

    // === SLEWING RING / TURNTABLE (sits right on cross-beams at top of legs) ===
    // Lower ring (large flat disk)
    drawCylinder(s,p, cx,6.6f,cz, 0,0,0, 5.0f,0.4f,5.0f, dk*0.5f,dkD,sp,32);
    // Upper ring (smaller, stacked on lower)
    drawCylinder(s,p, cx,6.7f,cz, 0,0,0, 4.2f,0.3f,4.2f, dk*0.6f,dkD*1.1f,sp,32);

    // === TURRET (everything above turntable rotates together with J/L) ===
    glm::mat4 turret = glm::translate(p, glm::vec3(cx, 6.7f, cz));
    turret = glm::rotate(turret, glm::radians(craneArmAngle), glm::vec3(0,1,0));

    // Turret base platform (wide flat slab sitting flush on slewing ring)
    drawCube(s,turret, 0,0.15f,0, 0,0,0, 4.0f,0.3f,4.0f, dk*0.5f,dkD,sp,16);

    // Machinery house body (rear section, sits on base platform)
    drawCube(s,turret, 0,1.3f,-0.8f, 0,0,0, 3.2f,1.8f,2.8f, ylD*0.35f,ylD,sp,32);
    // Machinery house roof plate (sits flush on top of yellow body)
    drawCube(s,turret, 0,2.2f,-0.8f, 0,0,0, 3.4f,0.12f,3.0f, dk*0.5f,dkD,sp,16);

    // Counterweight block (red, at rear of house)
    drawCube(s,turret, 0,0.9f,-2.6f, 0,0,0, 2.6f,1.2f,1.0f, cw*0.4f,cw,sp,16);

    // --- Operator cab (front-left, on base) ---
    drawCube(s,turret, -1.0f,1.2f,1.2f, 0,0,0, 1.3f,1.4f,1.3f, ylD*0.4f,ylD,sp,32);  // Cab body
    // Cab windows (3 sides)
    drawCube(s,turret, -1.0f,1.4f,1.86f, 0,0,0, 0.9f,0.6f,0.04f, gl*0.3f,gl,glm::vec3(0.8f),64);  // Front
    drawCube(s,turret, -1.66f,1.4f,1.2f, 0,0,0, 0.04f,0.6f,0.9f, gl*0.3f,gl,glm::vec3(0.8f),64);  // Left
    drawCube(s,turret, -0.34f,1.4f,1.2f, 0,0,0, 0.04f,0.6f,0.9f, gl*0.3f,gl,glm::vec3(0.8f),64);  // Right

    // --- Vertical mast (connects house roof up to boom pivot) ---
    drawCube(s,turret, 0,3.2f,0, 0,0,0, 0.6f,2.0f,0.6f, ylD*0.3f,ylD*0.85f,sp,32);  // Mast column
    drawCube(s,turret, 0,4.3f,0, 0,0,0, 0.8f,0.15f,0.8f, dk*0.5f,dkD,sp,16);         // Mast cap

    // --- Boom pivot axle (at top of mast) ---
    drawCylinder(s,turret, 0,4.3f,0, 0,0,90, 1.0f,0.7f,1.0f, dk*0.5f,dkD,glm::vec3(0.5f),32);

    // === BOOM ARM (pivots up/down from mast top with O/P) ===
    glm::mat4 boom = glm::translate(turret, glm::vec3(0, 4.3f, 0));
    boom = glm::rotate(boom, glm::radians(craneArmPitch), glm::vec3(1,0,0));

    // Main boom (starts right at the pivot, extends forward)
    drawCube(s,boom, 0,0.3f,5.0f, 0,0,0, 0.5f,0.5f,10.0f, ylD*0.35f,ylD,sp,32);  // Boom body
    // Boom lattice side rails
    drawCube(s,boom, 0.3f,0.3f,5.0f, 0,0,0, 0.08f,0.55f,10.0f, ylD*0.3f,ylD*0.8f,sp,16);
    drawCube(s,boom, -0.3f,0.3f,5.0f, 0,0,0, 0.08f,0.55f,10.0f, ylD*0.3f,ylD*0.8f,sp,16);
    // Boom cross-members
    for(float bz=0.5f;bz<10.0f;bz+=1.2f)
        drawCube(s,boom, 0,0.3f,bz, 0,0,0, 0.55f,0.08f,0.08f, ylD*0.25f,ylD*0.7f,sp,16);

    // Boom head (tip) — sheave housing
    drawCube(s,boom, 0,0.6f,10.3f, 0,0,0, 0.7f,0.3f,0.5f, dk*0.5f,dkD,sp,16);
    drawCylinder(s,boom, 0,0.8f,10.3f, 0,0,90, 0.5f,0.5f,0.5f, dk*0.6f,dkD,glm::vec3(0.5f),32);  // Sheave


    // --- B2: Hoist cable as Catmull-Rom spline + 2D spherical pendulum ---
    //
    // The cable bows naturally under swing. 3 control points for the CR spline:
    //   P1 = boom tip (cable attachment, in world space)
    //   P2 = midpoint displaced by half the pendulum swing (cable bows here)
    //   P3 = hook position (computed by crane_physics.cpp)
    // P0 and P4 are virtual endpoints for CR tangent continuity.

    float cableLen = craneCableLength; // matches physics; changes during lower/lift phases

    // Boom tip in boom-local space → transform to world via boom matrix
    glm::vec4 tipBoomLocal = glm::vec4(0.0f, 0.3f, 10.3f, 1.0f);
    glm::vec3 tipWorld = glm::vec3(boom * tipBoomLocal);

    // Hook world position from physics (includes both θ and φ pendulum displacements)
    glm::vec3 hookW = hookWorldPos;

    // Midpoint: halfway between tip and hook, displaced outward by pendulum swing
    // This makes the cable bow — larger swing = more bow (physically: cable inertia lag)
    glm::vec3 midBase = (tipWorld + hookW) * 0.5f;
    glm::vec3 swingDisp = (hookW - tipWorld);
    // Lateral displacement proportional to swing magnitude
    float bowFactor = 0.25f;  // how much the cable bows (0=straight, 0.5=extreme)
    glm::vec3 midpoint = midBase + glm::vec3(
        sinf(pendulumTheta) * bowFactor * cableLen,
        0.0f,
        sinf(pendulumPhi) * bowFactor * cableLen
    );

    // Virtual endpoints for Catmull-Rom tangent continuity
    glm::vec3 P0 = tipWorld + (tipWorld - midpoint);  // above tip, mirrored
    glm::vec3 P1 = tipWorld;
    glm::vec3 P2 = midpoint;
    glm::vec3 P3 = hookW;
    glm::vec3 P4 = hookW + (hookW - midpoint);        // below hook, mirrored

    // Draw cable as chain of cylinder segments along Catmull-Rom spline
    const int CABLE_SEGS = 12;
    glm::vec3 cableMaterial(0.15f);
    s.setVec3("material.ambient",  cableMaterial * 0.3f);
    s.setVec3("material.diffuse",  cableMaterial * 0.8f);
    s.setVec3("material.specular", glm::vec3(0.4f));
    s.setFloat("material.shininess", 8.0f);
    s.setBool("useTexture", false);

    glm::vec3 prevPt = P1;
    for (int i = 1; i <= CABLE_SEGS; i++) {
        float t = (float)i / CABLE_SEGS;
        glm::vec3 pt;
        if (t <= 0.5f) {
            // First half: P0,P1,P2,P3 segment, remap t to 0..1
            pt = catmullRomPoint(P0, P1, P2, P3, t * 2.0f);
        } else {
            // Second half: P1,P2,P3,P4 segment, remap t to 0..1
            pt = catmullRomPoint(P1, P2, P3, P4, (t - 0.5f) * 2.0f);
        }
        drawRopeSegment(s, p, prevPt, pt, 0.03f);
        prevPt = pt;
    }

    // --- Hook / Spreader / Container (at hook world position) ---
    // Build a model matrix at the hook position, oriented to hang vertically
    // but tilted by the pendulum angles for natural look
    glm::mat4 hookMat = glm::translate(p, hookW);
    // Tilt container slightly with pendulum — looks more natural
    hookMat = glm::rotate(hookMat, pendulumTheta * 0.3f, glm::vec3(0, 0, 1));  // lateral tilt
    hookMat = glm::rotate(hookMat, pendulumPhi * 0.3f, glm::vec3(1, 0, 0));    // longitudinal tilt

    // B2: collision flash — container turns red when hit detected
    glm::vec3 contAmb(0.3f), contDif(0.5f), contSpec(0.15f);
    // Use the texture that matches the yard column the container was picked from
    GLuint carriedTex = containerTex[cranePickupTexIdx];
    if (collisionFlashTimer > 0.0f) {
        float flash = collisionFlashTimer / 0.5f;
        contAmb = glm::mix(contAmb, glm::vec3(0.8f, 0.1f, 0.05f), flash);
        contDif = glm::mix(contDif, glm::vec3(1.0f, 0.15f, 0.08f), flash);
        carriedTex = 0;  // remove texture during collision flash so red material shows
    }

    drawCube(s, hookMat, 0,0,0, 0,0,0, 1.2f,0.15f,0.5f, ylD*0.4f,ylD,sp,16);              // Spreader bar
    drawCube(s, hookMat, 0,-0.3f,0, 0,0,0, 0.3f,0.4f,0.3f, dk*0.5f,dkD,glm::vec3(0.5f),32); // Hook block

    // Container on hook — only drawn while crane is carrying (B7 loading operation).
    // Uses the same size as the yard containers so it matches visually when it lands.
    // Nothing is drawn when the crane is idle (hook hangs empty).
    if (craneCarrying) {
        drawCube(s, hookMat, 0,-0.55f,0, 0,0,0, 1.0f,1.1f,1.6f,
                 contAmb, contDif, contSpec, 12, glm::vec3(0), carriedTex);
    }
}

// ==================== SHIPS ====================

//drawCargoShip
void drawCargoShip(Shader &s, glm::mat4 p)
{
    // B1+B3+B4: apply heading, wave heave+draft, pitch, roll to ship model matrix
    glm::mat4 sm = glm::translate(p, glm::vec3(shipX, shipHeave, shipZ));
    sm = glm::rotate(sm, shipHeading, glm::vec3(0, 1, 0)); // B3: heading (Y-rotation)
    sm = glm::rotate(sm, shipPitch + shipPitchForContainers, glm::vec3(1, 0, 0));   // combined wave pitch + cargo trim
    sm = glm::rotate(sm, shipRoll,  glm::vec3(0, 0, 1));   // roll around Z
    glm::vec3 sp(0.3f);
    glm::vec3 darkHull(0.06f, 0.06f, 0.08f);
    glm::vec3 redStripe(0.5f, 0.06f, 0.04f);
    glm::vec3 wh(0.82f, 0.8f, 0.78f);
    glm::vec3 gl(0.2f, 0.35f, 0.5f);
    glm::vec3 dk(0.5f, 0.85f, 0.2f);

    float hullW = 5.0f, hullH = 2.2f, hullL = 18.0f, hullCZ = 0.0f, hullY = 0.3f;
    float bowLen = 5.0f, bowZ = 9.0f + bowLen * 0.5f;
    // Ship hull body textured with ship_body.jpg, texScale=(16,8) for 4x smaller repeated pattern
    glm::vec3 hullFB(0.7f, 0.13f, 0.13f); // Firebrick red material for hull
    s.setVec2("texScale", glm::vec2(16.0f, 8.0f));
    drawCube(s, sm, 0, hullY, hullCZ, 0, 0, 0, hullW, hullH, hullL, hullFB * 0.3f, hullFB, sp, 16, glm::vec3(0), shipBodyTex);
    drawWedge(s, sm, 0, hullY, bowZ, 0, 0, 0, hullW, hullH, bowLen, hullFB * 0.3f, hullFB, sp, 16, glm::vec3(0), shipBodyTex);
    drawCube(s, sm, 0, hullY + 0.1f, -9.3f, 0, 0, 0, hullW * 0.88f, hullH * 0.85f, 0.8f, hullFB * 0.2f, hullFB * 0.5f, sp, 16, glm::vec3(0), shipBodyTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
    drawCube(s, sm, 0, -0.35f, 0, 0, 0, 0, hullW + 0.1f, 0.3f, hullL, redStripe * 0.4f, redStripe, sp, 16); // Waterline stripe (no texture)

    float deckY = hullY + hullH * 0.5f + 0.04f;
    drawCube(s, sm, 0, deckY, hullCZ, 0, 0, 0, hullW - 0.1f, 0.06f, hullL, dk * 0.5f, dk, sp, 8, glm::vec3(0), woodDeckTex);

    for (float side : {-(hullW * 0.5f - 0.05f), hullW * 0.5f - 0.05f})
    {
        drawCube(s, sm, side, deckY + 0.4f, hullCZ, 0, 0, 0, 0.04f, 0.04f, hullL - 1, glm::vec3(0.35f), glm::vec3(0.45f), sp, 16);
        for (float zz = -8; zz <= 8; zz += 1.5f)
            drawCube(s, sm, side, deckY + 0.2f, zz, 0, 0, 0, 0.03f, 0.4f, 0.03f, glm::vec3(0.35f), glm::vec3(0.45f), sp, 16);
    }
    for (float side : {-(hullW * 0.5f + 0.02f), hullW * 0.5f + 0.02f})
        for (float fz = -7; fz <= 7; fz += 2.5f)
        {
            drawCylinder(s, sm, side, hullY, fz, 0, 0, 90, 0.8f, 0.4f, 0.8f, glm::vec3(0.06f), glm::vec3(0.1f), sp, 8);
            drawCylinder(s, sm, side, hullY, fz, 0, 0, 90, 0.4f, 0.5f, 0.4f, glm::vec3(0.2f), glm::vec3(0.3f), sp, 16);
        }

    float bridgeZ = -1.0f;
    drawCube(s, sm, 0, deckY + 1.0f, bridgeZ, 0, 0, 0, 3.2f, 1.8f, 3.5f, wh * 0.5f, wh, sp, 32);
    drawCube(s, sm, 0, deckY + 1.0f, bridgeZ + 1.76f, 0, 0, 0, 2.8f, 0.5f, 0.04f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    drawCube(s, sm, 0, deckY + 1.0f, bridgeZ - 1.76f, 0, 0, 0, 2.8f, 0.5f, 0.04f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    for (float sx : {-1.61f, 1.61f})
        drawCube(s, sm, sx, deckY + 1.0f, bridgeZ, 0, 0, 0, 0.04f, 0.5f, 3.0f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    drawCube(s, sm, 0, deckY + 2.3f, bridgeZ, 0, 0, 0, 2.8f, 0.8f, 3.0f, wh * 0.5f, wh, sp, 32);
    drawCube(s, sm, 0, deckY + 2.3f, bridgeZ + 1.51f, 0, 0, 0, 2.4f, 0.4f, 0.04f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    for (float sx : {-1.41f, 1.41f})
        drawCube(s, sm, sx, deckY + 2.3f, bridgeZ, 0, 0, 0, 0.04f, 0.4f, 2.6f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    drawCube(s, sm, 0, deckY + 3.2f, bridgeZ, 0, 0, 0, 2.4f, 0.8f, 2.6f, wh * 0.5f, wh, sp, 32);
    drawCube(s, sm, 0, deckY + 3.2f, bridgeZ + 1.31f, 0, 0, 0, 2.0f, 0.5f, 0.04f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    for (float sx : {-1.21f, 1.21f})
        drawCube(s, sm, sx, deckY + 3.2f, bridgeZ, 0, 0, 0, 0.04f, 0.5f, 2.2f, gl * 0.3f, gl, glm::vec3(0.8f), 64);
    drawCube(s, sm, 0, deckY + 3.7f, bridgeZ, 0, 0, 0, 2.6f, 0.1f, 2.8f, wh * 0.4f, wh * 0.9f, sp, 16);
    drawCylinder(s, sm, 0, deckY + 4.5f, bridgeZ, 0, 0, 0, 0.12f, 2.5f, 0.12f, glm::vec3(0.3f), glm::vec3(0.4f), sp, 16);
    drawCube(s, sm, 0, deckY + 5.0f, bridgeZ, 0, 0, 0, 1.0f, 0.06f, 0.3f, wh * 0.4f, wh, sp, 16);
    for (float dy : {deckY + 0.6f, deckY + 1.9f, deckY + 2.8f})
        drawCube(s, sm, 0, dy, bridgeZ, 0, 0, 0, 3.3f, 0.06f, 3.6f, glm::vec3(0.25f), glm::vec3(0.35f), sp, 8);

    // Containers with textures
    float contW = 1.4f, contH = 1.3f, contD = 2.0f;
 
    for (int col = 0; col < 2; col++)
        for (int row = 0; row < 2; row++)
            for (int stk = 0; stk < 3; stk++)
            {
                int ci = (col + row + stk) % 5;
                float cx = -1.0f + col * 2.0f, cz = 4.0f + row * 2.2f, cy = deckY + 0.08f + contH * 0.5f + stk * contH;
                drawCube(s, sm, cx, cy, cz, 0, 0, 0, contW, contH, contD, glm::vec3(0.3f), glm::vec3(0.5f), glm::vec3(0.12f), 12, glm::vec3(0), containerTex[ci]);
            }
    

    drawSphere(s, sm, 0, -0.1f, 8.5f, 0, 0, 0, 0.25f, 0.25f, 0.25f,
               glm::vec3(0.4f, 0.35f, 0.05f), glm::vec3(0.6f, 0.5f, 0.1f), glm::vec3(0.5f), 32);
    drawCube(s, sm, 0, 0.15f, 8.5f, 0, 0, 0, 0.06f, 0.5f, 0.06f, glm::vec3(0.15f), glm::vec3(0.2f), sp, 16);
    drawCylinder(s, sm, 0, deckY + 1.5f, 7, 0, 0, 0, 0.1f, 3, 0.1f, glm::vec3(0.3f), glm::vec3(0.4f), sp, 16);

    // ── B5: Cargo slot visual indicators ────────────────────────────────────
    // 4 slots drawn as stacked container blocks further aft on the deck.
    // Slot X positions match containerSlotX[] (ship-local, transverse).
    // Each 5-ton unit = 1 container block drawn with a random container texture.
    // Active slot: bright yellow base plate.
    // Slot layout (ship-local): 0=port-outer(-1.8), 1=port-inner(-0.6),
    //                           2=starb-inner(+0.6), 3=starb-outer(+1.8)
    // All placed at Z = -6.0 (well aft, behind the static containers).
    {
        const float slotZ      = -6.0f;   // Z on deck (well aft of bridge)
        const float contW = 1.0f;   // original slot container size (1.2 unit slot pitch — no overlap)
        const float contH = 1.1f;
        const float contD = 1.6f;
        const float markerH    = 0.08f;   // base plate height
        const float maxWeight  = 40.0f;
        const float baseY      = deckY + 0.04f; // sit on deck

        // Yellow for active slot highlight
        glm::vec3 activeYellow(0.9f, 0.85f, 0.0f);
        glm::vec3 activeYellowAmb(0.4f, 0.38f, 0.0f);

        for (int i = 0; i < 4; i++) {
            float spacing = 1.05f;
            float sx = containerSlotX[i] * spacing;

            float wt  = containerWeights[i];  // current weight

            // Base marker plate (always visible — shows slot boundary)
            if (i == activeSlot) {
                drawCube(s, sm, sx, baseY + markerH * 0.5f, slotZ,
                         0, 0, 0, contW, markerH, contD,
                         activeYellowAmb, activeYellow, glm::vec3(0.6f), 64);
            } else {
                drawCube(s, sm, sx, baseY + markerH * 0.5f, slotZ,
                         0, 0, 0, contW, markerH, contD,
                         glm::vec3(0.15f), glm::vec3(0.25f), glm::vec3(0.2f), 16);
            }

            // Draw individual container blocks (one per 5-ton unit of weight)
            // Each container uses a different random texture from containerTex[0..4]
            int numContainers = glm::min((int)(wt / 5.0f), 5); // 5 tons = 1 container
            for (int stk = 0; stk < numContainers; stk++) {
        
                float cy = baseY + markerH + contH * 0.5f + stk * contH;
                // Pick a pseudo-random texture based on slot+stack index
                int texIdx = (i * 7 + stk * 3 + 1) % 5;
                drawCube(s, sm, sx, cy, slotZ,
                         0, 0, 0, contW, contH, contD,
                         glm::vec3(0.3f), glm::vec3(0.5f), glm::vec3(0.12f), 12,
                         glm::vec3(0), containerTex[texIdx]);
            }
        }
    }
}

//drawLighthouse
void drawLighthouse(Shader &s, glm::mat4 p)
{
    float lx = -20, lz = -8;
    glm::vec3 wh(0.88f), rd(0.72f, 0.12f, 0.08f), sp(0.3f);

    // Rock/stone base mound
    drawCylinder(s, p, lx, -0.1f, lz, 0, 0, 0, 10, 0.8f, 10, glm::vec3(0.35f, 0.3f, 0.25f), glm::vec3(0.45f, 0.4f, 0.35f), sp, 16);

    // 8 alternating red/white striped bands (tapering upward)
    for (int i = 0; i < 8; i++)
    {
        float y = 0.6f + i * 0.9f, sc = 2.5f - i * 0.12f;
        glm::vec3 col = (i % 2 == 0) ? rd : wh;
        drawCylinder(s, p, lx, y, lz, 0, 0, 0, sc, 4.5f, sc, col * 0.4f, col, sp, 32);
    }

    // Lantern room glass cylinder (sits directly on top of last stripe band)
    drawCylinder(s, p, lx, 7.5f, lz, 0, 0, 0, 2.2f, 3.0f, 2.2f, glm::vec3(0.08f), glm::vec3(0.18f, 0.18f, 0.1f), glm::vec3(0.8f), 64);

    // Disco ball light (sphere inside lantern room, touching cylinder top)
    drawSphere(s, p, lx, 7.5f, lz, 0, 0, 0, 1.2f, 1.2f, 1.2f,
               glm::vec3(1, 0.9f, 0.5f), glm::vec3(1, 0.95f, 0.6f), glm::vec3(1), 64, glm::vec3(0.0f), discoBallTex);

    // Cone roof with cone.jpg texture - texScale=(2,1) + MIRRORED_REPEAT for seamless wrap
    glm::vec3 coneW(0.9f, 0.1f, 0.1f);
    s.setVec2("texScale", glm::vec2(2.0f, 1.0f));
    drawCone(s, p, lx, 8.8f, lz, 0, 0, 0, 2.5f, 2.5f, 2.5f, coneW * 0.5f, coneW, sp, 32, glm::vec3(0), coneTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
   
}

// --- drawBuoys: 7 navigation buoys scattered in the water ---
// Each buoy: flat base -> spherical body (buoy texture) -> cylindrical pole -> small top sphere
void drawBuoys(Shader &s, glm::mat4 p)
{
    glm::vec3 rd(0.75f, 0.08f, 0.04f), sp(0.3f);
    float bp[][2] = {{18, 6}, {26, -3}, {29, -12}, {30, -25}, {14, 9}, {5, 5}, {30, -5}};
    for (int i = 0; i < 7; i++)
    {
        float bx = bp[i][0], bz = bp[i][1];
        // B1: buoy parent transform with wave heave + tilt
        glm::mat4 bm = glm::translate(p, glm::vec3(bx, buoyHeave[i], bz));
        bm = glm::rotate(bm, buoyTiltX[i], glm::vec3(1, 0, 0));
        bm = glm::rotate(bm, buoyTiltZ[i], glm::vec3(0, 0, 1));

        drawCube(s, bm, 0, -0.2f, 0, 0, 0, 0, 0.6f, 0.2f, 0.6f, glm::vec3(0.1f, 0.2f, 0.4f), glm::vec3(0.15f, 0.3f, 0.55f), sp, 32);
        // Buoy body - use texScale=(2,1) with MIRRORED_REPEAT for seamless mirrored UV
        s.setVec2("texScale", glm::vec2(2.0f, 1.0f));
        drawSphere(s, bm, 0, 0.15f, 0, 0, 0, 0, 0.65f, 0.5f, 0.65f, glm::vec3(0.5f), glm::vec3(0.8f), sp, 32, glm::vec3(0), bouyTex);
        s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
        drawCylinder(s, bm, 0, 0.55f, 0, 0, 0, 0, 0.18f, 2, 0.18f, rd * 0.3f, rd * 0.7f, sp, 16);
        drawSphere(s, bm, 0, 0.85f, 0, 0, 0, 0, 0.22f, 0.22f, 0.22f, rd * 0.4f, rd, sp, 32);
    }
}

// ==================== CURVES: A1 — CATMULL-ROM MOORING ROPES ====================

// catmullRomPoint: evaluates a point on the Catmull-Rom spline at parameter t in [0,1].
// The curve passes through P1 and P2. P0 and P3 are neighbor points used only for tangents.
glm::vec3 catmullRomPoint(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t)
{
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * (
        (2.0f * P1) +
        (-P0 + P2) * t +
        (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * t2 +
        (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * t3
    );
}

// drawRopeSegment: draws one smooth cylinder segment between two world-space points.
// Cylinder local radius=0.2, local height=0.2 (from createCylinder(0.2,0.2,...)).
// Scale: x/z = thickness*2.5 → world radius = thickness/2; y = len*5 → world height = len.
void drawRopeSegment(Shader &s, glm::mat4 p, glm::vec3 a, glm::vec3 b, float thickness)
{
    glm::vec3 dir = b - a;
    float len = glm::length(dir);
    if (len < 0.001f) return;
    dir = glm::normalize(dir);

    glm::mat4 model = glm::translate(p, (a + b) * 0.5f);

    // Rotate local Y-axis to align with segment direction
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    float d = glm::dot(up, dir);
    if (d < -0.9999f)
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(1, 0, 0));
    else if (d < 0.9999f)
    {
        glm::vec3 axis = glm::normalize(glm::cross(up, dir));
        model = glm::rotate(model, acosf(d), axis);
    }
    // cylVAO local radius=0.2 → sx=thickness*2.5 gives world radius=thickness/2
    // cylVAO local height=0.2 → sy=len*5       gives world height=len
    model = glm::scale(model, glm::vec3(thickness * 2.5f, len * 5.0f, thickness * 2.5f));

    glm::vec3 ropeC(0.38f, 0.28f, 0.14f); // Hemp rope brown
    setMaterial(s, ropeC * 0.3f, ropeC, glm::vec3(0.15f), 16.0f, glm::vec3(0));
    s.setBool("useTexture", false);
    s.setMat4("model", model);
    glBindVertexArray(cylVAO);
    glDrawElements(GL_TRIANGLES, cylIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// drawMooringRopes: draws 3 static ropes using Catmull-Rom splines.
// Each rope: 1.5-turn bollard hitch helix + CR spline from bollard → ship cleat.
// CR layout: P0(phantom)—P1(bollard tangent)—P2(sagged mid)—P3(cleat)—P4(phantom)
void drawMooringRopes(Shader &s, glm::mat4 p)
{
    const float bollardX   = 0.5f;
    const float postRadius = 0.28f; // world radius 0.24 + small clearance
    const float wrapY      = 0.62f; // wrap height, just below bollard cap
    const float baseSag = 0.75f;
    const int   steps   = 24;

    // B1+B3: Build ship transform to convert cleat local positions to world space
    // (matches drawCargoShip model matrix: translate + heading + pitch + roll)
    glm::mat4 shipMat = glm::translate(glm::mat4(1.0f), glm::vec3(shipX, shipHeave, shipZ));
    shipMat = glm::rotate(shipMat, shipHeading, glm::vec3(0, 1, 0)); // B3: heading
    shipMat = glm::rotate(shipMat, shipPitch + shipPitchForContainers, glm::vec3(1, 0, 0));  // combined wave pitch + cargo trim
    shipMat = glm::rotate(shipMat, shipRoll,  glm::vec3(0, 0, 1));

    float offsets[] = { -4.4f, 0.0f, 4.4f };

    for (int r = 0; r < 3; r++)
    {
        float bz = offsets[r];

        // ── 1. Bollard hitch: 1.5-turn helix around post ─────────────────────────
        {
            const int   hitchN = 60;
            const float maxTh  = 1.5f * 2.0f * PI;
            const float rise   = 0.14f;
            vector<glm::vec3> hpts;
            for (int i = 0; i <= hitchN; i++)
            {
                float th = maxTh * (float)i / hitchN;
                hpts.push_back(glm::vec3(
                    bollardX + postRadius * cosf(th),
                    wrapY    + rise * (th / maxTh),
                    bz       + postRadius * sinf(th)));
            }
            for (int i = 0; i + 1 < (int)hpts.size(); i++)
                drawRopeSegment(s, p, hpts[i], hpts[i + 1], 0.04f);
        }

        // ── 2. CR spline: bollard tangent point → sagged mid → ship cleat ────────
        glm::vec3 P1(bollardX + postRadius, wrapY, bz);

        // B1: cleat in ship-local space (port side of hull, hullW/2 = 2.5)
        // Old world cleatX=5.5 = shipX(8) - 2.5, cleatY=1.4, Z=offset along hull
        glm::vec3 cleatShipLocal(-2.5f, 1.4f, offsets[r]);
        glm::vec3 P3 = glm::vec3(shipMat * glm::vec4(cleatShipLocal, 1.0f));

        // B1: dynamic sag — rope gets taut when stretched, slack when compressed
        float ropeLen3D = glm::length(P3 - P1);
        float restLen   = 6.0f; // approximate rest length of rope
        float slackFactor = glm::clamp(1.0f - (ropeLen3D / restLen - 1.0f) * 2.0f, 0.3f, 1.5f);
        float sag = baseSag * slackFactor;

        glm::vec3 mid = (P1 + P3) * 0.5f;
        glm::vec3 P2(mid.x, mid.y - sag, mid.z);
        glm::vec3 P0 = P1 - (P2 - P1);
        glm::vec3 P4 = P3 + (P3 - P2);

        vector<glm::vec3> pts;
        for (int i = 0; i <= steps; i++)
            pts.push_back(catmullRomPoint(P0, P1, P2, P3, (float)i / steps));
        for (int i = 1; i <= steps; i++)
            pts.push_back(catmullRomPoint(P1, P2, P3, P4, (float)i / steps));

        int totalSegs = (int)pts.size() - 1;
        for (int i = 0; i < totalSegs; i++)
            drawRopeSegment(s, p, pts[i], pts[i + 1], 0.045f);
    }
}

// ==================== A2: ANCIENT BOAT — BEZIER LOFTED HULL ====================
// Hull is a lofted surface: Nz cross-section rings swept along the boat's Z axis.
// Width profile W(t): 1D cubic Bezier (t=0=bow, t=1=stern) — teardrop top-view outline.
// Cross-section at each station: cubic Bezier in XY from port gunwale → keel → starboard.
// Raised prow: gunwale and keel both lift at the bow tip (t < 0.28).
// Outward normal at each vertex: dP/ds × dP/dz = (dBy/ds, -dBx/ds, 0).

// Generic cubic Bezier point evaluator (reused by future curve objects).
glm::vec3 cubicBezierPoint(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t)
{
    float u = 1.0f - t;
    return u*u*u*P0 + 3.0f*u*u*t*P1 + 3.0f*u*t*t*P2 + t*t*t*P3;
}

// Cubic Bezier tangent (derivative w.r.t. t) — used for normal computation.
glm::vec3 cubicBezierTangent(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t)
{
    float u = 1.0f - t;
    return 3.0f * (u*u*(P1-P0) + 2.0f*u*t*(P2-P1) + t*t*(P3-P2));
}

void generateAncientBoatMesh()
{
    const int   Nz      = 40;     // stations along length (0=bow, Nz=stern)
    const int   Nc      = 24;     // cross-section points (0=port gunwale, Nc=starboard)
    const float halfLen = 3.0f;   // boat half-length; z: -3 (bow) to +3 (stern)
    const float gY      = 0.6f;   // base gunwale height
    const float bKeel   = -0.3f;  // Bezier control Y for keel depth
    const float thick   = 0.03f;  // hull wall thickness (reduced — was 0.06)

    const float wcp[4] = { 0.0f, 0.95f, 0.95f, 0.18f };
    auto halfWidth = [&](float t) -> float {
        float u = 1.0f - t;
        return u*u*u*wcp[0] + 3.0f*u*u*t*wcp[1] + 3.0f*u*t*t*wcp[2] + t*t*t*wcp[3];
    };

    int ringSize   = Nc + 1;
    int totalRings = Nz + 1;

    // Pre-compute outer positions and normals for use in rims and caps
    vector<vector<glm::vec3>> oPos(totalRings, vector<glm::vec3>(ringSize));
    vector<vector<glm::vec3>> oNrm(totalRings, vector<glm::vec3>(ringSize));
    vector<vector<glm::vec3>> iPos(totalRings, vector<glm::vec3>(ringSize));

    vector<float>        verts;
    vector<unsigned int> inds;

    // ── Outer surface ────────────────────────────────────────────────────────
    // Vertex [iz][ic] = iz*ringSize + ic
    for (int iz = 0; iz <= Nz; iz++) {
        float tz = (float)iz / Nz;
        float z  = -halfLen + 2.0f * halfLen * tz;
        float W  = halfWidth(tz);

        float prow = 0.0f, keelRise = 0.0f;
        if (tz < 0.28f) {
            float f  = 1.0f - tz / 0.28f;
            prow     = 0.50f * f * f;
            keelRise = 0.28f * f * f;
        }
        float gunwaleY = gY + prow;
        float keelCtrl = bKeel + keelRise;

        glm::vec3 csP0(-W,        gunwaleY, 0.0f);
        glm::vec3 csP1(-W * 0.6f, keelCtrl, 0.0f);
        glm::vec3 csP2( W * 0.6f, keelCtrl, 0.0f);
        glm::vec3 csP3( W,        gunwaleY, 0.0f);

        for (int ic = 0; ic <= Nc; ic++) {
            float s    = (float)ic / Nc;
            glm::vec3 pt   = cubicBezierPoint(csP0, csP1, csP2, csP3, s);
            glm::vec3 tang = cubicBezierTangent(csP0, csP1, csP2, csP3, s);

            glm::vec3 n(tang.y, -tang.x, 0.0f);
            float nlen = glm::length(n);
            if (nlen > 0.001f) n /= nlen; else n = glm::vec3(0.0f, -1.0f, 0.0f);

            oPos[iz][ic] = glm::vec3(pt.x, pt.y, z);
            oNrm[iz][ic] = n;
            // Fade thickness to 0 near bow (W→0) to prevent degenerate inner positions
            float effThick = thick * glm::min(1.0f, W / 0.08f);
            iPos[iz][ic] = oPos[iz][ic] - effThick * n;

            verts.insert(verts.end(), { pt.x, pt.y, z, n.x, n.y, n.z, s, tz });
        }
    }

    // Outer quad indices
    for (int iz = 0; iz < Nz; iz++)
        for (int ic = 0; ic < Nc; ic++) {
            unsigned int c  = iz * ringSize + ic;
            unsigned int nx = (iz + 1) * ringSize + ic;
            inds.push_back(c);     inds.push_back(nx);     inds.push_back(c + 1);
            inds.push_back(c + 1); inds.push_back(nx);     inds.push_back(nx + 1);
        }

    // ── Inner surface ────────────────────────────────────────────────────────
    // Base vertex index = totalRings*ringSize
    int innerBase = totalRings * ringSize;
    for (int iz = 0; iz <= Nz; iz++) {
        float tz = (float)iz / Nz;
        for (int ic = 0; ic <= Nc; ic++) {
            glm::vec3 ip   = iPos[iz][ic];
            glm::vec3 in_n = -oNrm[iz][ic]; // inward normal
            float s = (float)ic / Nc;
            verts.insert(verts.end(), { ip.x, ip.y, ip.z, in_n.x, in_n.y, in_n.z, s, tz });
        }
    }

    // Inner quad indices (reversed winding so inside face is visible)
    for (int iz = 0; iz < Nz; iz++)
        for (int ic = 0; ic < Nc; ic++) {
            unsigned int c  = innerBase + iz * ringSize + ic;
            unsigned int nx = innerBase + (iz + 1) * ringSize + ic;
            inds.push_back(c + 1); inds.push_back(nx);     inds.push_back(c);
            inds.push_back(nx + 1); inds.push_back(nx);    inds.push_back(c + 1);
        }

    // ── Port gunwale rim (ic=0, outer→inner along length) ───────────────────
    int portRimBase = (int)(verts.size() / 8);
    for (int iz = 0; iz <= Nz; iz++) {
        float tz = (float)iz / Nz;
        glm::vec3 on = oNrm[iz][0];
        glm::vec3 op = oPos[iz][0];
        glm::vec3 ip = iPos[iz][0];
        verts.insert(verts.end(), { op.x, op.y, op.z, on.x, on.y, on.z, 0.0f, tz });
        verts.insert(verts.end(), { ip.x, ip.y, ip.z, on.x, on.y, on.z, 1.0f, tz });
    }
    for (int iz = 0; iz < Nz; iz++) {
        int b = portRimBase + iz * 2;
        inds.push_back(b);   inds.push_back(b + 2); inds.push_back(b + 1);
        inds.push_back(b+1); inds.push_back(b + 2); inds.push_back(b + 3);
    }

    // ── Starboard gunwale rim (ic=Nc, outer→inner along length) ─────────────
    int stbdRimBase = (int)(verts.size() / 8);
    for (int iz = 0; iz <= Nz; iz++) {
        float tz = (float)iz / Nz;
        glm::vec3 on = oNrm[iz][Nc];
        glm::vec3 op = oPos[iz][Nc];
        glm::vec3 ip = iPos[iz][Nc];
        verts.insert(verts.end(), { op.x, op.y, op.z, on.x, on.y, on.z, 0.0f, tz });
        verts.insert(verts.end(), { ip.x, ip.y, ip.z, on.x, on.y, on.z, 1.0f, tz });
    }
    for (int iz = 0; iz < Nz; iz++) {
        int b = stbdRimBase + iz * 2;
        // Reversed winding for starboard rim
        inds.push_back(b + 1); inds.push_back(b + 2); inds.push_back(b);
        inds.push_back(b + 3); inds.push_back(b + 2); inds.push_back(b + 1);
    }

    // ── Stern cap (iz=Nz, faces +Z) ─────────────────────────────────────────
    int sternBase = (int)(verts.size() / 8);
    for (int ic = 0; ic <= Nc; ic++) {
        float s = (float)ic / Nc;
        glm::vec3 op = oPos[Nz][ic];
        glm::vec3 ip = iPos[Nz][ic];
        verts.insert(verts.end(), { op.x, op.y, op.z, 0.0f, 0.0f, 1.0f, s, 0.0f });
        verts.insert(verts.end(), { ip.x, ip.y, ip.z, 0.0f, 0.0f, 1.0f, s, 1.0f });
    }
    for (int ic = 0; ic < Nc; ic++) {
        int b = sternBase + ic * 2;
        inds.push_back(b);     inds.push_back(b + 2); inds.push_back(b + 1);
        inds.push_back(b + 1); inds.push_back(b + 2); inds.push_back(b + 3);
    }

    // ── Stern transom (fill inner stern ring with centroid fan) ──────────────
    // The stern cap fills the hull-thickness annular strip (outer→inner).
    // This transom fills the INTERIOR of the inner stern ring (the full cross-section face).
    // Fan winding: (cen, inner[ic], inner[ic+1]) verified CCW from +Z → normal +Z.
    {
        glm::vec3 cen(0.0f);
        for (int ic = 0; ic <= Nc; ic++) cen += iPos[Nz][ic];
        cen /= (float)(Nc + 1);

        // New vertices: inner ring points + centroid, all with +Z normal
        int transomBase = (int)(verts.size() / 8);
        for (int ic = 0; ic <= Nc; ic++) {
            glm::vec3 ip = iPos[Nz][ic];
            float s = (float)ic / Nc;
            verts.insert(verts.end(), { ip.x, ip.y, ip.z, 0.0f, 0.0f, 1.0f, s, 0.0f });
        }
        int cenV = (int)(verts.size() / 8);
        verts.insert(verts.end(), { cen.x, cen.y, cen.z, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f });

        // Fan: covers U-curve bottom and sides
        for (int ic = 0; ic < Nc; ic++) {
            inds.push_back(cenV);
            inds.push_back(transomBase + ic);
            inds.push_back(transomBase + ic + 1);
        }
        // Close top: centroid → starboard gunwale → port gunwale
        inds.push_back(cenV);
        inds.push_back(transomBase + Nc);
        inds.push_back(transomBase);
    }

    // Bow (iz=0): W=0, so outer and inner converge to the same point (effThick=0 there).
    // The outer hull naturally closes to a point at the bow; no cap needed.

    ancientBoatIndexCount = (int)inds.size();
    setupVAO8(ancientBoatVAO, ancientBoatVBO, ancientBoatEBO,
              verts.data(), (int)(verts.size() * sizeof(float)),
              inds.data(),  (int)(inds.size()  * sizeof(unsigned int)));
}

// ── Oar mesh — 2D outline extruded along Z axis ─────────────────────────────
// Step 1: define a CCW polygon outline of the oar shape in XY (X=length, Y=width)
// Step 2: extrude by oarThick in Z → front + back faces + perimeter edge quads
void generateOarMesh()
{
    const float oarLen   = 2.5f;    // oar length (X axis: 0=grip, oarLen=blade tip)
    const float gripW    = 0.018f;  // handle half-width
    const float bladeW   = 0.14f;   // blade half-width (wider paddle)
    const float oarThick = 0.012f;  // extrusion depth (Z) — very flat paddle
    const float shaftLen = 1.55f;   // X where shaft starts widening into blade
    const int   nBez     = 10;      // Bezier samples for top/bottom edges
    const int   nCap     = 6;       // semicircle points per end cap

    // ── Build 2D CCW outline in XY plane ──────────────────────────────────────
    // Order: grip cap (left semicircle) → top edge → blade tip cap → bottom edge
    vector<glm::vec2> pts;

    // 1. Grip cap: from (0,-gripW) going LEFT (CCW) to (0,+gripW)
    //    Angle from -PI/2 decreasing to -3PI/2 (≡ +PI/2)
    for (int i = 0; i <= nCap; i++) {
        float a = -PI/2.0f - PI*(float)i/nCap;
        pts.push_back({ gripW*cosf(a), gripW*sinf(a) });
    }

    // 2. Top edge (Bezier): (0,+gripW) → shoulder → (oarLen,+bladeW)
    {
        glm::vec2 P0(0.0f, gripW), P1(shaftLen, gripW),
                  P2(shaftLen+0.3f, bladeW*0.75f), P3(oarLen, bladeW);
        for (int i = 1; i <= nBez; i++) {
            float t = (float)i/nBez, u = 1.0f-t;
            pts.push_back(u*u*u*P0 + 3.0f*u*u*t*P1 + 3.0f*u*t*t*P2 + t*t*t*P3);
        }
    }

    // 3. Blade tip cap: from (oarLen,+bladeW) going RIGHT (CCW) to (oarLen,-bladeW)
    //    Slightly flattened ellipse tip (scale x by 0.35)
    for (int i = 0; i <= nCap; i++) {
        float a = PI/2.0f - PI*(float)i/nCap;
        pts.push_back({ oarLen + bladeW*0.35f*cosf(a), bladeW*sinf(a) });
    }

    // 4. Bottom edge (Bezier): (oarLen,-bladeW) → shoulder → (0,-gripW)
    {
        glm::vec2 P0(oarLen,-bladeW), P1(shaftLen+0.3f,-bladeW*0.75f),
                  P2(shaftLen,-gripW), P3(0.0f,-gripW);
        for (int i = 1; i <= nBez; i++) {
            float t = (float)i/nBez, u = 1.0f-t;
            pts.push_back(u*u*u*P0 + 3.0f*u*u*t*P1 + 3.0f*u*t*t*P2 + t*t*t*P3);
        }
    }
    // pts.back() is (0,-gripW), closing back to the grip cap start

    int N  = (int)pts.size();
    float zt = oarThick * 0.5f;
    float zb = -oarThick * 0.5f;

    // Centroid for face fans
    glm::vec2 cen(0.0f);
    for (auto& p : pts) cen += p;
    cen /= (float)N;

    vector<float>        verts;
    vector<unsigned int> inds;

    // ── Front face (z=zt, normal +Z) ─────────────────────────────────────────
    // Vertices 0..N-1 = outline; N = centroid
    for (int i = 0; i < N; i++)
        verts.insert(verts.end(), { pts[i].x, pts[i].y, zt,
                                    0.0f, 0.0f, 1.0f,
                                    pts[i].x/oarLen, 0.5f + pts[i].y/(bladeW*2.0f) });
    int fCen = N;
    verts.insert(verts.end(), { cen.x, cen.y, zt, 0.0f, 0.0f, 1.0f, cen.x/oarLen, 0.5f });
    // Fan: CCW (outline was built CCW)
    for (int i = 0; i < N; i++) {
        int nxt = (i+1)%N;
        inds.push_back(fCen); inds.push_back(i); inds.push_back(nxt);
    }

    // ── Back face (z=zb, normal -Z) ──────────────────────────────────────────
    // Vertices N+1 .. 2N = outline; 2N+1 = centroid
    int bBase = N + 1;
    for (int i = 0; i < N; i++)
        verts.insert(verts.end(), { pts[i].x, pts[i].y, zb,
                                    0.0f, 0.0f, -1.0f,
                                    pts[i].x/oarLen, 0.5f + pts[i].y/(bladeW*2.0f) });
    int bCen = bBase + N;
    verts.insert(verts.end(), { cen.x, cen.y, zb, 0.0f, 0.0f, -1.0f, cen.x/oarLen, 0.5f });
    // Fan: reversed winding for back face
    for (int i = 0; i < N; i++) {
        int nxt = (i+1)%N;
        inds.push_back(bCen); inds.push_back(bBase+nxt); inds.push_back(bBase+i);
    }

    // ── Perimeter edge quads (connect front outline to back outline) ───────────
    // Each edge: 4 unique vertices with outward XY normal; winding CCW from outside
    int edgeBase = (int)(verts.size() / 8);
    for (int i = 0; i < N; i++) {
        int nxt = (i+1)%N;
        glm::vec2 a = pts[i], b = pts[nxt];
        glm::vec2 dir = b - a;
        glm::vec2 en(-dir.y, dir.x); // left-normal = outward for CCW polygon
        float enlen = glm::length(en);
        if (enlen > 1e-6f) en /= enlen;

        int vb = edgeBase + i * 4;
        // front-a, front-b, back-a, back-b
        verts.insert(verts.end(), { a.x, a.y, zt, en.x, en.y, 0.0f, 0.0f, 0.0f });
        verts.insert(verts.end(), { b.x, b.y, zt, en.x, en.y, 0.0f, 1.0f, 0.0f });
        verts.insert(verts.end(), { a.x, a.y, zb, en.x, en.y, 0.0f, 0.0f, 1.0f });
        verts.insert(verts.end(), { b.x, b.y, zb, en.x, en.y, 0.0f, 1.0f, 1.0f });
        // CCW when viewed from outside: (front-a, front-b, back-b) + (front-a, back-b, back-a)
        inds.push_back(vb);   inds.push_back(vb+1); inds.push_back(vb+3);
        inds.push_back(vb);   inds.push_back(vb+3); inds.push_back(vb+2);
    }

    oarIndexCount = (int)inds.size();
    setupVAO8(oarVAO, oarVBO, oarEBO,
              verts.data(), (int)(verts.size() * sizeof(float)),
              inds.data(),  (int)(inds.size()  * sizeof(unsigned int)));
}

void drawAncientBoat(Shader &s, glm::mat4 p)
{
    // B1: position, heading, and wave response driven by wave_physics
    glm::mat4 am = glm::translate(p, glm::vec3(boatX, -0.20f + boatHeave, boatZ));
    am = glm::rotate(am, boatYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    am = glm::rotate(am, boatPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    am = glm::rotate(am, boatRoll,  glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 woodA(0.42f, 0.26f, 0.09f), woodD(0.58f, 0.36f, 0.14f), sp(0.12f);

    // ── Hull (double-walled Bezier lofted mesh with caps and rims) ────────────
    setMaterial(s, woodA * 0.4f, woodD, sp, 16.0f, glm::vec3(0.0f));
    bindTex(s, ancientBoatTex);
    s.setMat4("model", am);
    glBindVertexArray(ancientBoatVAO);
    glDrawElements(GL_TRIANGLES, ancientBoatIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // ── Thwarts / seats — 3 planks sized to interior hull width ──────────────
    // Widths reduced to fit inside hull (interior half-width - hull thickness - margin)
    //   tz=0.30 → W≈0.603 → interior ≈ 0.90
    //   tz=0.55 → W≈0.735 → interior ≈ 1.10
    //   tz=0.75 → W≈0.610 → interior ≈ 0.92
    const float thZ[3] = { -1.2f,  0.3f,  1.5f };
    const float thW[3] = {  0.90f, 1.10f, 0.92f };
    glm::vec3 tw(0.50f, 0.30f, 0.10f);
    s.setBool("useTexture", false);
    for (int i = 0; i < 3; i++)
        drawCube(s, am, 0.0f, 0.22f, thZ[i], 0, 0, 0,
                 thW[i], 0.04f, 0.14f,
                 tw * 0.4f, tw, sp, 16.0f, glm::vec3(0.0f), ancientBoatTex);

    // ── Oarlocks — two vertical thole pins per side forming a U-bracket ─────
    // The oar shaft sits between the pins at the gunwale pivot point
    const float L_IN_OAR = 0.75f;   // inboard handle length (must match wave_physics.cpp)
    glm::vec3 metalA(0.25f, 0.25f, 0.25f), metalD(0.45f, 0.42f, 0.38f);
    s.setBool("useTexture", false);
    for (int side = 0; side < 2; side++) {
        float sx = (side == 0) ? -1.0f : 1.0f;
        float pinGap = 0.035f;   // half-gap between pins (oar sits between)
        // Two thole pins straddling the oar at oarlock Z position
        drawCylinder(s, am, sx * 0.65f, 0.50f, 0.3f - pinGap, 0, 0, 0,
                     0.020f, 0.10f, 0.020f,
                     metalA, metalD, sp, 32.0f);
        drawCylinder(s, am, sx * 0.65f, 0.50f, 0.3f + pinGap, 0, 0, 0,
                     0.020f, 0.10f, 0.020f,
                     metalA, metalD, sp, 32.0f);
    }

    // ── Oars — physics-driven lever rotation with inboard handle visible ────
    // Oar mesh: X=0 (grip) → X=2.5 (blade tip)
    // Pivot at X=L_IN from grip → translate mesh by -L_IN so pivot aligns with oarlock
    // Result: handle (0.75 units) extends inboard, blade (1.75 units) extends outboard
    //
    // Transform pipeline (GL applies last-in-code first-to-vertex):
    //   1. Translate(-L_IN, 0, 0)  — shift pivot to origin
    //   2. Port: RotateY(180°)     — mirror so blade extends to port side
    //   3. RotateX(90°)            — lay oar flat (paddle face visible from above)
    //   4. RotateZ(dip)            — blade dip into water (phase-animated)
    //   5. RotateY(sweep)          — forward/backward sweep (physics-driven θ)
    //   6. Translate(oarlockPos)   — place at oarlock on boat hull
    glm::vec3 oarC(0.38f, 0.22f, 0.07f);
    setMaterial(s, oarC * 0.3f, oarC, sp, 8.0f, glm::vec3(0.0f));
    s.setBool("useTexture", false);
    glBindVertexArray(oarVAO);
    for (int oarIdx = 0; oarIdx < 2; oarIdx++)
    {
        float sideSign = (oarIdx == 0) ? -1.0f : 1.0f;  // -1=port, +1=starboard
        float phase = (oarIdx == 0) ? leftOarPhase : rightOarPhase;
        float theta = (oarIdx == 0) ? leftOarTheta : rightOarTheta;

        // Dip angle from stroke phase (animation-driven blade entry/exit)
        float dip;
        if (phase <= 0.0f)      dip = 0.0f;
        else if (phase < 0.12f) dip = -28.0f * (phase / 0.12f);
        else if (phase < 0.60f) dip = -28.0f;
        else if (phase < 0.75f) dip = -28.0f * (1.0f - (phase - 0.60f) / 0.15f);
        else                    dip = 0.0f;

        // Sweep angle from physics θ (negate for port to correct mirror direction)
        float sweepRad = sideSign * theta;

        // Build transform: start at oarlock position on gunwale
        glm::mat4 om = glm::translate(am, glm::vec3(sideSign * 0.65f, 0.40f, 0.3f));

        // Sweep: physics-driven forward/backward rotation around Y axis
        om = glm::rotate(om, sweepRad, glm::vec3(0.0f, 1.0f, 0.0f));

        // Dip: base droop (-8°) + animated blade entry into water
        // Port side: negate dip because 180° Y flip inverts Z-rotation sense
        float dipAngle = -8.0f + dip;
        if (sideSign < 0.0f) dipAngle = -dipAngle;
        om = glm::rotate(om, glm::radians(dipAngle), glm::vec3(0.0f, 0.0f, 1.0f));

        // Lay oar flat: 90° X rotation so paddle face is visible from above
        om = glm::rotate(om, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

        // Port: flip 180° so blade extends outward in -X direction
        if (sideSign < 0.0f)
            om = glm::rotate(om, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // Offset oar so oarlock (pivot) is at origin — handle extends inboard
        om = glm::translate(om, glm::vec3(-L_IN_OAR, 0.0f, 0.0f));

        s.setMat4("model", om);
        glDrawElements(GL_TRIANGLES, oarIndexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);

    // ── Bow post — vertical timber at the prow tip ────────────────────────────
    drawCylinder(s, am, 0.0f, 1.05f, -3.0f, 0, 0, 0,
                 0.07f, 0.9f, 0.07f,
                 woodA * 0.4f, woodD, sp, 16.0f);
}

// ==================== A4: Harbor Patrol / Rescue Vessel ====================
// Hull geometry: LOFTED SURFACE from deck down to keel.
//   5 horizontal Bezier width profiles define hull half-width at each height.
//   Bow rake: lower levels don't extend as far forward -- creates curved bow
//   profile in side view (deck leads, keel trails).
//   Double-wall thickness, bow cap, deck surface, stern transom.
//
void generatePatrolShipHull()
{
    const int   Nt     = 40;
    const int   Ns     = 24;
    const float ZB     = -7.0f;
    const float ZS     =  7.0f;
    const float ZL     = ZS - ZB;
    const float thick  =  0.06f;

    const float Y[5] = { -1.00f, -0.50f, 0.10f, 0.90f, 1.60f };

    // Width profiles scaled up so hull comfortably contains superstructure
    // Deck half-width ~3.9 > bridge half-width 3.3
    const float WP[5][4] = {
        { 0.007f, 0.07f,  0.10f,  0.08f  },   // keel — nearly a line
        { 0.03f,  1.54f,  2.10f,  1.82f  },   // lower bilge
        { 0.06f,  2.66f,  3.50f,  3.26f  },   // waterline
        { 0.07f,  2.94f,  3.78f,  3.50f  },   // upper hull
        { 0.09f,  3.08f,  3.92f,  3.64f  },   // deck edge (~3.9 max half-width)
    };

    auto wAt = [&](int lv, float t) -> float {
        float u = 1.0f - t;
        return u*u*u*WP[lv][0] + 3*u*u*t*WP[lv][1] + 3*u*t*t*WP[lv][2] + t*t*t*WP[lv][3];
    };

    auto crossSection = [&](float t, float s, float &outX, float &outY) {
        float w[5], yv[5];
        for (int i = 0; i < 5; i++) { w[i] = wAt(i, t); yv[i] = Y[i]; }

        float segF = s * 4.0f;
        int seg = (int)segF;
        if (seg > 3) seg = 3;
        float localS = segF - (float)seg;
        int i0 = seg, i1 = seg + 1;

        float dwi0, dyi0, dwi1, dyi1;
        if (i0 > 0 && i0 < 4) {
            dwi0 = (w[i0+1] - w[i0-1]) * 0.5f;
            dyi0 = (yv[i0+1] - yv[i0-1]) * 0.5f;
        } else if (i0 == 0) {
            dwi0 = w[1] - w[0]; dyi0 = yv[1] - yv[0];
        } else {
            dwi0 = w[4] - w[3]; dyi0 = yv[4] - yv[3];
        }
        if (i1 > 0 && i1 < 4) {
            dwi1 = (w[i1+1] - w[i1-1]) * 0.5f;
            dyi1 = (yv[i1+1] - yv[i1-1]) * 0.5f;
        } else if (i1 == 0) {
            dwi1 = w[1] - w[0]; dyi1 = yv[1] - yv[0];
        } else {
            dwi1 = w[4] - w[3]; dyi1 = yv[4] - yv[3];
        }

        float ss = localS, ss2 = ss*ss, ss3 = ss2*ss;
        float h00 = 2*ss3 - 3*ss2 + 1, h10 = ss3 - 2*ss2 + ss;
        float h01 = -2*ss3 + 3*ss2,    h11 = ss3 - ss2;
        outX = h00*w[i0] + h10*dwi0 + h01*w[i1] + h11*dwi1;
        outY = h00*yv[i0] + h10*dyi0 + h01*yv[i1] + h11*dyi1;
        if (outX < 0.0f) outX = 0.0f;
    };

    // Hull point with bow rake: lower hull trails behind upper hull.
    // +0.05 minimum fade ensures even the deck level converges to a single bow tip.
    auto hullPoint = [&](float t, float s, float &hw, float &py) {
        crossSection(t, s, hw, py);
        float bowFadeEnd = (1.0f - s) * 0.45f + 0.05f;
        if (t < bowFadeEnd) {
            float f = t / bowFadeEnd;
            f = f * f * (3.0f - 2.0f * f);  // smoothstep S-curve
            hw *= f;
        }
        // Snap near-zero bow widths to zero for clean bow closure
        if (hw < 0.005f) hw = 0.0f;
    };

    int ringSize   = Ns + 1;
    int totalRings = Nt + 1;

    vector<vector<glm::vec3>> oPos(totalRings, vector<glm::vec3>(ringSize));
    vector<vector<glm::vec3>> oNrm(totalRings, vector<glm::vec3>(ringSize));
    vector<vector<glm::vec3>> iPos(totalRings, vector<glm::vec3>(ringSize));

    vector<float>        verts;
    vector<unsigned int> inds;

    // Bow rake: keel bow (s=0) trails deck bow (s=1) by this many Z units,
    // creating the classic raked stem profile visible from the side.
    const float bowRake = 3.5f;

    // Port side outer surface
    for (int it = 0; it <= Nt; it++) {
        float t = (float)it / Nt;
        for (int is = 0; is <= Ns; is++) {
            float s = (float)is / Ns;
            // Each height level has its own bow Z: deck (s=1) at ZB, keel (s=0) at ZB+bowRake
            float zBow_s = ZB + (1.0f - s) * bowRake;
            float z = zBow_s + t * (ZS - zBow_s);

            float hw, py;
            hullPoint(t, s, hw, py);

            float eps = 0.003f;
            float hw_t1, py_t1, hw_t0, py_t0;
            hullPoint(glm::min(t+eps,1.0f), s, hw_t1, py_t1);
            hullPoint(glm::max(t-eps,0.0f), s, hw_t0, py_t0);
            float hw_s1, py_s1, hw_s0, py_s0;
            hullPoint(t, glm::min(s+eps,1.0f), hw_s1, py_s1);
            hullPoint(t, glm::max(s-eps,0.0f), hw_s0, py_s0);

            float dt_r = (t > eps && t < 1.0f-eps) ? 2.0f*eps : eps;
            float ds_r = (s > eps && s < 1.0f-eps) ? 2.0f*eps : eps;

            // Correct derivatives accounting for bow-rake Z variation:
            // dZ/dt = ZS - zBow(s);  dZ/ds = -bowRake*(1-t)
            float zLen_s = ZS - zBow_s;
            float dz_ds  = -bowRake * (1.0f - t);
            glm::vec3 dPdt((hw_t1-hw_t0)/dt_r, (py_t1-py_t0)/dt_r, zLen_s);
            glm::vec3 dPds((hw_s1-hw_s0)/ds_r, (py_s1-py_s0)/ds_r, dz_ds);

            glm::vec3 n = glm::cross(dPds, dPdt);
            float nl = glm::length(n);
            if (nl > 1e-5f) n /= nl; else n = glm::vec3(1,0,0);
            if (n.x < 0.0f) n = -n;

            oPos[it][is] = glm::vec3(hw, py, z);
            oNrm[it][is] = n;
            float effThick = thick * glm::min(1.0f, hw / 0.05f);
            iPos[it][is] = oPos[it][is] - effThick * n;
            verts.insert(verts.end(), {hw, py, z, n.x, n.y, n.z, t, s});
        }
    }
    for (int it = 0; it < Nt; it++)
        for (int is = 0; is < Ns; is++) {
            unsigned int c = it*ringSize+is, cn = (it+1)*ringSize+is;
            inds.push_back(c); inds.push_back(c+1); inds.push_back(cn+1);
            inds.push_back(c); inds.push_back(cn+1); inds.push_back(cn);
        }

    // Starboard side outer surface (mirrored) — use op.z which has bow-rake baked in
    int sbBase = totalRings * ringSize;
    for (int it = 0; it <= Nt; it++) {
        for (int is = 0; is <= Ns; is++) {
            glm::vec3 op = oPos[it][is], on = oNrm[it][is];
            verts.insert(verts.end(), {-op.x, op.y, op.z, -on.x, on.y, on.z, (float)it/Nt, (float)is/Ns});
        }
    }
    for (int it = 0; it < Nt; it++)
        for (int is = 0; is < Ns; is++) {
            unsigned int c = sbBase+it*ringSize+is, cn = sbBase+(it+1)*ringSize+is;
            inds.push_back(c); inds.push_back(cn); inds.push_back(cn+1);
            inds.push_back(c); inds.push_back(cn+1); inds.push_back(c+1);
        }

    // Inner hull surfaces removed — they caused z-fighting artifacts near the bow
    // where the outer hull converges to near-zero thickness. iPos still used by gunwale rims.

    // Gunwale rims (port)
    int portRimBase = (int)(verts.size() / 8);
    for (int it = 0; it <= Nt; it++) {
        glm::vec3 op = oPos[it][Ns], ip = iPos[it][Ns];
        float t = (float)it / Nt;
        verts.insert(verts.end(), {op.x, op.y, op.z, 0,1,0, t, 0});
        verts.insert(verts.end(), {ip.x, ip.y, ip.z, 0,1,0, t, 1});
    }
    for (int it = 0; it < Nt; it++) {
        int b = portRimBase + it*2;
        inds.push_back(b); inds.push_back(b+2); inds.push_back(b+1);
        inds.push_back(b+1); inds.push_back(b+2); inds.push_back(b+3);
    }
    // Gunwale rims (starboard)
    int stbdRimBase = (int)(verts.size() / 8);
    for (int it = 0; it <= Nt; it++) {
        glm::vec3 op = oPos[it][Ns], ip = iPos[it][Ns];
        float t = (float)it / Nt;
        verts.insert(verts.end(), {-op.x, op.y, op.z, 0,1,0, t, 0});
        verts.insert(verts.end(), {-ip.x, ip.y, ip.z, 0,1,0, t, 1});
    }
    for (int it = 0; it < Nt; it++) {
        int b = stbdRimBase + it*2;
        inds.push_back(b+1); inds.push_back(b+2); inds.push_back(b);
        inds.push_back(b+3); inds.push_back(b+2); inds.push_back(b+1);
    }

    // Keel strip (connecting port to starboard at bottom)
    int keelBase = (int)(verts.size() / 8);
    for (int it = 0; it <= Nt; it++) {
        glm::vec3 pk = oPos[it][0];
        float t = (float)it / Nt;
        verts.insert(verts.end(), { pk.x, pk.y, pk.z, 0,-1,0, t, 0});
        verts.insert(verts.end(), {-pk.x, pk.y, pk.z, 0,-1,0, t, 1});
    }
    for (int it = 0; it < Nt; it++) {
        int b = keelBase + it*2;
        inds.push_back(b+1); inds.push_back(b+2); inds.push_back(b);
        inds.push_back(b+3); inds.push_back(b+2); inds.push_back(b+1);
    }

    // Deck surface (flat top connecting port to starboard deck edges)
    int deckBase = (int)(verts.size() / 8);
    for (int it = 0; it <= Nt; it++) {
        glm::vec3 pd = oPos[it][Ns];
        float t = (float)it / Nt;
        verts.insert(verts.end(), { pd.x, pd.y, pd.z, 0,1,0, t, 0});
        verts.insert(verts.end(), {-pd.x, pd.y, pd.z, 0,1,0, t, 1});
    }
    for (int it = 0; it < Nt; it++) {
        int b = deckBase + it*2;
        inds.push_back(b); inds.push_back(b+1); inds.push_back(b+2);
        inds.push_back(b+1); inds.push_back(b+3); inds.push_back(b+2);
    }

    // Bow cap (closes front gap between port and starboard)
    // Use quad strip stitching: match port (is) to starboard (-X mirrored is)
    // for watertight connection at the bow.
    {
        int bowBase = (int)(verts.size() / 8);
        // Port bow cross-section (keel=0 to deck=Ns)
        for (int is = 0; is <= Ns; is++) {
            glm::vec3 p = oPos[0][is];
            float s = (float)is / Ns;
            verts.insert(verts.end(), {p.x, p.y, p.z, 0,0,-1, s, 0.0f});
        }
        // Starboard bow cross-section (keel=0 to deck=Ns), mirrored X
        int bowSbBase = (int)(verts.size() / 8);
        for (int is = 0; is <= Ns; is++) {
            glm::vec3 p = oPos[0][is];
            float s = (float)is / Ns;
            verts.insert(verts.end(), {-p.x, p.y, p.z, 0,0,-1, s, 1.0f});
        }
        // Stitch port to starboard with quad strip (matched by is index)
        for (int is = 0; is < Ns; is++) {
            int p0 = bowBase + is,     p1 = bowBase + is + 1;
            int s0 = bowSbBase + is,   s1 = bowSbBase + is + 1;
            // Two triangles per quad: (p0, s0, p1) and (p1, s0, s1)
            inds.push_back(p0); inds.push_back(s0); inds.push_back(p1);
            inds.push_back(p1); inds.push_back(s0); inds.push_back(s1);
        }
    }

    // Stern transom cap (closes back)
    {
        int transomBase = (int)(verts.size() / 8);
        for (int is = 0; is <= Ns; is++) {
            glm::vec3 p = oPos[Nt][is];
            verts.insert(verts.end(), {p.x, p.y, p.z, 0,0,1, (float)is/Ns, 0});
        }
        for (int is = Ns; is >= 0; is--) {
            glm::vec3 p = oPos[Nt][is];
            verts.insert(verts.end(), {-p.x, p.y, p.z, 0,0,1, (float)is/Ns, 1});
        }
        int totalPts = 2*(Ns+1);
        glm::vec3 cen(0.0f);
        for (int i = 0; i < totalPts; i++) {
            int vi = transomBase+i;
            cen += glm::vec3(verts[vi*8], verts[vi*8+1], verts[vi*8+2]);
        }
        cen /= (float)totalPts;
        int cenV = (int)(verts.size()/8);
        verts.insert(verts.end(), {cen.x, cen.y, cen.z, 0,0,1, 0.5f, 0.5f});
        for (int i = 0; i < totalPts; i++) {
            int nxt = (i+1)%totalPts;
            inds.push_back(cenV);
            inds.push_back(transomBase+i);
            inds.push_back(transomBase+nxt);
        }
    }

    patrolHullIndexCount = (int)inds.size();
    setupVAO8(patrolHullVAO, patrolHullVBO, patrolHullEBO,
              verts.data(), (int)(verts.size()*sizeof(float)),
              inds.data(),  (int)(inds.size()*sizeof(unsigned int)));
}

// drawPatrolShip: renders the complete harbor patrol/rescue vessel.
// Reference: orange harbor patrol boat (see .claude/demo/view-graphic-3d-ship.jpg)
// Hull sides (chine→deck) are drawn from the ruled-surface VAO; all other parts
// use the existing primitive draw functions.
void drawPatrolShip(Shader &s, glm::mat4 p)
{
    // B6: use dynamic position + heading from autopilot (replaces hardcoded values)
    // B1: wave heave + pitch/roll still applied from wave_physics
    glm::mat4 sm = glm::translate(p, glm::vec3(patrolX, 0.25f + patrolHeave, patrolZ));
    sm = glm::rotate(sm, patrolHeading, glm::vec3(0,1,0));
    sm = glm::rotate(sm, patrolPitch, glm::vec3(1,0,0));
    sm = glm::rotate(sm, patrolRoll,  glm::vec3(0,0,1));

    // ── Material palette ─────────────────────────────────────────────────
    const glm::vec3 orA(0.48f,0.21f,0.02f), orD(0.95f,0.48f,0.08f), orS(0.55f,0.38f,0.18f);
    const glm::vec3 dkA(0.05f,0.05f,0.07f), dkD(0.12f,0.13f,0.19f), dkS(0.22f,0.22f,0.32f);
    const glm::vec3 gyA(0.28f,0.28f,0.30f), gyD(0.72f,0.74f,0.78f), gyS(0.38f,0.38f,0.42f);
    const glm::vec3 wtA(0.48f,0.48f,0.50f), wtD(0.93f,0.93f,0.94f), wtS(0.75f,0.75f,0.80f);
    const glm::vec3 goA(0.36f,0.26f,0.02f), goD(0.84f,0.64f,0.10f), goS(0.88f,0.78f,0.38f);
    const glm::vec3 wn (0.03f,0.04f,0.07f); // dark window glass
    const glm::vec3 e0(0.0f);
    const float sp = 32.0f, sh = 64.0f;

    // ── Full hull: lofted surface — patrol hull texture (8×4 tiling along hull) ──
    setMaterial(s, orA, orD, orS, sp, e0);
    s.setVec2("texScale", glm::vec2(8.0f, 4.0f));
    bindTex(s, patrolBodyTex);
    s.setMat4("model", sm);
    glBindVertexArray(patrolHullVAO);
    glDrawElements(GL_TRIANGLES, patrolHullIndexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));

    // ── Main bridge superstructure ────────────────────────────────────────
    // Main body — patrol wall texture (tex passed as last arg, drawCube binds it internally)
    s.setVec2("texScale", glm::vec2(4.0f, 2.0f));
    drawCube(s, sm,  0, 2.30f, 0.50f, 0,0,0, 3.30f,1.60f,5.20f, orA,orD,orS,sp,e0, patrolWallTex);
    // Roof — patrol roof texture
    s.setVec2("texScale", glm::vec2(3.0f, 2.0f));
    drawCube(s, sm,  0, 3.12f, 0.50f, 0,0,0, 3.44f,0.12f,5.34f,
             glm::vec3(0.45f), glm::vec3(0.55f), gyS, sp, e0, patrolRoofTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
    // Front windows (3 panels) — patrol glass texture
    for (int i = -1; i <= 1; i++)
        drawCube(s, sm, (float)i*0.96f, 2.30f,-2.14f, 0,0,0,
                 0.82f,0.62f,0.06f, wn*0.3f,wn,gyS,32.0f,glm::vec3(0.01f,0.02f,0.04f), patrolGlassTex);
    // Port side windows
    for (int wi = 0; wi < 3; wi++) {
        float wz = -0.30f + wi*1.50f;
        drawCube(s, sm, 1.67f,2.28f,wz, 0,0,0,
                 0.06f,0.48f,0.72f, wn*0.3f,wn,gyS,32.0f,glm::vec3(0.01f,0.02f,0.04f), patrolGlassTex);
    }
    // Starboard side windows
    for (int wi = 0; wi < 3; wi++) {
        float wz = -0.30f + wi*1.50f;
        drawCube(s, sm,-1.67f,2.28f,wz, 0,0,0,
                 0.06f,0.48f,0.72f, wn*0.3f,wn,gyS,32.0f,glm::vec3(0.01f,0.02f,0.04f), patrolGlassTex);
    }

    // ── Aft deck house (smaller secondary structure) ──────────────────────
    // Walls — patrol wall texture
    s.setVec2("texScale", glm::vec2(2.0f, 1.5f));
    drawCube(s, sm,  0.3f, 2.00f, 4.00f, 0,0,0, 2.20f,1.00f,2.20f, orA,orD,orS,sp,e0, patrolWallTex);
    // Roof — patrol roof texture
    s.setVec2("texScale", glm::vec2(2.0f, 2.0f));
    drawCube(s, sm,  0.3f, 2.52f, 4.00f, 0,0,0, 2.28f,0.10f,2.28f,
             glm::vec3(0.45f), glm::vec3(0.55f), gyS, sp, e0, patrolRoofTex);
    s.setVec2("texScale", glm::vec2(1.0f, 1.0f));
    // Aft house front windows — patrol glass texture
    drawCube(s, sm,  0.3f, 2.00f, 2.86f, 0,0,0, 1.40f,0.40f,0.06f,
             wn*0.3f,wn,gyS,32.0f,glm::vec3(0.008f,0.012f,0.025f), patrolGlassTex);

    // ── Exhaust funnel / chimney ──────────────────────────────────────────
    // cx=0.5 (slightly starboard), behind bridge
    drawCylinder(s, sm,  0.5f,3.20f, 2.80f, 0,0,0, 1.00f,4.00f,1.00f, dkA,dkD*1.3f,dkS,sp,e0);
    // Funnel flange cap
    drawCylinder(s, sm,  0.5f,3.60f, 2.80f, 0,0,0, 1.40f,0.60f,1.40f, dkA,dkD,dkS,sp,e0);

    // ── Mast ─────────────────────────────────────────────────────────────
    // Main vertical pole  (world r=0.2*0.35=0.07, world h=0.2*10=2.0)
    drawCylinder(s, sm,  0.0f, 4.20f, 0.50f, 0,0,0, 0.35f,10.0f,0.35f, gyA,gyD,gyS,sp,e0);
    // Cross-tree spreader
    drawCube(s, sm,      0.0f, 4.56f, 0.50f, 0,0,0, 1.80f,0.06f,0.06f, gyA,gyD,gyS,sp,e0);
    // Port antenna rod
    drawCylinder(s, sm,  0.90f,4.70f, 0.50f, 0,0,0, 0.10f,3.20f,0.10f, gyA,gyD,gyS,sp,e0);
    // Starboard antenna rod
    drawCylinder(s, sm, -0.90f,4.70f, 0.50f, 0,0,0, 0.10f,3.20f,0.10f, gyA,gyD,gyS,sp,e0);
    // Radar dome base cylinder  (r=0.2*1.1=0.22, h=0.2*2.0=0.4)
    drawCylinder(s, sm,  0.0f, 5.30f, 0.50f, 0,0,0, 1.10f,2.00f,1.10f, wtA,wtD*0.92f,wtS,sh,e0);
    // Radar dome sphere  (r=0.5*0.30=0.15 world)
    drawSphere(s, sm,    0.0f, 5.72f, 0.50f, 0,0,0, 0.30f,0.26f,0.30f, wtA,wtD,wtS,sh,e0);

    // ── Searchlight (binocular housing) ──────────────────────────────────
    // Main housing cylinder (rotated 90° around X → points forward -Z)
    drawCylinder(s, sm,  0.0f, 3.38f,-1.80f, 90,0,0, 2.00f,3.00f,2.00f, dkA,dkD*1.4f,dkS,sp,e0);
    // Support arm
    drawCylinder(s, sm,  0.0f, 3.12f,-1.60f, 28,0,0, 0.30f,2.80f,0.30f, gyA,gyD,gyS,sp,e0);
    // Left lens
    drawCylinder(s, sm, -0.18f,3.38f,-2.14f, 90,0,0, 0.65f,0.50f,0.65f, wtA*0.7f,wtD*0.6f,wtS,sh,e0);
    // Right lens
    drawCylinder(s, sm,  0.18f,3.38f,-2.14f, 90,0,0, 0.65f,0.50f,0.65f, wtA*0.7f,wtD*0.6f,wtS,sh,e0);

    // ── Bow bollard post with white ball ──────────────────────────────────
    drawCylinder(s, sm,  0.0f, 1.70f,-5.80f, 0,0,0, 0.50f,2.70f,0.50f, gyA,gyD,gyS,sp,e0);
    drawSphere(s, sm,    0.0f, 2.08f,-5.80f, 0,0,0, 0.26f,0.26f,0.26f, wtA,wtD,wtS,sh,e0);

    // ── Deck railings (follow hull Bezier curve, 3 rails) ────────────────
    auto deckHW_r = [](float t_r) -> float {
        const float w0=0.09f, w1=3.08f, w2=3.92f, w3=3.64f;
        float u = 1.0f - t_r;
        return u*u*u*w0 + 3.0f*u*u*t_r*w1 + 3.0f*u*t_r*t_r*w2 + t_r*t_r*t_r*w3;
    };
    // Stanchion: base at deck (1.62), total fence height 0.75 world units
    // cylVAO base height=0.2, so sy = stH*5.0 makes world height = stH exactly
    const float stBase = 1.62f;
    const float stH    = 0.75f;
    const float stCy   = stBase + stH * 0.5f;          // stanchion center Y
    // Three rails at 28%, 62%, 97% of stanchion height
    const float railY[3] = { stBase+stH*0.28f, stBase+stH*0.62f, stBase+stH*0.97f };

    float prev_dx = -1.0f, prev_rz = 0.0f;
    for (float rz = -5.5f; rz <= 6.2f; rz += 1.5f) {
        float t_rz = glm::clamp((rz + 7.0f) / 14.0f, 0.0f, 1.0f);
        float dx = glm::max(0.18f, deckHW_r(t_rz) - 0.08f);
        // Stanchions — sy=stH*5 so world height=stH (cylVAO base height=0.2)
        drawCylinder(s, sm,  dx, stCy, rz, 0,0,0, 0.16f,stH*5.0f,0.16f, gyA,gyD,gyS,sp,e0);
        drawCylinder(s, sm, -dx, stCy, rz, 0,0,0, 0.16f,stH*5.0f,0.16f, gyA,gyD,gyS,sp,e0);
        // Three connecting rail segments to the previous stanchion
        if (prev_dx > 0.0f) {
            float midX = (prev_dx + dx) * 0.5f;
            float midZ = (prev_rz + rz) * 0.5f;
            float ddx  = dx - prev_dx, ddz = rz - prev_rz;
            float len  = sqrtf(ddx*ddx + ddz*ddz);
            float ry   = glm::degrees(atan2f(ddx, ddz));
            for (int ri = 0; ri < 3; ri++) {
                drawCube(s, sm,  midX, railY[ri], midZ, 0, ry, 0, 0.04f,0.04f,len, gyA,gyD*0.8f,gyS,sp,e0);
                drawCube(s, sm, -midX, railY[ri], midZ, 0,-ry, 0, 0.04f,0.04f,len, gyA,gyD*0.8f,gyS,sp,e0);
            }
        }
        prev_dx = dx; prev_rz = rz;
    }

    // ── Propeller (aft of stern, centered on keel centerline) ────────────
    // Moved to z=7.35 (behind stern transom at z=7.0), y=-0.85 (below hull)
    drawSphere(s, sm,  0.0f,-0.85f, 7.35f, 0,0,0, 0.36f,0.36f,0.36f, dkA,dkD,dkS,sp,e0);
    // 3 blades (120° apart, slight pitch angle)
    for (int bi = 0; bi < 3; bi++) {
        glm::mat4 bm = glm::translate(sm, glm::vec3(0.0f,-0.85f,7.35f));
        bm = glm::rotate(bm, glm::radians(120.0f*(float)bi+30.0f), glm::vec3(0,0,1));
        drawCube(s, bm, 0,0.38f,0, 12,0,0, 0.10f,0.48f,0.26f, goA,goD,goS,sh,e0);
    }

    // ── Navigation lights (port=red, starboard=green) ────────────────────
    // Mounted on forward bridge corners: z=-2.14 = forward bridge face, x=±1.70 = bridge wings
    // Port side (red) — left/port bridge wing
    drawCylinder(s, sm,  1.70f, 3.22f,-2.14f, 0,0,0, 0.18f,1.40f,0.18f, gyA,gyD,gyS,sp,e0);
    drawSphere(s, sm,    1.70f, 3.58f,-2.14f, 0,0,0, 0.16f,0.16f,0.16f,
               glm::vec3(0.50f,0,0),glm::vec3(0.90f,0,0),glm::vec3(0.30f),16.0f,glm::vec3(0.22f,0,0));
    // Starboard side (green) — right/starboard bridge wing
    drawCylinder(s, sm, -1.70f, 3.22f,-2.14f, 0,0,0, 0.18f,1.40f,0.18f, gyA,gyD,gyS,sp,e0);
    drawSphere(s, sm,   -1.70f, 3.58f,-2.14f, 0,0,0, 0.16f,0.16f,0.16f,
               glm::vec3(0,0.28f,0),glm::vec3(0,0.72f,0),glm::vec3(0.28f),16.0f,glm::vec3(0,0.18f,0));

    // ── Porthole (one per side, orange section of hull) ───────────────────
    // Porthole y=0.68 is between WP[2] (waterline, y=0.10, s=0.50) and WP[3] (upper hull, y=0.90, s=0.75)
    // Lerp factor within that segment: (0.68-0.10)/(0.90-0.10) = 0.725
    // Interpolating both Bezier levels gives the TRUE hull surface X at that height.
    {
        float t_ph = (0.80f + 7.0f) / 14.0f;   // z=0.80 → t≈0.557
        float u_ph = 1.0f - t_ph;
        auto bezW = [&](float w0, float w1, float w2, float w3) -> float {
            return u_ph*u_ph*u_ph*w0 + 3.0f*u_ph*u_ph*t_ph*w1
                 + 3.0f*u_ph*t_ph*t_ph*w2 + t_ph*t_ph*t_ph*w3;
        };
        float hw2 = bezW(0.06f, 2.66f, 3.50f, 3.26f);  // WP[2] waterline
        float hw3 = bezW(0.07f, 2.94f, 3.78f, 3.50f);  // WP[3] upper hull
        float phX = glm::mix(hw2, hw3, 0.725f);         // hull surface at y=0.68 exactly
        drawCylinder(s, sm,  phX, 0.68f, 0.80f, 0,0,90, 1.40f,0.45f,1.40f,
                     gyA*0.6f,gyD*0.7f,gyS,sp,e0, patrolGlassTex);
        drawCylinder(s, sm, -phX, 0.68f, 0.80f, 0,0,90, 1.40f,0.45f,1.40f,
                     gyA*0.6f,gyD*0.7f,gyS,sp,e0, patrolGlassTex);
    }

    // Anchor fairlead removed — original z=-6.20 now sits in front of the bow
    // (keel bow moved to z=-3.5 with bow rake), making it float in open water.
}

// ── A3: Fractal Tree ────────────────────────────────────────────────────────
// drawFractalBranch: recursive branch renderer.
//   mat   = parent transform (branch base at origin, grows +Y)
//   depth = remaining levels (0 = leaf cluster only)
//   len   = world-space branch length
//   radius= world-space branch radius
//   nMin/nMax = branch count range per node:
//               nMin==nMax → fixed; nMin<nMax → random pick each node
//   lean  = degrees each sub-branch leans from parent axis
//
// Cylinder scale (createCylinder r=0.2, h=0.2):
//   sx=sz=radius*5, sy=len*5; cylinder is Y-centred → drawn at y=len/2
// Sphere scale (createSphere r=0.5):
//   sx=sy=sz=radius*7 (world radius ≈ radius*3.5)
//
// drawTree seeds rand() with a fixed value so the random tree shape is
// stable every frame (same seed → same branching pattern each call).
void drawFractalBranch(Shader &s, glm::mat4 mat, int depth, float len, float radius,
                       int nMin, int nMax, float lean)
{
    glm::vec3 barkA(0.22f, 0.13f, 0.04f), barkD(0.38f, 0.22f, 0.09f), barkS(0.08f);
    glm::vec3 leafA(0.04f, 0.22f, 0.04f), leafD(0.10f, 0.52f, 0.10f), leafS(0.05f);

    if (depth <= 0)
    {
        float randMultiplier = 1.0f + static_cast<float>(rand()) / RAND_MAX * 24.0f;
        float lr = radius * randMultiplier;
        drawSphere(s, mat, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                   lr, lr, lr,
                   leafA, leafD, leafS, 8.0f, glm::vec3(0.0f), treeLeavesTotalTex);
        return;
    }

    // Draw this branch cylinder (base at y=0, centred at y=len/2, tip at y=len)
    float cxz = radius * 5.0f;
    float cy  = len    * 5.0f;
    drawCylinder(s, mat, 0.0f, len * 0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
                 cxz, cy, cxz,
                 barkA, barkD, barkS, 16.0f, glm::vec3(0.0f), treeBarkTex);

    // Pick branch count for this node (random if nMin < nMax, fixed if equal)
    int n = (nMin == nMax) ? nMin : nMin + rand() % (nMax - nMin + 1);

    glm::mat4 tip = glm::translate(mat, glm::vec3(0.0f, len, 0.0f));
    float azStep  = 360.0f / (float)n;
    for (int i = 0; i < n; i++)
    {
        float az      = (float)i * azStep;
        glm::mat4 rot = glm::rotate(tip, glm::radians(az),   glm::vec3(0.0f, 1.0f, 0.0f));
        rot           = glm::rotate(rot, glm::radians(lean),  glm::vec3(1.0f, 0.0f, 0.0f));
        drawFractalBranch(s, rot, depth - 1, len * 0.65f, radius * 0.60f, nMin, nMax, lean);
    }
}

// drawTree: places a tree at (px,py,pz) with given branching character.
//   seed ensures the random branch pattern is identical every frame.
void drawTree(Shader &s, glm::mat4 p, float px, float py, float pz,
              int depth, float len, float radius, int nMin, int nMax, float lean, int seed)
{
    srand((unsigned int)seed); // fixed seed → stable random shape per tree
    glm::mat4 base = glm::translate(p, glm::vec3(px, py, pz));
    drawFractalBranch(s, base, depth, len, radius, nMin, nMax, lean);
}

void printControls()
{
    cout << "===================================================\n";
    cout << "          SeaForge - 3D SEA PORT CONTROLS          \n";
    cout << "===================================================\n";
    cout << " Escape     : Exit\n";
    cout << "\n [ CAMERA ]\n";
    cout << "  W/S/A/D   : Move Forward/Back/Left/Right\n";
    cout << "  E/R       : Up / Down\n";
    cout << "  X/Y/Z     : Pitch / Yaw / Roll\n";
    cout << "  F         : Orbit Camera\n";
    cout << "  V         : Reset Camera\n";
    cout << "  Mouse     : Look | Scroll: Zoom\n";
    cout << "\n [ PRESET VIEWS (Arrow Keys) ]\n";
    cout << "  UP        : Isometric view\n";
    cout << "  DOWN      : Top-down view\n";
    cout << "  LEFT      : Front view\n";
    cout << "  RIGHT     : Ship follow view\n";
    cout << "\n [ CARGO SHIP (B3 Inertia + B4 Buoyancy) ]\n";
    cout << "  I/K       : Ship Thrust Forward / Backward (drag coast)\n";
    cout << "  , / .     : Turn Port / Starboard\n";
    cout << "  [ / ]     : Add / Remove Cargo Weight (5t step)\n";
    cout << "  C         : Cycle Active Ship Slot\n";
    cout << "  Tab       : Cycle Active Yard Column (Pickup Pos)\n";
    cout << "\n [ CRANE ]\n";
    cout << "  J/L       : Crane Rotate Left / Right\n";
    cout << "  O/P       : Crane Arm Up / Down\n";
    cout << "\n [ CRANE LOADING (B7 — one-shot) ]\n";
    cout << "  V         : Part 1 — Pick up container from active yard col\n";
    cout << "  U         : Part 2 — Deliver to active ship slot\n";
    cout << "\n [ ANCIENT BOAT ROWING ]\n";
    cout << "  T         : Left (Port) Oar Stroke (Forward)\n";
    cout << "  G         : Right (Starboard) Oar Stroke (Forward)\n";
    cout << "  T+G       : Both Oars (Straight Forward)\n";
    cout << "  Ctrl+T    : Left Oar Reverse Stroke\n";
    cout << "  Ctrl+G    : Right Oar Reverse Stroke\n";
    cout << "  Ctrl+T+G  : Both Oars Reverse (Straight Backward)\n";
    cout << "\n [ SIMULATION ]\n";
    cout << "  Backspace : Full Simulation Reset\n";
    cout << "\n [ LIGHTING ]\n";
    cout << "  1/2/3     : Dir/Point/Spot Light Toggle\n";
    cout << "  5/6/7     : Ambient/Diffuse/Specular Toggle\n";
    cout << "\n [ RENDERING ]\n";
    cout << "  M         : Material Only Mode\n";
    cout << "  N         : Texture Only Mode\n";
    cout << "  8         : Blended Mode (Material+Texture)\n";
    cout << "  4         : Toggle Phong/Gouraud Shader\n";
    cout << "\n [ PATROL SHIP AUTOPILOT (B6) ]\n";
    cout << "  H         : Toggle Autopilot (harbor patrol route)\n";
    cout << "===================================================\n";
}

// ==================== B6: Autopilot Path Visualization ====================
// Draws the cubic B-spline path as a trail of small red cubes,
// and each control point (waypoint) as a green cube.
void drawPatrolShipPath(Shader &s, glm::mat4 p)
{
    // Material: bright red for path segments
    glm::vec3 rA(0.4f, 0.0f, 0.0f), rD(1.0f, 0.1f, 0.1f), rS(0.5f, 0.2f, 0.2f);
    glm::vec3 rE(0.8f, 0.05f, 0.05f); // slight emissive glow
    // Material: bright green for control points
    glm::vec3 gA(0.0f, 0.3f, 0.0f), gD(0.1f, 1.0f, 0.1f), gS(0.2f, 0.5f, 0.2f);
    glm::vec3 gE(0.05f, 0.7f, 0.05f);
    glm::vec3 z3(0.0f);

    float waveY = 0.3f; // slightly above water surface

    int n = (int)patrolAutopilot.waypoints.size();
    if (n < 4) return;
    float maxT = (float)(n - 3);

    // ── Draw spline path: ~120 small red cubes along the curve ────────────
    int numSamples = 120;
    for (int i = 0; i <= numSamples; i++) {
        float t = maxT * (float)i / (float)numSamples;
        glm::vec3 pt = patrolAutopilot.getSplinePoint(t);
        drawCube(s, p, pt.x, waveY, pt.z,  0,0,0,  0.2f,0.2f,0.2f,
                 rA, rD, rS, 16.0f, rE, 0);
    }

    // ── Draw control points: green cubes at each waypoint ─────────────────
    //    Skip the last duplicated convergence points (draw only unique ones)
    for (int i = 0; i < n; i++) {
        // Skip consecutive duplicates
        if (i > 0 && patrolAutopilot.waypoints[i] == patrolAutopilot.waypoints[i-1])
            continue;
        glm::vec3 wp = patrolAutopilot.waypoints[i];
        drawCube(s, p, wp.x, waveY + 0.1f, wp.z,  0,0,0,  0.5f,0.5f,0.5f,
                 gA, gD, gS, 16.0f, gE, 0);
    }
}