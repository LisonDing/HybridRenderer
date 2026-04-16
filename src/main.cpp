#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdlib>

#include "Core/Logger.h"
#include "Core/Camera.h"
#include "Renderer/VulkanContext.h"

// Constants and Global Camera State
// 常量与全局摄像机状态 (由于 GLFW C-API 回调的限制，需要使用全局或静态变量)
const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// 在 WIDTH 和 HEIGHT 下方，替换原有的 Camera 状态
Core::Camera camera(glm::vec3(0.0f, 0.0f, 0.0f), 4.0f); // 目标点在原点，距离 4.0
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;
bool isDragging = false; // 记录鼠标是否在拖拽

// Timing mechanisms to ensure smooth movement regardless of framerate.
// 时间机制，确保不同帧率下的移动速度恒定。
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse movement callback.
// 鼠标位移回调函数。
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    // Get current mouse button states.
    // 获取当前鼠标左右键的状态。
    bool isLeftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool isRightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    // Only track movement if a button is held down.
    if (isLeftPressed || isRightPressed) {
        if (!isDragging) {
            lastX = xpos;
            lastY = ypos;
            isDragging = true;
        }

        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY;
        lastX = xpos;
        lastY = ypos;

        // Dispatch to corresponding camera behaviors.
        // 根据按键状态分发至不同的摄像机行为。
        if (isLeftPressed) {
            camera.ProcessOrbit(xoffset, yoffset); // 左键：环绕
        } else if (isRightPressed) {
            camera.ProcessPan(xoffset, yoffset);   // 右键：平移
        }
    } else {
        isDragging = false;
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    camera.ProcessZoom(static_cast<float>(yoffset)); // 滚轮：推拉缩放
}

void processInput(GLFWwindow *window) {

    // Critical : Emerqency exit on ESC key press, to prevent lockup during development.
    // 关键：ESC 键紧急退出，防止开发过程中程序锁死
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Hybrid FPS navigation using WASD keys, moving the camera and target together.
    // 混合 FPS 导航，使用 WASD 键同时移动摄像机和目标点。
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(Core::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(Core::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(Core::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(Core::RIGHT, deltaTime);
}

int main() {
    Core::Logger::Init();
    HR_LOG_INFO("--- System Boot ---");

    // Initialize GLFW and disable default OpenGL context.
    // 初始化 GLFW 窗口系统，并禁用默认的 OpenGL 上下文绑定。
    if (!glfwInit()) {
        HR_LOG_ERROR("Failed to initialize GLFW!");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hybrid Renderer - Core", nullptr, nullptr);

    // Bind GLFW input configuration.
    // 绑定 GLFW 鼠标隐藏与回调。
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // Retrieve required Vulkan instance extensions from GLFW.
    // 从 GLFW 获取窗口系统所需的 Vulkan 实例扩展列表。
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    Renderer::VulkanContext vkContext;
    if (!vkContext.Init(requiredExtensions)) return EXIT_FAILURE;

    // Create OS-specific window surface.
    // 创建与操作系统绑定的窗口呈现表面 (Surface)。
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(vkContext.GetInstance(), window, nullptr, &surface) != VK_SUCCESS) {
        HR_LOG_ERROR("Failed to create Window Surface!");
        return EXIT_FAILURE;
    }
    vkContext.SetSurface(surface);

    // ==========================================
    // Phase 1: Hardware & Rendering Infrastructure
    // 阶段 1：硬件设备与渲染基础设施初始化
    // ==========================================
    vkContext.PickPhysicalDevice(); // 选择物理设备（GPU）
    vkContext.CreateLogicalDevice(); // 创建逻辑设备与获取队列
    vkContext.CreateSwapchain(WIDTH, HEIGHT); // 创建交换链与相关图像资源
    vkContext.CreateImageViews(); // 创建交换链图像视图
    vkContext.CreateRenderPass(); // 创建渲染通道，定义渲染流程与附件格式
    vkContext.CreateDescriptorSetLayout(); // 创建描述符集布局，定义着色器资源绑定
    vkContext.CreateGraphicsPipeline(); // 创建图形管线，绑定着色器与固定功能状态
    vkContext.CreateDepthResources(); // 创建深度缓冲区资源
    vkContext.CreateFramebuffers(); // 创建帧缓冲区，绑定交换链图像视图与深度视图

    // ==========================================
    // Phase 2: Command & Synchronization Systems
    // 阶段 2：命令池与同步信号量系统构建
    // ==========================================
    vkContext.CreateCommandPool(); // 创建命令池，管理命令缓冲的分配与重置
    vkContext.CreateCommandBuffer(); // 分配命令缓冲区，用于记录渲染命令
    vkContext.CreateSyncObjects(); // 创建同步对象（信号量与栅栏），协调 CPU-GPU 工作流

    // ==========================================
    // Phase 3: Assets & Memory Management
    // 阶段 3：资产加载、内存分配与描述符绑定
    // ==========================================
    vkContext.CreateTextureImage(); // 加载纹理图像并分配 GPU 内存, 必须在描述符集创建前完成
    vkContext.CreateTextureImageView(); // 创建纹理图像视图，抽象底层图像资源, 用于识别纹理属性
    vkContext.CreateTextureSampler(); // 创建纹理采样器，定义纹理过滤与边界处理方式
    
    vkContext.CreateVertexBuffer(); // 创建顶点缓冲区并上传顶点数据
    vkContext.CreateIndexBuffer(); // 创建索引缓冲区并上传索引数据
    vkContext.CreateUniformBuffers(); // 创建统一缓冲区，用于存储 MVP 矩阵等动态数据
    vkContext.CreateDescriptorPool(); // 创建描述符池，管理描述符集的分配
    vkContext.CreateDescriptorSets(); // 创建描述符集，绑定纹理与 uniform 数据，必须在纹理资源创建完成后进行绑定
    // ==========================================
    // Phase 4: Main Execution Loop
    // 阶段 4：引擎主执行循环
    // ==========================================
    while (!glfwWindowShouldClose(window)) {
        // Frame timing calculations.
        // 帧时间差计算。
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Process inputs and dispatch events.
        // 处理键盘输入与事件分发。
        processInput(window);
        glfwPollEvents();

        // Compute current View and Projection matrices.
        // 计算当前帧的观察与投影矩阵。
        glm::mat4 view = camera.GetViewMatrix();
        // glm::mat4 proj = glm::perspective(glm::radians(camera.Zoom), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

        // Inject matrices into the rendering pipeline.
        // 将矩阵注入渲染管线。
        vkContext.DrawFrame(view, proj); 
    }
    
    vkDeviceWaitIdle(vkContext.GetDevice()); 

    HR_LOG_INFO("--- System Shutdown ---");
    vkContext.Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}