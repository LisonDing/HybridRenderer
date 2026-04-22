#pragma once

#include "glm/fwd.hpp"
#include <sys/types.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <array>
#include <fstream>
#include <unordered_map> // 用于顶点去重的哈希表

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL // 启用 GLM 的实验性功能，以使用 std::hash 支持 glm::vec3 和 glm::vec2
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <chrono>

namespace Renderer {

// Standard vertex structure aligning with shader inputs.
// 标准顶点结构体，需与着色器输入布局严格对齐。
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord; 
    glm::vec3 normal; // 【新增】顶点法线向量，用于光照计算

    // 更新相等运算符
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
    }

    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    // 将属性数量从 3 提升到 4
    static std::array<VkVertexInputAttributeDescription, 4> GetAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        // 【新增】法线属性映射
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, normal);

        return attributeDescriptions;
    }
};

} // namespace Renderer （临时闭合，注入std）

namespace std {
    // Inject custom hash function for the Vertex struct into the std namespace.
    // 将 Vertex 结构体的自定义哈希函数注入 std 命名空间。
    template<> struct hash<Renderer::Vertex> {
        size_t operator()(Renderer::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1) ^
                   (hash<glm::vec3>()(vertex.normal) << 1); // 【新增】将法线纳入哈希计算
        }
    };
} // namespace std

namespace Renderer {
// Uniform Buffer Object structure. Memory alignment is strictly enforced (16 bytes).
// 统一缓冲对象结构体。强制实行 16 字节内存对齐，以符合 Vulkan 规范。
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec3 lightDir; // 【新增】平行光方向
    alignas(16) glm::vec3 viewPos;  // 【新增】摄像机世界坐标
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

    // --- Helper Functions for Commands & Images ---
    // 命令录制与图像处理的底层辅助函数
    VkCommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
    void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkImageView CreateImageView(VkImage image, VkFormat format, uint32_t mipLevels); // Refactored for reuse / 提取出来以便复用

    // --- Texture Resources ---
    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();

    // --- Depth Testing & Blending ---
    void CreateDepthResources();

    // --- Model Loading & Asset Management ---
    void LoadModel();

    // --- Execution ---
    void DrawFrame(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos); 
    void Cleanup();

private:
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void UpdateUniformBuffer(uint32_t currentImage, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& viewPos);
    static std::vector<char> ReadFile(const std::string& filename);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

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

    // Texture Resources
    // 纹理贴图
    uint32_t       m_MipLevels = 1; // 纹理 mipmap 层级数量
    VkImage        m_TextureImage = VK_NULL_HANDLE;
    VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
    VkImageView    m_TextureImageView = VK_NULL_HANDLE;
    VkSampler      m_TextureSampler = VK_NULL_HANDLE;

    // Model Data
    // 模型数据
    std::vector<Vertex> m_Vertices;
    std::vector<uint32_t> m_Indices;


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

    // Depth Resources
    // 深度测试资源
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat FindDepthFormat();
    VkImage        m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView    m_DepthImageView = VK_NULL_HANDLE;

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