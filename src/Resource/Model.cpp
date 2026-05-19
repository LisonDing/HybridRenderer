#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "Model.h"
#include "../Renderer/VulkanContext.h"
#include "../Core/Logger.h"
#include <unordered_map>
#include <cstring>
#include <algorithm>

namespace Resource {

    bool Model::LoadFromFile(const std::string& path, Renderer::VulkanContext& vkContext) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
            HR_LOG_ERROR("Model: Failed to load " + path + " " + warn + err);
            return false;
        }

        std::vector<Renderer::Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Renderer::Vertex, uint32_t> uniqueVertices{};

        glm::vec3 minBounds(FLT_MAX);
        glm::vec3 maxBounds(-FLT_MAX);

        for (const auto& shape : shapes) {
            for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
                auto idx0 = shape.mesh.indices[i + 0];
                auto idx1 = shape.mesh.indices[i + 1];
                auto idx2 = shape.mesh.indices[i + 2];

                glm::vec3 pos0 = {attrib.vertices[3 * idx0.vertex_index + 0], attrib.vertices[3 * idx0.vertex_index + 1], attrib.vertices[3 * idx0.vertex_index + 2]};
                glm::vec3 pos1 = {attrib.vertices[3 * idx1.vertex_index + 0], attrib.vertices[3 * idx1.vertex_index + 1], attrib.vertices[3 * idx1.vertex_index + 2]};
                glm::vec3 pos2 = {attrib.vertices[3 * idx2.vertex_index + 0], attrib.vertices[3 * idx2.vertex_index + 1], attrib.vertices[3 * idx2.vertex_index + 2]};
                
                glm::vec3 edge1 = pos1 - pos0;
                glm::vec3 edge2 = pos2 - pos0;
                glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

                tinyobj::index_t indices_array[3] = {idx0, idx1, idx2};

                for (int j = 0; j < 3; ++j) {
                    auto index = indices_array[j];
                    Renderer::Vertex vertex{};
                    vertex.pos = {attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1], attrib.vertices[3 * index.vertex_index + 2]};
                    
                    minBounds = glm::min(minBounds, vertex.pos);
                    maxBounds = glm::max(maxBounds, vertex.pos);

                    if (index.texcoord_index >= 0) {
                        vertex.texCoord = {attrib.texcoords[2 * index.texcoord_index + 0], 1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
                    } 

                    if (attrib.normals.size() > 0 && index.normal_index >= 0) {
                        vertex.normal = {attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1], attrib.normals[3 * index.normal_index + 2]};
                    } else {
                        vertex.normal = faceNormal;
                    }

                    vertex.color = {1.0f, 1.0f, 1.0f};

                    if (uniqueVertices.count(vertex) == 0) {
                        uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                        vertices.push_back(vertex);
                    }
                    indices.push_back(uniqueVertices[vertex]);
                }
            }
        }

        // 居中与缩放标准化
        glm::vec3 center = (minBounds + maxBounds) / 2.0f;
        glm::vec3 extents = maxBounds - minBounds;
        float maxDim = std::max(extents.x, std::max(extents.y, extents.z));
        for (auto& v : vertices) {
            v.pos = (v.pos - center) / maxDim * 2.0f;
        }

        m_IndexCount = static_cast<uint32_t>(indices.size());

        // 1. 创建 Vertex Buffer (交由底层 RHI 处理显存)
        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        VkBuffer vertexStagingBuffer;
        VkDeviceMemory vertexStagingBufferMemory;
        vkContext.CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexStagingBuffer, vertexStagingBufferMemory);

        void* data;
        vkMapMemory(vkContext.GetDevice(), vertexStagingBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t)vertexBufferSize);
        vkUnmapMemory(vkContext.GetDevice(), vertexStagingBufferMemory);

        vkContext.CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_VertexBuffer, m_VertexBufferMemory);
        vkContext.CopyBuffer(vertexStagingBuffer, m_VertexBuffer, vertexBufferSize);

        vkDestroyBuffer(vkContext.GetDevice(), vertexStagingBuffer, nullptr);
        vkFreeMemory(vkContext.GetDevice(), vertexStagingBufferMemory, nullptr);

        // 2. 创建 Index Buffer
        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        VkBuffer indexStagingBuffer;
        VkDeviceMemory indexStagingBufferMemory;
        vkContext.CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, indexStagingBuffer, indexStagingBufferMemory);

        vkMapMemory(vkContext.GetDevice(), indexStagingBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)indexBufferSize);
        vkUnmapMemory(vkContext.GetDevice(), indexStagingBufferMemory);

        vkContext.CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_IndexBuffer, m_IndexBufferMemory);
        vkContext.CopyBuffer(indexStagingBuffer, m_IndexBuffer, indexBufferSize);

        vkDestroyBuffer(vkContext.GetDevice(), indexStagingBuffer, nullptr);
        vkFreeMemory(vkContext.GetDevice(), indexStagingBufferMemory, nullptr);

        HR_LOG_INFO("Model: Loaded successfully -> " + path + " (Verts: " + std::to_string(vertices.size()) + ")");
        return true;
    }

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