#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

// // Uniform buffer object containing transformation matrices and lighting information.
// // 包含变换矩阵和光照信息的统一缓冲对象 (占用 binding 0 槽位)。
// layout(binding = 0) uniform UniformBufferObject {
//     mat4 model;
//     mat4 view;
//     mat4 proj;
//     vec3 lightDir;
//     vec3 viewPos;

//     // 接受动态参数
//     float ambientStrength;
//     float specularStrength;
// } ubo;
// // Combined Image Sampler for texture reading.
// // 组合图像采样器，用于读取纹理像素 (占用 binding 1 槽位)。
// layout(binding = 1) uniform sampler2D texSampler;

// layout(location = 0) out vec4 outColor;

// void main() {
//     // Simple diffuse lighting calculation.
//     vec3 textureColor = texture(texSampler, fragTexCoord).rgb * fragColor;

//     // Ambient component
//     float ambientStrength = ubo.ambientStrength;
//     vec3 ambient = ambientStrength * textureColor;

//     // Diffuse component
//     vec3 norm = normalize(fragNormal);
//     vec3 lightDir = normalize(ubo.lightDir);
//     float diff = max(dot(norm, lightDir), 0.0);
//     vec3 diffuse = diff * textureColor;

//     // Specular component
//     float specularStrength = ubo.specularStrength;
//     vec3 viewDir = normalize(ubo.viewPos - fragPos);
//     vec3 reflectDir = reflect(-lightDir, norm);
//     float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
//     vec3 specular = specularStrength * spec * vec3(1.0); // White specular highlights

//     // Combine all components
//     vec3 result = ambient + diffuse + specular;
//     outColor = vec4(result, 1.0);
// }

// MRT
// 片段着色器接受来自顶点着色器的多个输出（颜色、纹理坐标、法线、位置），并将计算结果写入多个渲染目标（G-Buffer）。

// 采样基础纹理
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outPosition; // G-Buffer 1: 存储世界空间位置
layout(location = 1) out vec4 outNormal;   // G-Buffer 2: 存储法线信息
layout(location = 2) out vec4 outAlbedo;   // G-Buffer 3: 存储颜色（反照率）

void main() {
    // 写入世界坐标位置到 G-Buffer 1
    outPosition = vec4(fragPos, 1.0);

    // 写入法线信息到 G-Buffer 2
    outNormal = vec4(normalize(fragNormal), 1.0);

    // 写入颜色信息到 G-Buffer 3（结合基础纹理和顶点颜色）
    vec3 albedo = texture(texSampler, fragTexCoord).rgb * fragColor;
    outAlbedo = vec4(albedo, 1.0);
}