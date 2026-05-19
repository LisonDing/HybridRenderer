#pragma once
#include <vulkan/vulkan.h>
#include <string>

// 前置声明，避免头文件循环依赖
namespace Renderer { class VulkanContext; }

namespace Resource {
    class Texture2D {
    public:
        Texture2D() = default;
        ~Texture2D() = default;

        // 传入 VulkanContext 引用，以便调用底层的显存分配和命令缓冲接口
        bool LoadFromFile(const std::string& path, Renderer::VulkanContext& vkContext);
        void Cleanup(VkDevice device);

        VkImageView GetImageView() const { return m_ImageView; }
        VkSampler GetSampler() const { return m_Sampler; }

    private:
        uint32_t m_MipLevels = 1;
        VkImage m_Image = VK_NULL_HANDLE;
        VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
        VkImageView m_ImageView = VK_NULL_HANDLE;
        VkSampler m_Sampler = VK_NULL_HANDLE;
        
        void CreateTextureSampler(VkDevice device);
    };
}