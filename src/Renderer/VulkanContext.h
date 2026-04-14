#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <array>
#include <fstream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace Renderer {

// Standard vertex structure aligning with shader inputs.
// 标准顶点结构体，需与着色器输入布局严格对齐。
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord; // New: Texture Coordinates (UV) / 新增：纹理坐标 (UV)

    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions() {
        // Expanded to 3 attributes.
        // 扩展为 3 个属性描述符。
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // Texture coordinate attribute.
        // 纹理坐标属性，对应 location = 2。
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

// Uniform Buffer Object structure. Memory alignment is strictly enforced (16 bytes).
// 统一缓冲对象结构体。强制实行 16 字节内存对齐，以符合 Vulkan 规范。
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// Stores the indices of available Vulkan queue families.
// 存储可用 Vulkan 队列族的索引信息。
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool IsComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    // Disable copy semantics to prevent double-freeing of Vulkan handles.
    // 禁用拷贝构造与赋值，防止 Vulkan 底层句柄被重复释放。
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // --- Initialization & Core Systems ---
    bool Init(const std::vector<const char*>& windowExtensions);
    void PickPhysicalDevice(); 
    void CreateLogicalDevice();
    void SetSurface(VkSurfaceKHR surface) { m_Surface = surface; }
    VkInstance GetInstance() const { return m_Instance; }
    VkDevice GetDevice() const { return m_Device; }

    // --- Swapchain & Pipeline ---
    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    void CreateFramebuffers();

    // --- Commands & Synchronization ---
    void CreateCommandPool();
    void CreateCommandBuffer();
    void CreateSyncObjects();

    // --- Memory & Buffers ---
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // --- Execution ---
    void DrawFrame(); 
    void Cleanup();

private:
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void UpdateUniformBuffer(uint32_t currentImage);
    static std::vector<char> ReadFile(const std::string& filename);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);

    // Core Vulkan Handles
    // Vulkan 核心层级句柄
    VkInstance       m_Instance       = VK_NULL_HANDLE;
    VkSurfaceKHR     m_Surface        = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice         m_Device         = VK_NULL_HANDLE;
    VkQueue          m_GraphicsQueue  = VK_NULL_HANDLE;
    VkQueue          m_PresentQueue   = VK_NULL_HANDLE;

    // Swapchain Resources
    // 交换链相关资源
    VkSwapchainKHR   m_Swapchain      = VK_NULL_HANDLE;
    VkFormat         m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D       m_SwapchainExtent = {0, 0};
    std::vector<VkImage> m_SwapchainImages;
    std::vector<VkImageView> m_SwapchainImageViews;
    std::vector<VkFramebuffer> m_SwapchainFramebuffers;

    // Pipeline & Rendering
    // 渲染管线相关资源
    VkRenderPass     m_RenderPass     = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_GraphicsPipeline = VK_NULL_HANDLE;

    // Command & Synchronization
    // 命令与同步对象
    VkCommandPool   m_CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
    VkSemaphore     m_ImageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore     m_RenderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence         m_InFlightFence = VK_NULL_HANDLE;

    // Buffers & Memory
    // 缓冲与物理显存
    VkBuffer       m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer       m_IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;

    // Descriptor Sets
    // 描述符与统一缓冲资源
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_DescriptorSets;
    std::vector<VkBuffer>       m_UniformBuffers;
    std::vector<VkDeviceMemory> m_UniformBuffersMemory;
    std::vector<void*>          m_UniformBuffersMapped;
};

} // namespace Renderer