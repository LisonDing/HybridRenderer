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
    // float ambientStrength;
    // float specularStrength;
    float metallic; // 占位，保持与之前版本的 uniform buffer 大小一致
    float roughness; // 占位，保持与之前版本的 uniform buffer 大小一致
    mat4 lightSpaceMatrix; // 新增：光源空间变换矩阵，用于阴影映射
    
} ubo;

// 【新增】：硬件级深度比较采样器
layout(binding = 4) uniform sampler2DShadow shadowMap;

layout(push_constant) uniform PostProcessParams {
    float exposure;
    float vignetteStrength;
} params;

const float PI = 3.14159265359;

// PBR: 法线分布函数 (GGX) - 控制高光的面积与形状
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // 防止除零
}

// PBR: 几何函数 (Schlick-GGX) - 控制微表面的自我遮挡
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// PBR: 菲涅尔方程 (Fresnel-Schlick) - 边缘的反光增强现象
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // 从 G-Buffer 中采样位置、法线和颜色信息
    vec3 fragPos = texture(gPositon, fragTexCoord).rgb;
    
    vec4 normalData = texture(gNormal, fragTexCoord);
    vec3 N = normalData.rgb;
    float metallic = normalData.a; // 解包金属度

    vec4 albedoData = texture(gAlbedo, fragTexCoord);
    vec3 albedo = albedoData.rgb;
    float roughness = albedoData.a; // 解包粗糙度

    // 法线长度接近零时，说明该像素没有有效的几何信息，直接输出背景色并跳过光照计算
    if (length(N) < 0.1) {
        outColor = vec4(albedo * 0.05, 1.0); // 输出暗淡的背景色
        return;
    }

    vec3 V = normalize(ubo.viewPos - fragPos);
    vec3 L = normalize(ubo.lightDir);
    vec3 H = normalize(V + L); // 半角向量

    // 基础反射率：绝缘体默认 0.04，金属则使用 Albedo 作为反射色
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // --- Cook-Torrance BRDF 计算 ---
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3 nominator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // 防止除零
    vec3 specular = nominator / denominator;
    
    // 能量守恒：反射出去的光 (kS) + 折射进材质的光 (kD) = 1.0
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 纯金属没有漫反射

    float NdotL = max(dot(N, L), 0.0);
    // 假设单色光源强度为 vec3(3.0)
    // vec3 radiance = vec3(3.0); 
    vec3 radiance = vec3(20.0); // 模拟 HDR 场景中的强光源，此处为方便展示PBR效果，在同样的灯光参数下，PBR 引擎里的漫反射亮度天然只有传统引擎的 三分之一

    // ==========================================
    // 【核心新增】：计算阴影遮挡因子 (Shadow Factor)
    // ==========================================
    vec4 fragPosLightSpace = ubo.lightSpaceMatrix * vec4(fragPos, 1.0);
    // 透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Vulkan 中 XY 坐标需从 [-1, 1] 映射到 UV 的 [0, 1]
    projCoords.xy = projCoords.xy * 0.5 + 0.5; 
    
    float shadow = 0.0;
    // 【严格修复】：对 X, Y, Z 三轴同时做 [0, 1] 范围的合法性检查
    // 只有完全落在光源视锥体内的数据才允许采样深度图，彻底根治中轴线裂开的惨剧！
    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
        projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
        projCoords.z >= 0.0 && projCoords.z <= 1.0) {
        // sampler2DShadow 会自动将 projCoords.z 与深度图里存的值做比较。
        // 返回 1.0 表示“未被遮挡”(照亮)，返回 0.0 表示“被遮挡”(阴影中)。
        // 配合前面 C++ 设置的双线性过滤 (LINEAR)，它会自动实现免费的 PCF 软阴影边缘！
        float visibility = texture(shadowMap, vec3(projCoords.xy, projCoords.z));
        shadow = 1.0 - visibility; 
    }
    // ==========================================

    // 【修改】：将直接光照 Lo 乘以 (1.0 - shadow) 以切断被遮挡的光线！
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // HDR 色调映射与伽马校正 (PBR 必须在线性空间计算，最后转回 sRGB)
    color = vec3(1.0) - exp(-color * params.exposure); // Tone mapping
    // color = pow(color, vec3(1.0/2.2)); // Gamma correction 
    
    // 暗角
    float dist = distance(fragTexCoord, vec2(0.5));
    float vignetteMask = smoothstep(0.3, 0.8, dist);
    color = mix(color, vec3(0.0), vignetteMask * params.vignetteStrength);

    outColor = vec4(color, 1.0);
}
    