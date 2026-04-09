#pragma once
// ================================================================
// human.h — SeaForge Human System
// Static humanoid figure placed at a fixed position on the dock.
// No movement, no physics, no boat riding.
// ================================================================
#include "shader.h"

// ---- Lifecycle ----
void initHuman();                   // Called once after OpenGL init
void drawHuman(Shader& shader);     // Called each frame (rendering)
