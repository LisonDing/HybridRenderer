#version 450

// 此处接受 Offscreen Texture 作为输入，进行后处理效果（如色调映射、抗锯齿等）。
// 当前实现 动态曝光调整（Exposure）和电影暗角（Vignette）效果。

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// // 接受上阶段的渲染结果纹理 (占用 binding 0 槽位)。
// layout(binding = 0) uniform sampler2D screenTexture;

// // 推送常量（Push Constants）用于动态调整曝光和暗角强度。
// layout(push_constant) uniform PostProcessParams {
//     float exposure;
//     float vignetteStrength;
// } params;

// void main() {
//     // 从屏幕纹理中采样颜色
//     vec3 color = texture(screenTexture, fragTexCoord).rgb;

//     // 曝光色调映射（Tone Mapping - Reinhard / Exposure based）
//     color = vec3(1.0) - exp(-color * params.exposure);

//     // 计算暗角效果（Vignette）
//     float dist = distance(fragTexCoord, vec2(0.5)); // 距离中心点的距离
//     // 平滑插值计算暗角强度，params.vignetteStrength 控制暗角范围和强度
//     float vignette = smoothstep(0.3, 0.8, dist); 
//     color = mix(color, vec3(0.0), vignette * params.vignetteStrength); // 将暗角效果应用到颜色上

//     outColor = vec4(color, 1.0);
// }

// MRT 版本：同时输出处理后的颜色和暗角强度
layout(binding = 0) uniform sampler2D gPositon; // G-Buffer 1: 世界空间位置
layout(binding = 1) uniform sampler2D gNormal;   // G-Buffer 2: 法线信息
layout(binding = 2) uniform sampler2D gAlbedo;   // G-Buffer 3: 漫反射颜色

layout(binding = 3) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
    float ambientStrength;
    float specularStrength;
} ubo;

layout(push_constant) uniform PostProcessParams {
    float exposure;
    float vignetteStrength;
} params;

void main() {
    // 从 G-Buffer 中采样位置、法线和颜色信息
    vec3 fragPos = texture(gPositon, fragTexCoord).rgb;
    vec3 normal = texture(gNormal, fragTexCoord).rgb;
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;

    // 法线长度接近零时，说明该像素没有有效的几何信息，直接输出背景色并跳过光照计算
    if (length(normal) < 0.1) {
        outColor = vec4(albedo * 0.05, 1.0); // 输出暗淡的背景色
        return;
    }

    // 在屏幕空间计算3D位置和法线的光照效果

    // Ambient component
    vec3 ambient = ubo.ambientStrength * albedo;

    // Diffuse component
    vec3 lightDir = normalize(ubo.lightDir);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * albedo;

    // Specular component
    vec3 viewDir = normalize(ubo.viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = ubo.specularStrength * spec * vec3(1.0); // 白色高光

    vec3 result = ambient + diffuse + specular;

    // 曝光色调映射
    result = vec3(1.0) - exp(-result * params.exposure);
    
    // 计算暗角效果
    float dist = distance(fragTexCoord, vec2(0.5));
    float vignette = smoothstep(0.3, 0.8, dist);
    result = mix(result, vec3(0.0), vignette * params.vignetteStrength);

    outColor = vec4(result, 1.0);
}
    