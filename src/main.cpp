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

    // 1. 初始化窗口系统
    if (!glfwInit()) {
        HR_LOG_ERROR("Failed to initialize GLFW!");
        return EXIT_FAILURE;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Hybrid Renderer - Swapchain", nullptr, nullptr);

    // 2. 获取窗口系统需要的 Vulkan 扩展
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // 3. 初始化渲染上下文
    Renderer::VulkanContext vkContext;
    if (!vkContext.Init(requiredExtensions)) {
        return EXIT_FAILURE;
    }
    // 4. 创建窗口表面 (Surface)，并把它交给 VulkanContext 管理
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(vkContext.GetInstance(), window, nullptr, &surface) != VK_SUCCESS) {
        HR_LOG_ERROR("Failed to create Window Surface!");
        return EXIT_FAILURE;
    }
    vkContext.SetSurface(surface);

    // 4. Vulkan 硬件配置
    // 挑选物理显卡
    vkContext.PickPhysicalDevice();
    // 创建逻辑设备和队列
    vkContext.CreateLogicalDevice();
    // 创建交换链
    vkContext.CreateSwapchain(WIDTH, HEIGHT);

    // 引擎主循环
    HR_LOG_INFO("Entering Main Loop...");
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        // 此处未来将调用 vkContext.DrawFrame() 等渲染指令
    }

    // 7. 系统关闭与清理
    HR_LOG_INFO("--- System Shutdown ---");
    vkContext.Cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}