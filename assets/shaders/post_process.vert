#version 450

// Uniform Buffer Object for post-processing parameters.
layout (location = 0) out vec2 fragTexCoord;

void main() {
    // Pass UV coordinates to the fragment shader for post-processing.
    // 当前顶点着色器用于全屏四边形时，UV 坐标可以通过顶点 ID 计算得出，以覆盖整个屏幕。
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);

    // 将 UV 映射到Vulkan到标准坐标系（-1到1），以覆盖整个屏幕。
    gl_Position = vec4(fragTexCoord * 2.0 - 1.0, 0.0, 1.0);
}