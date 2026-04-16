#pragma once

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Core {

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT };

class Camera {
public:
    // Core parameters for DCC Orbit Camera.
    // DCC 环绕摄像机的核心参数。
    glm::vec3 Target;    // 视觉焦点
    float Distance;      // 摄像机距离焦点的半径距离

    // Euler Angles.
    // 欧拉角。
    float Yaw;
    float Pitch;

    // Derived vectors and position.
    // 推导出的空间向量与最终位置。
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float MovementSpeed;

    Camera(glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f), float distance = 4.0f, float yaw = -90.0f, float pitch = 0.0f) 
        : Target(target), Distance(distance), Yaw(yaw), Pitch(pitch), WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)), MovementSpeed(5.0f) {
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() const {
        // Always look at the defined target.
        // 永远死死盯住我们定义的目标焦点。
        return glm::lookAt(Position, Target, Up);
    }

    // Synchronized translation : moves both Position and Target to maintain the orbit relationship.
    // 同步平移：同时移动位置与目标，保持环绕关系不变。
    void ProcessKeyboard(Camera_Movement direction, float deltaTime) {
        float velocity = MovementSpeed * deltaTime;
        glm::vec3 moveVec(0.0f);

        if (direction == FORWARD) moveVec = Front * velocity;
        if (direction == BACKWARD) moveVec = -Front * velocity;
        if (direction == LEFT) moveVec = -Right * velocity;
        if (direction == RIGHT) moveVec = Right * velocity;

        // Update the camera's position and target.
        Position += moveVec;
        Target += moveVec;
    }

    // Orbit: Rotate the camera around the target.
    // 环绕：改变欧拉角，使摄像机绕目标点旋转。
    void ProcessOrbit(float xoffset, float yoffset, float sensitivity = 0.25f) {
        Yaw   += xoffset * sensitivity;
        Pitch -= yoffset * sensitivity; // Invert Y for standard DCC feel / 反转 Y 轴偏移以符合 DCC 习惯

        if (Pitch > 89.0f)  Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        updateCameraVectors();
    }

    // Pan: Move the target and camera parallel to the screen.
    // 平移：使目标点在摄像机的局部 X/Y 平面上移动。
    void ProcessPan(float xoffset, float yoffset, float sensitivity = 0.005f) {
        // The further away, the faster we pan to maintain consistent visual speed.
        // 距离越远，平移越快，以保持视觉上的移速恒定。
        float speed = sensitivity * Distance; 
        
        Target -= Right * (xoffset * speed);
        Target += Up * (yoffset * speed);

        updateCameraVectors();
    }

    // Zoom: Change the distance to the target.
    // 缩放：改变摄像机到目标点的物理半径距离。
    void ProcessZoom(float yoffset, float sensitivity = 0.5f) {
        Distance -= yoffset * sensitivity;
        if (Distance < 0.1f) Distance = 0.1f; // Prevent going through the target / 防止穿过目标点

        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        // 1. Calculate new Front vector using Euler angles.
        // 1. 利用欧拉角计算新的前向向量。
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        
        // 2. Re-calculate Right and Up vectors.
        // 2. 重新计算局部右向与上向向量。
        Right = glm::normalize(glm::cross(Front, WorldUp));  
        Up    = glm::normalize(glm::cross(Right, Front));

        // 3. Crucial Shift: Position is now strictly derived from Target and Distance.
        // 3. 核心转变：位置不再是独立的，而是由目标点、前向向量和距离逆向推导出来的。
        Position = Target - Front * Distance;
    }
};

} // namespace Core