#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Core {

    class Camera {
    public:
        // 核心状态
        glm::vec3 Position;
        glm::vec3 Focus;
        float Distance;

        // 欧拉角 (严格控制，杜绝侧倾)
        float Yaw;
        float Pitch;

        // 局部坐标系
        glm::vec3 Front;
        glm::vec3 Up;
        glm::vec3 Right;
        glm::vec3 WorldUp;

        // 灵敏度参数
        float OrbitSensitivity = 0.2f;
        float PanSensitivity = 0.01f;

        // 构造函数，增加初始 Yaw 和 Pitch 设置
        Camera(glm::vec3 focus = glm::vec3(0.0f), float distance = 5.0f, float yaw = -90.0f, float pitch = 0.0f);

        glm::mat4 GetViewMatrix() const;

        // 交互接口
        void ProcessOrbit(float xoffset, float yoffset);
        void ProcessPan(float xoffset, float yoffset);
        void ProcessZoom(float yoffset, float speed);
        void ResetFocus(glm::vec3 newFocus);

    private:
        // 每次修改参数后，必须重新计算向量
        void updateCameraVectors();
    };

} // namespace Core