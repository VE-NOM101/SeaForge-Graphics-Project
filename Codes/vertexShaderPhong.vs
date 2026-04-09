#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// B1: Gerstner wave uniforms
uniform float time;
uniform int useWave;

void main()
{
    vec3 pos = aPos;

    // B1: Gerstner wave displacement (only for water mesh)
    if (useWave == 1)
    {
        // Wave 1: k=1.2, A=0.15, w=1.5, phi=0.0, dir=(1,0) along X
        float phase1 = 1.2 * pos.x - 1.5 * time + 0.0;
        pos.y += 0.15 * sin(phase1);
        pos.x += 0.15 * cos(phase1);

        // Wave 2: k=0.8, A=0.08, w=1.1, phi=1.2, dir=(0,1) along Z
        float phase2 = 0.8 * pos.z - 1.1 * time + 1.2;
        pos.y += 0.08 * sin(phase2);
        pos.z += 0.08 * cos(phase2);
    }

    FragPos = vec3(model * vec4(pos, 1.0));

    // Recompute normal for wave surface using finite differences
    if (useWave == 1)
    {
        float eps = 0.05;
        // Neighboring wave heights for normal estimation
        float hC = pos.y;  // center already displaced

        vec3 posR = aPos;
        posR.x += eps;
        float p1R = 1.2 * posR.x - 1.5 * time + 0.0;
        float p2R = 0.8 * posR.z - 1.1 * time + 1.2;
        float hR = aPos.y + 0.15 * sin(p1R) + 0.08 * sin(p2R);

        vec3 posF = aPos;
        posF.z += eps;
        float p1F = 1.2 * posF.x - 1.5 * time + 0.0;
        float p2F = 0.8 * posF.z - 1.1 * time + 1.2;
        float hF = aPos.y + 0.15 * sin(p1F) + 0.08 * sin(p2F);

        float hBase = aPos.y + 0.15 * sin(1.2 * aPos.x - 1.5 * time) + 0.08 * sin(0.8 * aPos.z - 1.1 * time + 1.2);

        vec3 tangentX = vec3(eps, hR - hBase, 0.0);
        vec3 tangentZ = vec3(0.0, hF - hBase, eps);
        Normal = mat3(transpose(inverse(model))) * normalize(cross(tangentZ, tangentX));
    }
    else
    {
        Normal = mat3(transpose(inverse(model))) * aNormal;
    }

    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
