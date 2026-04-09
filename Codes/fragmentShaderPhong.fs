#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

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

uniform bool useObjectColor;
uniform vec3 objectColor;
uniform float alpha;

// Texture & Render Mode
uniform int renderMode;        // 0=material, 1=texture, 2=blended
uniform sampler2D texture1;
uniform bool useTexture;       // per-object flag
uniform vec2 texScale;         // UV texture scale (default=(1,1); e.g. (2,1) for U-mirror, (3,3) for roof tiling)

// Prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec);


uniform vec3 globalAmbient;

void main()
{
    if (useObjectColor) {
        FragColor = vec4(objectColor, alpha);
        return;
    }

    // Determine base colors from render mode
    vec3 baseDiffuse = material.diffuse;
    vec3 baseAmbient = material.ambient;

    if (renderMode == 1 && useTexture) {
        vec2 tc = TexCoord * texScale;
        vec3 texColor = texture(texture1, tc).rgb;
        baseDiffuse = texColor;
        baseAmbient = texColor * 0.3;
    }
    else if (renderMode == 2 && useTexture) {
        vec2 tc = TexCoord * texScale;
        vec3 texColor = texture(texture1, tc).rgb;
        baseDiffuse = material.diffuse * texColor;
        baseAmbient = material.ambient * texColor * 0.3;
    }

    vec3 baseSpecular = material.specular;

    // Per-fragment lighting (Phong)
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 result = globalAmbient * baseAmbient;

    if (directLightOn)
        result += CalcDirLight(dirLight, norm, viewDir, baseDiffuse, baseAmbient, baseSpecular);
    if (pointLightOn)
        for (int i = 0; i < NR_POINT_LIGHTS; i++)
            result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, baseDiffuse, baseAmbient, baseSpecular);
    if (spotLightOn)
        result += CalcSpotLight(spotLight, norm, FragPos, viewDir, baseDiffuse, baseAmbient, baseSpecular);

    result += material.emissive;
    FragColor = vec4(result, alpha);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec)
{
    vec3 lightDir = normalize(-light.direction);
    float d = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    vec3 ambient  = light.ambient  * amb;
    vec3 diffuse  = light.diffuse  * d * diff;
    vec3 specular = light.specular * s * spec;

    if (!ambientOn)  ambient  = vec3(0.0);
    if (!diffuseOn)  diffuse  = vec3(0.0);
    if (!specularOn) specular = vec3(0.0);

    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float d = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 ambient  = light.ambient  * amb  * attenuation;
    vec3 diffuse  = light.diffuse  * d * diff * attenuation;
    vec3 specular = light.specular * s * spec * attenuation;

    if (!ambientOn)  ambient  = vec3(0.0);
    if (!diffuseOn)  diffuse  = vec3(0.0);
    if (!specularOn) specular = vec3(0.0);

    return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diff, vec3 amb, vec3 spec)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction));

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 ambient = light.ambient * amb * attenuation;
    if (!ambientOn) ambient = vec3(0.0);

    if (theta > light.cutOff) {
        float d = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = light.diffuse * d * diff * attenuation;
        vec3 reflectDir = reflect(-lightDir, normal);
        float s = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = light.specular * s * spec * attenuation;

        if (!diffuseOn)  diffuse  = vec3(0.0);
        if (!specularOn) specular = vec3(0.0);

        return ambient + diffuse + specular;
    }
    return ambient;
}
