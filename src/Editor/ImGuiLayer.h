#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace UI {
    class ImGuiLayer {
    public:
        // 接收底层的 Vulkan 句柄，初始化 UI
        void Init(GLFWwindow* window, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t graphicsQueueFamily, VkQueue graphicsQueue, VkRenderPass renderPass);
        
        void NewFrame();
        void RenderControlPanel(glm::vec3& lightDir, float& ambient, float& specular, float& exposure, float& vignette);
        
        // 将 UI 录制进主 Command Buffer
        void RenderDrawData(VkCommandBuffer commandBuffer);
        void Cleanup();

        static bool WantCaptureMouse();
        static bool WantCaptureKeyboard();

    private:
        VkDevice m_Device = VK_NULL_HANDLE;
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
    };
}