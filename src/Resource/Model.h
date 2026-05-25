#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <array>
#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "../Resource/Texture.h"

namespace Renderer { class VulkanContext; }

namespace Renderer {
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord; 
        glm::vec3 normal;
        glm::vec4 tangent;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && 
                   texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
        }
        static VkVertexInputBindingDescription GetBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }
        static std::array<VkVertexInputAttributeDescription, 5> GetAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};
            
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
            
            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(Vertex, normal);

            // 切线属性
            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(Vertex, tangent);

            return attributeDescriptions;
        }
    };

    
} // namespace Renderer

namespace std {
    template<> struct hash<Renderer::Vertex> {
        size_t operator()(Renderer::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1) ^ (hash<glm::vec3>()(vertex.normal) << 1) ^ (hash<glm::vec3>()(vertex.tangent) << 1);
        }
    };
}

namespace Resource {
    
    // 【新增】工业级材质参数定义
    struct Material {
        std::string name;
        std::string albedoMapPath;
        std::string normalMapPath;
        std::string metallicRoughnessMapPath;
        
        // 当没有贴图时的默认 PBR 物理值
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        float metallicFactor = 0.0f;
        float roughnessFactor = 0.5f;

        // 【新增】：真正在显存中的贴图对象指针
        std::shared_ptr<Texture2D> albedoTexture;
        std::shared_ptr<Texture2D> normalTexture;
        std::shared_ptr<Resource::Texture2D> metallicRoughnessTexture; // 根据你的命名空间调整
    };

    // 【新增】子网格定义 (拆分 Draw Call 的最小单元)
    struct SubMesh {
        uint32_t firstIndex;    // 该部件在全局 IBO 中的起始位置
        uint32_t indexCount;    // 该部件包含的索引数量
        uint32_t materialIndex; // 该部件绑定的材质 ID
    };

    class Model {
    public:
        Model() = default;
        ~Model() = default;

        // 【重命名】专门加载 GLTF
        bool LoadGLTF(const std::string& path, Renderer::VulkanContext& vkContext);
        void Cleanup(VkDevice device);

        VkBuffer GetVertexBuffer() const { return m_VertexBuffer; }
        VkBuffer GetIndexBuffer() const { return m_IndexBuffer; }
        uint32_t GetIndexCount() const { return m_IndexCount; }

        // 获取资产数据
        const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }
        const std::vector<Material>& GetMaterials() const { return m_Materials; }

    private:
        VkBuffer m_VertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_VertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer m_IndexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_IndexBufferMemory = VK_NULL_HANDLE;
        uint32_t m_IndexCount = 0;

        std::vector<SubMesh> m_SubMeshes;
        std::vector<Material> m_Materials;
    };
}