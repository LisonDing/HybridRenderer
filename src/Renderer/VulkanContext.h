// 目的为Vulkan状态管理

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional> // 用于安全地检查值是否存在

// 辅助结构体：记录找到的车间（队列族）的编号
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily; // 负责把画面送到屏幕的“显示车间”

    // 当所有的 std::optional 都有值时，说明找齐了需要的车间
    bool IsComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

namespace Renderer {

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    // 【C++ 架构规范】：禁用拷贝构造和拷贝赋值
    // Vulkan 句柄（如 VkInstance）本质上是指向显存或驱动资源的指针。
    // 如果允许复制这个类，会导致析构时同一份资源被销毁两次 (Double Free)。
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // 初始化 Vulkan 实例，接收从上层窗口系统传来的扩展需求
    bool Init(const std::vector<const char*>& windowExtensions);
    
    // 接收外部创建的 Surface
    void SetSurface(VkSurfaceKHR surface) { m_Surface = surface; }

    // 提供 Instance 供外部创建 Surface
    VkInstance GetInstance() const { return m_Instance; }

    // 挑选最合适的物理显卡
    void PickPhysicalDevice(); 

    // 创建逻辑设备
    void CreateLogicalDevice();
    
    // 创建交换链（画板翻滚系统）
    void CreateSwapchain(uint32_t width, uint32_t height);

    // 清理资源
    void Cleanup();

private:
    // 【Vulkan 教学】：句柄 (Handle)
    // 所有以 Vk 开头的类型（没有指针星号），大多是 Vulkan 的不透明句柄 (Opaque Handle)。
    // 你可以把它们理解为一串 uint64_t 的 ID，用来在显卡驱动里查表找资源。
    // 习惯上将其初始化为 VK_NULL_HANDLE (也就是 0)。

    // 内部辅助函数：寻找满足需求的队列族
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    
    VkInstance       m_Instance       = VK_NULL_HANDLE; // 应用程序与 Vulkan 库的连接桥梁
    
    VkSurfaceKHR     m_Surface        = VK_NULL_HANDLE; // 【新增】画布

    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE; // 物理显卡的抽象表示
    VkDevice         m_Device         = VK_NULL_HANDLE; // 逻辑设备，代表与物理设备的连接

    VkQueue          m_GraphicsQueue  = VK_NULL_HANDLE; // 图形队列的句柄，用于提交绘制命令
    VkQueue          m_PresentQueue   = VK_NULL_HANDLE; // 【新增】显示队列

    VkSwapchainKHR   m_Swapchain      = VK_NULL_HANDLE; // 【新增】交换链
};

} // namespace Renderer