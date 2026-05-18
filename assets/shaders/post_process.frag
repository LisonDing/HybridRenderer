#version 450

// 此处接受 Offscreen Texture 作为输入，进行后处理效果（如色调映射、抗锯齿等）。
// 当前实现 动态曝光调整（Exposure）和电影暗角（Vignette）效果。

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

// 接受上阶段的渲染结果纹理 (占用 binding 0 槽位)。
layout(binding = 0) uniform sampler2D screenTexture;

// 推送常量（Push Constants）用于动态调整曝光和暗角强度。
layout(push_constant) uniform PostProcessParams {
    float exposure;
    float vignetteStrength;
} params;

void main() {
    // 从屏幕纹理中采样颜色
    vec3 color = texture(screenTexture, fragTexCoord).rgb;

    // 曝光色调映射（Tone Mapping - Reinhard / Exposure based）
    color = vec3(1.0) - exp(-color * params.exposure);

    // 计算暗角效果（Vignette）
    float dist = distance(fragTexCoord, vec2(0.5)); // 距离中心点的距离
    // 平滑插值计算暗角强度，params.vignetteStrength 控制暗角范围和强度
    float vignette = smoothstep(0.3, 0.8, dist); 
    color = mix(color, vec3(0.0), vignette * params.vignetteStrength); // 将暗角效果应用到颜色上

    outColor = vec4(color, 1.0);
}