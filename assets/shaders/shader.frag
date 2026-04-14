#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

// Combined Image Sampler for texture reading.
// 组合图像采样器，用于读取纹理像素 (占用 binding 1 槽位)。
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture and multiply by the vertex color.
    // 对纹理进行采样，并与顶点颜色进行正片叠底运算。
    outColor = texture(texSampler, fragTexCoord) * vec4(fragColor, 1.0);
}