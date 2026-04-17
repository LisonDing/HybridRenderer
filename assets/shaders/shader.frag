#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

// Uniform buffer object containing transformation matrices and lighting information.
// 包含变换矩阵和光照信息的统一缓冲对象 (占用 binding 0 槽位)。
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
} ubo;
// Combined Image Sampler for texture reading.
// 组合图像采样器，用于读取纹理像素 (占用 binding 1 槽位)。
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple diffuse lighting calculation.
    vec3 textureColor = texture(texSampler, fragTexCoord).rgb * fragColor;

    // Ambient component
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * textureColor;

    // Diffuse component
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(ubo.lightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * textureColor;

    // Specular component
    float specularStrength = 0.5;
    vec3 viewDir = normalize(ubo.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * vec3(1.0); // White specular highlights

    // Combine all components
    vec3 result = ambient + diffuse + specular;
    outColor = vec4(result, 1.0);
}