#include "ImGuiLayer.h"
#include "../Core/Logger.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <vector>

namespace UI {

    void ImGuiLayer::Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue, VkRenderPass renderPass) {
        m_Device = device;

        // 1. 创建专属描述符池
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000;
        poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        poolInfo.pPoolSizes = poolSizes;

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_ImGuiDescriptorPool) != VK_SUCCESS) {
            HR_LOG_ERROR("ImGuiLayer: Failed to create descriptor pool!");
            return;
        }

        // 2. 初始化 ImGui 上下文
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        // 高分屏适配
        float xscale, yscale;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        ImGui::GetIO().FontGlobalScale = xscale;
        ImGui::GetStyle().ScaleAllSizes(xscale);

        // 3. 初始化后端
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = physicalDevice;
        init_info.Device = m_Device;
        init_info.QueueFamily = graphicsQueueFamily;
        init_info.Queue = graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_ImGuiDescriptorPool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoMain.RenderPass = renderPass; // 【修正】现在严格绑定到后处理 Pass

        ImGui_ImplVulkan_Init(&init_info);
        HR_LOG_INFO("ImGuiLayer: Initialized successfully.");
    }

    void ImGuiLayer::NewFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::RenderControlPanel(glm::vec3& lightDir, float& ambient, float& specular, float& exposure, float& vignette) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Once);

        ImGui::Begin("Control Panel");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Separator();
        
        ImGui::Text("Lighting Settings");
        ImGui::SliderFloat3("Light Dir", &lightDir.x, -10.0f, 10.0f);
        ImGui::SliderFloat("Ambient Strength", &ambient, 0.0f, 1.0f);
        ImGui::SliderFloat("Specular Strength", &specular, 0.0f, 2.0f);
        ImGui::Separator();
        
        ImGui::Text("Post-Processing");
        ImGui::SliderFloat("Exposure", &exposure, 0.1f, 5.0f);
        ImGui::SliderFloat("Vignette", &vignette, 0.0f, 1.0f);

        ImGui::End();
        ImGui::Render();
    }

    void ImGuiLayer::RenderDrawData(VkCommandBuffer commandBuffer) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }

    void ImGuiLayer::Cleanup() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        if (m_ImGuiDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_Device, m_ImGuiDescriptorPool, nullptr);
        }
        HR_LOG_INFO("ImGuiLayer: Cleaned up.");
    }

    bool ImGuiLayer::WantCaptureMouse() {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiLayer::WantCaptureKeyboard() {
        return ImGui::GetIO().WantCaptureKeyboard;
    }
}