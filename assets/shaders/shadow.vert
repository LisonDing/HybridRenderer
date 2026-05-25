#version 450

// 接收 Set 0 里的全局 UBO
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
    vec3 viewPos;
    float metallic;
    float roughness;
    mat4 lightSpaceMatrix; // 我们只关心这个！
} ubo;

// 我们只需要顶点的位置信息，不需要法线、UV和切线
layout(location = 0) in vec3 inPosition;

void main() {
    // 【核心】：利用光照空间矩阵，计算顶点在光源视角下的屏幕坐标
    gl_Position = ubo.lightSpaceMatrix * ubo.model * vec4(inPosition, 1.0);
}