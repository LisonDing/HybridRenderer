#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Camera.h"
#include "../Renderer/VulkanContext.h"
#include "../Editor/ImGuiLayer.h"

namespace Core {

    class Application {
    public:
        Application();
        ~Application();

        // 引擎的主入口点
        void Run();

    private:
        void InitWindow();
        void InitVulkan();
        void MainLoop();
        void Cleanup();
        void ProcessInput();

        // 【核心架构技巧】：由于 GLFW 的回调是 C 函数指针，无法直接传递 C++ 类的成员函数。
        // 我们必须定义静态的回调函数，并在内部将其转发给具体的类实例。
        static void MouseCallback(GLFWwindow* window, double xpos, double ypos);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

        // 系统核心模块
        GLFWwindow* m_Window;
        uint32_t m_Width = 1080;
        uint32_t m_Height = 720;

        Renderer::VulkanContext m_VkContext;
        UI::ImGuiLayer m_ImGuiLayer;
        Camera m_Camera;

        // 交互与时间状态 (彻底告别全局变量)
        float m_LastX;
        float m_LastY;
        bool m_IsDragging;
        double m_ClickAnchorX;
        double m_ClickAnchorY;
        bool m_IgnoreFirstDelta;

        float m_DeltaTime;
        float m_LastFrame;

        // UI 渲染参数
        glm::vec3 m_LightDir;
        float m_Metallic;
        float m_Roughness;
        float m_Exposure;
        float m_VignetteStrength;
    };

} // namespace Core