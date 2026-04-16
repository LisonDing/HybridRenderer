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

// 【新增】用于在拖拽结束后把鼠标完美放回原位的锚点
static double clickAnchorX = WIDTH / 2.0;
static double clickAnchorY = HEIGHT / 2.0;
static bool ignoreFirstDelta = false;

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
    // 【Blender 逻辑】只侦听鼠标中键 (Middle Mouse Button)
    bool isMiddlePressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    
    // 侦听 Shift 键状态 (左右 Shift 皆可)
    bool isShiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || 
                          glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    // Only track movement if a button is held down.
    if (isMiddlePressed) {
        if (!isDragging) {
            // 记录点击瞬间的锚点
            glfwGetCursorPos(window, &clickAnchorX, &clickAnchorY);
            
            // 使用隐藏模式，突破屏幕物理边缘限制
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            
            isDragging = true;
            ignoreFirstDelta = true; 
            return;
        }

        if (ignoreFirstDelta) {
            lastX = xpos;
            lastY = ypos;
            ignoreFirstDelta = false;
            return;
        }

        float xoffset = xpos - lastX;
        float yoffset = ypos - lastY; 
        lastX = xpos;
        lastY = ypos;

        // 【核心：Blender 无限屏幕环绕 (Continuous Grab)】
        if (isShiftPressed) {
                camera.ProcessPan(xoffset, yoffset);   // Shift + 中键：平移画布
            } else {
                camera.ProcessOrbit(xoffset, yoffset); // 中键：环绕物体 (Z轴向上的 Turntable)
        }
    } else {
        if (isDragging) {
            // 3. 拖拽结束：恢复可见模式
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            // 4. 瞬间将指针拉回一开始点击的原位，维持完美的交互错觉
            glfwSetCursorPos(window, clickAnchorX, clickAnchorY);
            isDragging = false;
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // 【Blender 逻辑】滚轮 = 以焦点为基准拉近/推远 (Zoom)
    // 稍微调高一点灵敏度，让滚轮缩放更爽快
    camera.ProcessZoom(static_cast<float>(yoffset), 0.8f); 
}

void processInput(GLFWwindow *window) {
    // ESC 退出
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // 【Blender 快捷键：小键盘 1/3/7 切换正交视图，或者 Home 键全局居中】
    // 这里保留一个快捷键 F：一键将焦点归零，相当于 Blender 的 Numpad '.' (View Selected)
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        camera.ResetFocus(glm::vec3(0.0f, 0.0f, 0.0f));
    }
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
    
    vkContext.LoadModel(); // 加载模型数据到内存，准备上传到 GPU

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