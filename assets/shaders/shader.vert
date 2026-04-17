#version 450

// Uniform Buffer Object containing transformation matrices.
// 统一缓冲对象，用于接收来自 CPU 端的模型、观察与投影矩阵。
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
} ubo;

// Input vertex attributes.
// 输入顶点属性（空间位置与顶点颜色）。
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal; // 接收法线

// Output color data to the fragment shader.
// 传递至片段着色器的输出颜色数据。
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos; // 传递世界坐标系下的位置给片段着色器计算光照

void main() {
    // Compute world position of the vertex.
    // 计算顶点的世界坐标位置。
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    // Compute the final clip space position using MVP transformation.
    // 使用 MVP 矩阵变换计算最终的裁剪空间坐标。
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Pass vertex color to the fragment stage.
    // 将顶点颜色透传至片段插值阶段。
    fragColor = inColor;
    // Pass UV coordinates to the fragment shader.
    // 将 UV 坐标透传至片段着色器。
    fragTexCoord = inTexCoord;

    // Pass normal vector to the fragment shader for lighting calculations.
    // 将法线向量透传至片段着色器以进行光照计算。
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal; // 法线变换：使用模型矩阵的逆转置矩阵来正确变换
    // Pass world position to the fragment shader for lighting calculations.
    // 将世界坐标位置透传至片段着色器以进行光照计算。
    fragPos = worldPosition.xyz;
}