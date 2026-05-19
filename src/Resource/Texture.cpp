// 【核心优化】：将 STB_IMAGE 隔离在这个独立文件中
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include "../Renderer/VulkanContext.h"
#include "../Core/Logger.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace Resource {

    bool Texture2D::LoadFromFile(const std::string& path, Renderer::VulkanContext& vkContext) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            HR_LOG_ERROR("Texture2D: Failed to load texture from " + path);
            return false;
        }

        m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        // 调用底层 RHI 接口分配显存
        vkContext.CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(vkContext.GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(vkContext.GetDevice(), stagingBufferMemory);

        stbi_image_free(pixels);

        VkFormat safeFormat = VK_FORMAT_R8G8B8A8_SRGB;

        // 向 GPU 申请图像空间并转移布局
        vkContext.CreateImage(texWidth, texHeight, m_MipLevels, VK_SAMPLE_COUNT_1_BIT, safeFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_ImageMemory);
        vkContext.TransitionImageLayout(m_Image, safeFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_MipLevels);
        vkContext.CopyBufferToImage(stagingBuffer, m_Image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        
        // 生成 Mipmap
        vkContext.GenerateMipmaps(m_Image, safeFormat, texWidth, texHeight, m_MipLevels);

        vkDestroyBuffer(vkContext.GetDevice(), stagingBuffer, nullptr);
        vkFreeMemory(vkContext.GetDevice(), stagingBufferMemory, nullptr);

        m_ImageView = vkContext.CreateImageView(m_Image, safeFormat, m_MipLevels);
        CreateTextureSampler(vkContext.GetDevice());

        HR_LOG_INFO("Texture2D: Loaded successfully -> " + path);
        return true;
    }

    void Texture2D::CreateTextureSampler(VkDevice device) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = static_cast<float>(m_MipLevels);
        samplerInfo.mipLodBias = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
            HR_LOG_ERROR("Texture2D: Failed to create texture sampler!");
        }
    }

    void Texture2D::Cleanup(VkDevice device) {
        if (m_Sampler != VK_NULL_HANDLE) vkDestroySampler(device, m_Sampler, nullptr);
        if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_ImageView, nullptr);
        if (m_Image != VK_NULL_HANDLE) {
            vkDestroyImage(device, m_Image, nullptr);
            vkFreeMemory(device, m_ImageMemory, nullptr);
        }
    }
}