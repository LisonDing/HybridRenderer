#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <cstdlib>

#include "Core/Logger.h"
#include "Renderer/VulkanContext.h"

int main() {
    Core::Logger::Init();
    HR_LOG_INFO("--- System Boot ---");

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    // Initialize GLFW and disable default OpenGL context.
    // 初始化 GLFW 窗口系统，并禁用默认的 OpenGL 上下文绑定。
    if (!glfwInit()) {
        HR_LOG_ERROR("Failed to initialize GLFW!");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hybrid Renderer - Core", nullptr, nullptr);

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
    vkContext.PickPhysicalDevice();
    vkContext.CreateLogicalDevice();
    vkContext.CreateSwapchain(WIDTH, HEIGHT);
    vkContext.CreateImageViews();
    vkContext.CreateRenderPass();
    vkContext.CreateDescriptorSetLayout();
    vkContext.CreateGraphicsPipeline();
    vkContext.CreateFramebuffers();

    // ==========================================
    // Phase 2: Command & Synchronization Systems
    // 阶段 2：命令池与同步信号量系统构建
    // ==========================================
    vkContext.CreateCommandPool();
    vkContext.CreateCommandBuffer();
    vkContext.CreateSyncObjects();

    // ==========================================
    // Phase 3: Assets & Memory Management
    // 阶段 3：资产加载、内存分配与描述符绑定
    // ==========================================
    vkContext.CreateVertexBuffer();
    vkContext.CreateIndexBuffer();
    vkContext.CreateUniformBuffers();
    vkContext.CreateDescriptorPool();
    vkContext.CreateDescriptorSets();
    
    // ==========================================
    // Phase 4: Main Execution Loop
    // 阶段 4：引擎主执行循环
    // ==========================================
    HR_LOG_INFO("Entering Main Loop...");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        vkContext.DrawFrame(); 
    }
    
    // Await GPU idle state before resource destruction.
    // 强制挂起 CPU，等待 GPU 执行完毕所有命令，防止析构时发生显存非法访问。
    vkDeviceWaitIdle(vkContext.GetDevice()); 

    HR_LOG_INFO("--- System Shutdown ---");
    vkContext.Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}