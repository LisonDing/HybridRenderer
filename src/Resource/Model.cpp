// 【核心架构魔法】：禁止 tinygltf 自行解码图片，防止与我们的 Texture 类产生 stb_image 宏冲突！
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE 
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include "../external/tinygltf/tiny_gltf.h"

#include "Model.h"
#include "../Renderer/VulkanContext.h"
#include "../Core/Logger.h"
#include <iostream>

namespace Resource {

    bool Model::LoadGLTF(const std::string& path, Renderer::VulkanContext& vkContext) {
        tinygltf::Model gltfModel;
        // 贴图缓存：路径 -> 贴图对象
        std::unordered_map<std::string, std::shared_ptr<Texture2D>> textureCache;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        HR_LOG_INFO("Model: Parsing glTF file -> " + path);
        bool ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, path);
        if (!warn.empty()) HR_LOG_INFO("glTF Warn: " + warn);
        if (!err.empty()) HR_LOG_ERROR("glTF Error: " + err);
        if (!ret) return false;

        // 获取文件所在目录，用于拼接贴图的绝对路径
        std::string baseDir = path.substr(0, path.find_last_of('/') + 1);

        // ==========================================
        // 1. 解析材质 (Materials & Textures)
        // ==========================================
        for (const auto& mat : gltfModel.materials) {
            Material material{};
            material.name = mat.name;

            // 提取 PBR 基础参数
            if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                material.baseColorFactor = glm::make_vec4(mat.pbrMetallicRoughness.baseColorFactor.data());
            }
            material.metallicFactor = mat.pbrMetallicRoughness.metallicFactor;
            material.roughnessFactor = mat.pbrMetallicRoughness.roughnessFactor;

            // 提取 Albedo 贴图路径
            int baseColorTexIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorTexIndex >= 0) {
                int imageIndex = gltfModel.textures[baseColorTexIndex].source;
                material.albedoMapPath = baseDir + gltfModel.images[imageIndex].uri;
            }

            // 提取 Normal 贴图路径
            int normalTexIndex = mat.normalTexture.index;
            if (normalTexIndex >= 0) {
                int imageIndex = gltfModel.textures[normalTexIndex].source;
                material.normalMapPath = baseDir + gltfModel.images[imageIndex].uri;
            }

            // 提取 Metallic/Roughness 贴图路径
            int mrTexIndex = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (mrTexIndex >= 0) {
                int imageIndex = gltfModel.textures[mrTexIndex].source;
                material.metallicRoughnessMapPath = baseDir + gltfModel.images[imageIndex].uri;
            }

            // 1. Albedo 贴图
            if (!material.albedoMapPath.empty()) {
                if (textureCache.find(material.albedoMapPath) == textureCache.end()) {
                    auto tex = std::make_shared<Texture2D>();
                    tex->LoadFromFile(material.albedoMapPath, vkContext); 
                    textureCache[material.albedoMapPath] = tex;
                }
                material.albedoTexture = textureCache[material.albedoMapPath];
            }

            // 2. Normal 贴图 (同理)
            if (!material.normalMapPath.empty()) {
                if (textureCache.find(material.normalMapPath) == textureCache.end()) {
                    auto tex = std::make_shared<Texture2D>();
                    tex->LoadFromFile(material.normalMapPath, vkContext);
                    textureCache[material.normalMapPath] = tex;
                }
                material.normalTexture = textureCache[material.normalMapPath];
            }

            // 3. MetallicRoughness 贴图 (同理)
            if (!material.metallicRoughnessMapPath.empty()) {
                if (textureCache.find(material.metallicRoughnessMapPath) == textureCache.end()) {
                    auto tex = std::make_shared<Texture2D>();
                    tex->LoadFromFile(material.metallicRoughnessMapPath, vkContext);
                    textureCache[material.metallicRoughnessMapPath] = tex;
                }
                material.metallicRoughnessTexture = textureCache[material.metallicRoughnessMapPath];
            }

            m_Materials.push_back(material);
        }

        // ==========================================
        // 2. 解析几何数据 (Meshes & Accessors)
        // ==========================================
        std::vector<Renderer::Vertex> globalVertices;
        std::vector<uint32_t> globalIndices;

        for (const auto& mesh : gltfModel.meshes) {
            for (const auto& primitive : mesh.primitives) {
                SubMesh subMesh{};
                subMesh.firstIndex = static_cast<uint32_t>(globalIndices.size());
                subMesh.materialIndex = primitive.material >= 0 ? primitive.material : 0;

                uint32_t vertexStart = static_cast<uint32_t>(globalVertices.size());

                // --- 提取顶点位置 (Position) ---
                const float* positionBuffer = nullptr;
                size_t vertexCount = 0;
                if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }

                // --- 提取法线 (Normal) ---
                const float* normalBuffer = nullptr;
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("NORMAL")];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    normalBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // --- 提取 UV (TexCoord) ---
                const float* uvBuffer = nullptr;
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    uvBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // 【新增】：--- 提取切线 (Tangent) ---
                const float* tangentBuffer = nullptr;
                if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("TANGENT")];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    tangentBuffer = reinterpret_cast<const float*>(&(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // 装配该 SubMesh 的所有顶点
                for (size_t v = 0; v < vertexCount; ++v) {
                    Renderer::Vertex vertex{};
                    vertex.pos = glm::vec3(positionBuffer[v * 3], positionBuffer[v * 3 + 1], positionBuffer[v * 3 + 2]);
                    
                    // // Sponza 模型非常大，缩小 100 倍以便在我们的摄像机里观看
                    // vertex.pos *= 0.01f; 

                    vertex.normal = normalBuffer ? glm::vec3(normalBuffer[v * 3], normalBuffer[v * 3 + 1], normalBuffer[v * 3 + 2]) : glm::vec3(0.0f, 1.0f, 0.0f);
                    vertex.texCoord = uvBuffer ? glm::vec2(uvBuffer[v * 2], uvBuffer[v * 2 + 1]) : glm::vec2(0.0f);
                    vertex.color = glm::vec3(1.0f);
                    
                    // 【修改】：如果模型自带切线则读取，否则给一个默认值
                    // 注意：glTF 标准中 TANGENT 是 vec4 (xyz 是方向，w 是副切线符号计算参数)，这里我们只取前 3 个 float
                    if (tangentBuffer) {
                        vertex.tangent = glm::vec4(
                            tangentBuffer[v * 4 + 0], 
                            tangentBuffer[v * 4 + 1], 
                            tangentBuffer[v * 4 + 2], 
                            tangentBuffer[v * 4 + 3]  // 致命的 W 分量！
                        );
                    } else {
                        // 如果模型没有切线，给一个默认的 vec4
                        vertex.tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); 
                    }

                    globalVertices.push_back(vertex);
                }

                // --- 提取索引 (Indices) ---
                if (primitive.indices >= 0) {
                    const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.indices];
                    const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                    const void* dataPtr = &(gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);

                    subMesh.indexCount = static_cast<uint32_t>(accessor.count);

                    // glTF 的索引可能是 16 位也可能是 32 位，必须分别处理
                    if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* buf = static_cast<const uint16_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++) {
                            globalIndices.push_back(buf[index] + vertexStart);
                        }
                    } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const uint32_t* buf = static_cast<const uint32_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++) {
                            globalIndices.push_back(buf[index] + vertexStart);
                        }
                    }
                }
                
                m_SubMeshes.push_back(subMesh);
            }
        }

        m_IndexCount = static_cast<uint32_t>(globalIndices.size());

        // ==========================================
        // 3. 上传数据到 Vulkan 显存
        // ==========================================
        // 顶点缓冲 (Vertex Buffer)
        VkDeviceSize vertexBufferSize = sizeof(globalVertices[0]) * globalVertices.size();
        VkBuffer vertexStagingBuffer;
        VkDeviceMemory vertexStagingBufferMemory;
        vkContext.CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);

        void* data;
        vkMapMemory(vkContext.GetDevice(), vertexStagingBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, globalVertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(vkContext.GetDevice(), vertexStagingBufferMemory);

        vkContext.CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);
        vkContext.CopyBuffer(vertexStagingBuffer, m_VertexBuffer, vertexBufferSize);
        vkDestroyBuffer(vkContext.GetDevice(), vertexStagingBuffer, nullptr);
        vkFreeMemory(vkContext.GetDevice(), vertexStagingBufferMemory, nullptr);

        // 索引缓冲 (Index Buffer)
        VkDeviceSize indexBufferSize = sizeof(globalIndices[0]) * globalIndices.size();
        VkBuffer indexStagingBuffer;
        VkDeviceMemory indexStagingBufferMemory;
        vkContext.CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferMemory);

        vkMapMemory(vkContext.GetDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, globalIndices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(vkContext.GetDevice(), indexStagingBufferMemory);

        vkContext.CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);
        vkContext.CopyBuffer(indexStagingBuffer, m_IndexBuffer, indexBufferSize);
        vkDestroyBuffer(vkContext.GetDevice(), indexStagingBuffer, nullptr);
        vkFreeMemory(vkContext.GetDevice(), indexStagingBufferMemory, nullptr);

        HR_LOG_INFO("Model: Sponza loaded! SubMeshes: " + std::to_string(m_SubMeshes.size()) + " | Materials: " + std::to_string(m_Materials.size()));
        return true;
    };



    void Model::Cleanup(VkDevice device) {
        if (m_IndexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_IndexBuffer, nullptr);
            vkFreeMemory(device, m_IndexBufferMemory, nullptr);
        }
        if (m_VertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, m_VertexBuffer, nullptr);
            vkFreeMemory(device, m_VertexBufferMemory, nullptr);
        }
    }
}