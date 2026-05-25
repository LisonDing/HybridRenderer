#include "Application.h"
#include "Logger.h"
#include <cstdlib>

namespace Core {

    Application::Application() 
        : m_Camera(glm::vec3(0.0f, 100.0f, 0.0f), 0.1f, 0.0f, 0.0f),
          m_LastX(400.0f), m_LastY(300.0f), m_IsDragging(false),
          m_ClickAnchorX(400.0), m_ClickAnchorY(300.0), m_IgnoreFirstDelta(false),
          m_DeltaTime(0.0f), m_LastFrame(0.0f),
          m_LightDir(glm::vec3(5.0f, 10.0f, 3.0f)), m_Metallic(0.5f), 
          m_Roughness(0.5f), m_Exposure(1.0f), m_VignetteStrength(0.5f) {
    }

    Application::~Application() {
    }

    void Application::Run() {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

    void Application::InitWindow() {
        if (!glfwInit()) {
            HR_LOG_ERROR("Application: Failed to initialize GLFW!");
            exit(EXIT_FAILURE);
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_Window = glfwCreateWindow(m_Width, m_Height, "Hybrid Renderer - Pro", nullptr, nullptr);

        // 【关键技巧】：把当前 Application 实例的指针绑定到 GLFW 窗口里
        glfwSetWindowUserPointer(m_Window, this);

        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetCursorPosCallback(m_Window, MouseCallback);
        glfwSetScrollCallback(m_Window, ScrollCallback);
    }

    void Application::InitVulkan() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (!m_VkContext.Init(requiredExtensions)) exit(EXIT_FAILURE);

        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(m_VkContext.GetInstance(), m_Window, nullptr, &surface) != VK_SUCCESS) {
            HR_LOG_ERROR("Application: Failed to create Window Surface!");
            exit(EXIT_FAILURE);
        }
        m_VkContext.SetSurface(surface);

        // Phase 1: Infrastructure
        m_VkContext.PickPhysicalDevice();
        m_VkContext.CreateLogicalDevice();
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_Window, &fbWidth, &fbHeight);
        m_VkContext.CreateSwapchain(fbWidth, fbHeight);
        m_VkContext.CreateImageViews();
        m_VkContext.CreateRenderPass();
        m_VkContext.CreateDescriptorSetLayout();
        m_VkContext.CreateGraphicsPipeline();
        m_VkContext.CreateDepthResources();
        m_VkContext.CreateOffscreenResources();
        m_VkContext.CreateShadowResources();
        m_VkContext.CreateShadowPipeline();
        m_VkContext.CreatePostProcessRenderPass();
        m_VkContext.CreateFramebuffers();

        // Phase 2: Command & Sync
        m_VkContext.CreateCommandPool();
        m_VkContext.CreateCommandBuffer();
        m_VkContext.CreateSyncObjects();

        // Phase 3: Assets
        m_VkContext.LoadTextures({"../assets/textures/viking_room.png"}, {"../assets/textures/normalMap_test.jpeg"});
        // m_VkContext.LoadTextures({"../assets/textures/viking_room.png"}, nullptr);
        // m_VkContext.LoadModel("../assets/models/viking_room.obj");
        m_VkContext.LoadModel("../assets/models/Sponza/glTF/Sponza.gltf");
        m_VkContext.CreateUniformBuffers();
        m_VkContext.CreatePostProcessPipeline();
        m_VkContext.CreateDescriptorPool();
        m_VkContext.CreateDescriptorSets();

        // UI Layer Init
        m_ImGuiLayer.Init(m_Window, m_VkContext.GetInstance(), m_VkContext.GetPhysicalDevice(), m_VkContext.GetDevice(),
                          m_VkContext.GetGraphicsQueueFamily(), m_VkContext.GetGraphicsQueue(), m_VkContext.GetPostProcessRenderPass());
    }

    void Application::MainLoop() {
        while (!glfwWindowShouldClose(m_Window)) {
            float currentFrame = static_cast<float>(glfwGetTime());
            m_DeltaTime = currentFrame - m_LastFrame;
            m_LastFrame = currentFrame;

            ProcessInput();
            glfwPollEvents();

            // 1. UI 渲染逻辑
            m_ImGuiLayer.NewFrame();
            m_ImGuiLayer.RenderControlPanel(m_LightDir, m_Metallic, m_Roughness, m_Exposure, m_VignetteStrength);

            // 2. 矩阵计算
            glm::mat4 view = m_Camera.GetViewMatrix();
            glm::mat4 proj = glm::perspective(glm::radians(75.0f), (float)m_Width / (float)m_Height, 0.1f, 2000.0f);

            // 3. 将 UI 绘制回调动态注入到管线渲染中
            m_VkContext.DrawFrame(view, proj, m_Camera.Position, m_LightDir, m_Metallic, m_Roughness, m_Exposure, m_VignetteStrength, [&](VkCommandBuffer cmd) {
                m_ImGuiLayer.RenderDrawData(cmd);
            });
        }
        vkDeviceWaitIdle(m_VkContext.GetDevice());
    }

    void Application::Cleanup() {
        m_ImGuiLayer.Cleanup();
        m_VkContext.Cleanup();
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }

    void Application::ProcessInput() {
        if (UI::ImGuiLayer::WantCaptureKeyboard()) return;

        if (glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(m_Window, true);

        if (glfwGetKey(m_Window, GLFW_KEY_F) == GLFW_PRESS) {
            m_Camera.ResetFocus(glm::vec3(0.0f, 0.0f, 0.0f));
        }
    }

    // 静态回调函数的实现
    void Application::MouseCallback(GLFWwindow* window, double xposIn, double yposIn) {
        if (UI::ImGuiLayer::WantCaptureMouse()) return;

        // 从窗口指针中提取出 Application 实例
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));

        float xpos = static_cast<float>(xposIn);
        float ypos = static_cast<float>(yposIn);

        bool isMiddlePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        bool isShiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        if (isMiddlePressed) {
            if (!app->m_IsDragging) {
                glfwGetCursorPos(window, &app->m_ClickAnchorX, &app->m_ClickAnchorY);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
                app->m_IsDragging = true;
                app->m_IgnoreFirstDelta = true;
                return;
            }
            if (app->m_IgnoreFirstDelta) {
                app->m_LastX = xpos;
                app->m_LastY = ypos;
                app->m_IgnoreFirstDelta = false;
                return;
            }
            float xoffset = xpos - app->m_LastX;
            float yoffset = ypos - app->m_LastY;
            app->m_LastX = xpos;
            app->m_LastY = ypos;

            if (isShiftPressed) {
                app->m_Camera.ProcessPan(xoffset * 30.0f, yoffset * 30.0f);
            } else {
                app->m_Camera.ProcessOrbit(xoffset, yoffset);
            }
        } else {
            if (app->m_IsDragging) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                glfwSetCursorPos(window, app->m_ClickAnchorX, app->m_ClickAnchorY);
                app->m_IsDragging = false;
            }
        }
    }

    void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        if (UI::ImGuiLayer::WantCaptureMouse()) return;
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->m_Camera.ProcessZoom(static_cast<float>(yoffset), 5.0f);
    }

} // namespace Core