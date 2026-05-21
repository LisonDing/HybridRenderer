#include "Camera.h"
#include <algorithm>

namespace Core {

    Camera::Camera(glm::vec3 focus, float distance, float yaw, float pitch)
        : Focus(focus), Distance(distance), Yaw(yaw), Pitch(pitch), WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)) {
        updateCameraVectors();
    }

    glm::mat4 Camera::GetViewMatrix() const {
        return glm::lookAt(Position, Focus, Up);
    }

    void Camera::ProcessOrbit(float xoffset, float yoffset) {
        // 【核心魔法】：Yaw 永远绕着世界 Y 轴转，Pitch 永远绕着相机的 Right 轴转
        Yaw += xoffset * OrbitSensitivity;
        Pitch -= yoffset * OrbitSensitivity;

        // 【安全锁】：锁死俯仰角，防止跨越天顶导致画面颠倒 (Gimbal Lock)
        if (Pitch > 89.0f)  Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        updateCameraVectors();
    }

    void Camera::ProcessPan(float xoffset, float yoffset) {
        // 平移时，移动的是焦点 (Focus)
        Focus -= Right * xoffset * PanSensitivity;
        Focus += Up * yoffset * PanSensitivity;
        updateCameraVectors();
    }

    void Camera::ProcessZoom(float yoffset, float speed) {
        Distance -= yoffset * speed;
        if (Distance < 0.01f) Distance = 0.01f; // 防止距离穿透为负数
        updateCameraVectors();
    }

    void Camera::ResetFocus(glm::vec3 newFocus) {
        Focus = newFocus;
        updateCameraVectors();
    }

    void Camera::updateCameraVectors() {
        // 1. 基于欧拉角计算出从焦点指向相机的方向向量 (球面坐标转笛卡尔坐标)
        glm::vec3 direction;
        direction.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        direction.y = sin(glm::radians(Pitch));
        direction.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        
        // 2. 反向即可得到相机的朝向 (Front)
        Front = glm::normalize(-direction);

        // 3. 更新相机的位置 (焦点 + 方向 * 距离)
        Position = Focus + direction * Distance;

        // 4. 利用叉乘重新计算出严格的 Right 和 Up (完美杜绝 Roll 翻滚角)
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }

} // namespace Core