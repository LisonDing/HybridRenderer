#pragma once

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Core {

class Camera {
public:
    glm::vec3 Target;    // 视觉焦点 (永远是你环绕的中心)
    float Distance;      // 距离
    float Yaw;           // 偏航角
    float Pitch;         // 俯仰角

    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // 【核心修复1：彻底确立 Z轴向上 (Z-Up) 的物理法则】
    Camera(glm::vec3 target = glm::vec3(0.0f), float distance = 4.0f) 
        : Target(target), Distance(distance), Yaw(0.0f), Pitch(0.0f), WorldUp(0.0f, 0.0f, 1.0f) {
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(Position, Target, Up);
    }

    // 中键：完美的 Turntable 环绕 (无死角)
    void ProcessOrbit(float xoffset, float yoffset, float sensitivity = 0.3f) {
        Yaw   -= xoffset * sensitivity; 
        Pitch -= yoffset * sensitivity; // 无限自由上下翻转，无角度锁死
        updateCameraVectors();
    }

    // Shift + 中键：平移画布，移动焦点
    void ProcessPan(float xoffset, float yoffset, float sensitivity = 0.005f) {
        float speed = sensitivity * Distance; 
        Target -= Right * (xoffset * speed);
        Target += Up * (yoffset * speed);
        updateCameraVectors();
    }

    // 滚轮缩放
    void ProcessZoom(float yoffset, float sensitivity = 0.5f) {
        Distance -= yoffset * sensitivity;
        if (Distance < 0.1f) Distance = 0.1f; 
        updateCameraVectors();
    }

    void ResetFocus(glm::vec3 focusPoint = glm::vec3(0.0f)) {
        Target = focusPoint;
        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        // 【核心修复2：基底向量对齐 Z-Up 坐标系】
        // 初始状态下，摄像机位于 -Y，看向 +Y，头顶是 +Z
        glm::vec3 baseFront(0.0f, 1.0f, 0.0f);
        glm::vec3 baseRight(1.0f, 0.0f, 0.0f);
        glm::vec3 baseUp(0.0f, 0.0f, 1.0f);

        // 利用纯正的欧拉旋转构建无横滚 (No-Roll) 的四元数
        glm::quat qYaw = glm::angleAxis(glm::radians(Yaw), baseUp);
        glm::quat qPitch = glm::angleAxis(glm::radians(Pitch), baseRight);

        // 先绕自身的 X 轴做俯仰，再绕世界的 Z 轴做偏航
        glm::quat orientation = qYaw * qPitch;

        Front = glm::normalize(orientation * baseFront);
        Right = glm::normalize(orientation * baseRight);
        Up    = glm::normalize(orientation * baseUp);

        Position = Target - Front * Distance;
    }
};

} // namespace Core