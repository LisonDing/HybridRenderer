#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec4 fragTangent; // 接收切线向量以便在片段着色器中计算切线空间的法线贴图

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


// layout(binding = 1) uniform sampler2D texSampler;
// layout(binding = 2) uniform sampler2D normalMap;
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
    float metallic;
    float roughness;
    mat4 lightSpaceMatrix;
} ubo;
// 暂时保留绑定的贴图，防止 Vulkan 报错，但我们先不采样它们
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D mraMap;

layout(location = 0) out vec4 outPosition; // G-Buffer 1: 存储世界空间位置
layout(location = 1) out vec4 outNormal;   // G-Buffer 2: 存储法线信息
layout(location = 2) out vec4 outAlbedo;   // G-Buffer 3: 存储颜色（反照率）


// 【新增】：使用 Push Constants 接收当前部件的材质基底属性
layout(push_constant) uniform MaterialPushConstant {
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
} matPC;

void main() {
    outPosition = vec4(fragPos, 1.0);
    
    // ==========================================
    // 1. 获取插值法线，并进行【双面光照与镜像修正】
    // ==========================================
    vec3 N = normalize(fragNormal);
    
    // 【核心黑魔法】：
    // 无论是单层布料的背面，还是因为 Scale=-1 被强行翻转成背面的镜像模型（如半个狮子头），
    // 只要显卡判定我们在看它的背面，我们就强制把法线翻转回朝向摄像机的一侧！
    vec3 V = normalize(ubo.viewPos - fragPos);
    if (dot(N, V) < 0.0) {
        N = -N;
    }

    // ==========================================
    // 2. 动态计算 TBN (依然保留极其稳定的屏幕偏导数法)
    // ==========================================
    vec3 dp1 = dFdx(fragPos);
    vec3 dp2 = dFdy(fragPos);
    vec2 duv1 = dFdx(fragTexCoord);
    vec2 duv2 = dFdy(fragTexCoord);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T,T), dot(B,B)));
    mat3 TBN = mat3(T * invmax, B * invmax, N);

    // ==========================================
    // 3. 采样并解析法线贴图
    // ==========================================
    vec3 tangentNormal = texture(normalMap, fragTexCoord).rgb;
    tangentNormal = tangentNormal * 2.0 - 1.0;
    
    // Vulkan Y 轴修正
    tangentNormal.y = -tangentNormal.y; 
    
    vec3 worldNormal = normalize(TBN * tangentNormal);

    // ==========================================
    // 4. PBR 材质参数采样
    // ==========================================
    vec4 texColor = texture(albedoMap, fragTexCoord);
    vec3 albedo = (texColor.rgb * matPC.baseColorFactor.rgb);
    
    vec4 mra = texture(mraMap, fragTexCoord);
    float metallic = mra.b * matPC.metallicFactor;   
    float roughness = mra.g * matPC.roughnessFactor; 

    outNormal = vec4(worldNormal, metallic);
    outAlbedo = vec4(albedo, roughness);
}