#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

// Output: final computed color (interpolated across fragments)
out vec4 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;

// Material
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
    vec3 emissive;
};
uniform Material material;

// Directional Light
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;

// Point Light
struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
#define NR_POINT_LIGHTS 9
uniform PointLight pointLights[NR_POINT_LIGHTS];

// Spot Light
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform SpotLight spotLight;

// Toggles
uniform bool directLightOn;
uniform bool ambientOn;
uniform bool diffuseOn;
uniform bool specularOn;
uniform bool pointLightOn;
uniform bool spotLightOn;

// Texture & Render Mode (sampled in vertex shader for true Gouraud)
uniform int renderMode;        // 0=material, 1=texture, 2=blended
uniform sampler2D texture1;
uniform bool useTexture;
uniform vec2 texScale;         // UV texture scale

// Object color bypass
uniform bool useObjectColor;
uniform vec3 objectColor;
uniform float alpha;

uniform vec3 globalAmbient;



// B1: Gerstner wave uniforms
uniform float time;
uniform int useWave;

void main()
{
    vec3 pos = aPos;
    vec3 norm = aNormal;

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

        // Recompute normal via finite differences
        float eps = 0.05;
        float p1R = 1.2 * (aPos.x + eps) - 1.5 * time;
        float p2R = 0.8 * aPos.z - 1.1 * time + 1.2;
        float hR = aPos.y + 0.15 * sin(p1R) + 0.08 * sin(p2R);

        float p1F = 1.2 * aPos.x - 1.5 * time;
        float p2F = 0.8 * (aPos.z + eps) - 1.1 * time + 1.2;
        float hF = aPos.y + 0.15 * sin(p1F) + 0.08 * sin(p2F);

        float hBase = aPos.y + 0.15 * sin(1.2 * aPos.x - 1.5 * time) + 0.08 * sin(0.8 * aPos.z - 1.1 * time + 1.2);

        vec3 tangentX = vec3(eps, hR - hBase, 0.0);
        vec3 tangentZ = vec3(0.0, hF - hBase, eps);
        norm = normalize(cross(tangentZ, tangentX));
    }

    vec3 FragPos = vec3(model * vec4(pos, 1.0));
    vec3 Normal = normalize(mat3(transpose(inverse(model))) * norm);
    gl_Position = projection * view * vec4(FragPos, 1.0);

    // Object color bypass (e.g. for debug or flat-colored objects)
    if (useObjectColor) {
        aColor = vec4(objectColor, alpha);
        return;
    }

    // --- Determine base colors from render mode ---
    vec3 baseDiffuse = material.diffuse;
    vec3 baseAmbient = material.ambient;

    vec2 tc = aTexCoord * texScale;

    if (renderMode == 1 && useTexture) {
        vec3 texColor = textureLod(texture1, tc, 0.0).rgb;
        baseDiffuse = texColor;
        baseAmbient = texColor * 0.3;
    }
    else if (renderMode == 2 && useTexture) {
        vec3 texColor = textureLod(texture1, tc, 0.0).rgb;
        baseDiffuse = material.diffuse * texColor;
        baseAmbient = material.ambient * texColor * 0.3;
    }

    // --- Accumulate lighting per-vertex ---
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 ambAcc = vec3(0.0), difAcc = vec3(0.0), spcAcc = vec3(0.0);

    // Directional Light
    if (directLightOn) {
        vec3 lightDir = normalize(-dirLight.direction);
        float d = max(dot(Normal, lightDir), 0.0);
        vec3 reflectDir = reflect(-lightDir, Normal);
        float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

        if (ambientOn)  ambAcc += dirLight.ambient;
        if (diffuseOn)  difAcc += dirLight.diffuse * d;
        if (specularOn) spcAcc += dirLight.specular * s;
    }

    // Point Lights
    if (pointLightOn) {
        for (int i = 0; i < NR_POINT_LIGHTS; i++) {
            vec3 lightDir = normalize(pointLights[i].position - FragPos);
            float d = max(dot(Normal, lightDir), 0.0);
            vec3 reflectDir = reflect(-lightDir, Normal);
            float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
            float dist = length(pointLights[i].position - FragPos);
            float att = 1.0 / (pointLights[i].constant + pointLights[i].linear * dist + pointLights[i].quadratic * dist * dist);

            if (ambientOn)  ambAcc += pointLights[i].ambient * att;
            if (diffuseOn)  difAcc += pointLights[i].diffuse * d * att;
            if (specularOn) spcAcc += pointLights[i].specular * s * att;
        }
    }

    // Spot Light
    if (spotLightOn) {
        vec3 lightDir = normalize(spotLight.position - FragPos);
        float theta = dot(lightDir, normalize(-spotLight.direction));
        float dist = length(spotLight.position - FragPos);
        float att = 1.0 / (spotLight.constant + spotLight.linear * dist + spotLight.quadratic * dist * dist);

        if (ambientOn) ambAcc += spotLight.ambient * att;

        if (theta > spotLight.cutOff) {
            float d = max(dot(Normal, lightDir), 0.0);
            vec3 reflectDir = reflect(-lightDir, Normal);
            float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
            if (diffuseOn)  difAcc += spotLight.diffuse * d * att;
            if (specularOn) spcAcc += spotLight.specular * s * att;
        }
    }

    // --- Final color: combine lighting with base colors ---
    vec3 result = globalAmbient*baseAmbient 
                + ambAcc * baseAmbient
                + difAcc * baseDiffuse
                + spcAcc * material.specular
                + material.emissive;

    aColor = vec4(result, alpha);
}
