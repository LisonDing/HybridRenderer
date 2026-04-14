
#include "VulkanContext.h"
#include "../Core/Logger.h"
#include <set>
#include <string>

namespace Renderer {

std::vector<char> VulkanContext::ReadFile(const std::string& filename) {
    // 工业规范：ate (从文件末尾开始读取，方便直接获取文件大小)，binary (以二进制模式读取)
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        HR_LOG_ERROR("Failed to open file: " + filename);
        // 在实际开发中，这里可以抛出异常或导致断言失败
        return {};
    }

    // 预分配对应的内存空间
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    // 回到文件头部，一口气读入所有数据
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule VulkanContext::CreateShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    
    // SPIR-V 字节码要求以 uint32_t (4字节) 对齐读取。
    // vector<char> 的底层数据分配默认满足对齐要求，使用 reinterpret_cast 强转是安全的。
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        HR_LOG_ERROR("Failed to create shader module!");
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

bool VulkanContext::Init(const std::vector<const char*>& windowExtensions) {
    // 1. 准备扩展列表 (融合窗口扩展 + 苹果专属扩展)
    std::vector<const char*> extensions = windowExtensions;

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    HR_LOG_INFO("VulkanContext: Injected Apple Portability Extension.");
#endif

    // 【Vulkan 教学】：显式 API (Explicit API) 的哲学
    // Vulkan 不做任何假设。你需要用结构体 (Struct) 把所有细节填好递交给它。
    // 所有 Vulkan 结构体都必须设置 sType (Structure Type)，这是为了驱动在解析内存时不会读错。
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hybrid Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3; 

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifdef __APPLE__
    // 告诉驱动：我们要包含那些非原生（转译）的 Vulkan 设备（即 MoltenVK）
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create Vulkan Instance!");
        return false;
    }
    
    HR_LOG_INFO("VulkanContext: Vulkan Instance created successfully.");
    return true;
}

void VulkanContext::PickPhysicalDevice() {
    // 【Vulkan 教学】：获取列表的“两步走”模式
    // 在 Vulkan 中获取数组数据，标准做法总是分两步：
    // 第一步：传入 nullptr，让 Vulkan 告诉你数组有多大（数量）。
    // 第二步：分配好内存后，再调一次函数，真正把数据填进来。
    
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        HR_LOG_ERROR("VulkanContext: Failed to find GPUs with Vulkan support!");
        return;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    // 简单起见，我们直接挑选第一个设备 (对于 Mac 通常就是 M 系列芯片)
    // 在后续的工业级开发中，我们会给显卡打分（比如独立显卡加分，支持几何着色器加分）来挑选。
    m_PhysicalDevice = devices[0];

    // 查询并打印显卡属性
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
    
    HR_LOG_INFO(std::string("VulkanContext: Picked Physical Device: ") + deviceProperties.deviceName);
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    // 获取当前显卡支持的所有队列族（车间）
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // 遍历这些车间，寻找我们需要的能力
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        // 寻找一个支持图形绘制 (Graphics) 的车间
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // 查询该车间是否支持向我们提供的 Surface 输出画面
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.IsComplete()) break;
        i++;
    }

    return indices;
}

void VulkanContext::CreateLogicalDevice() {
    // 1. 查找我们需要使用的队列族
    QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
    // if (!indices.IsComplete()) {
    //     HR_LOG_ERROR("VulkanContext: Failed to find suitable queue families!");
    //     return;
    // }

    // 【架构升级】：使用 std::set 自动去重。
    // 如果画图和显示是同一个车间，set 里就只有 1 个元素；如果是不同的车间，就有 2 个元素。
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // 2. 填写队列创建信息
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
#ifdef __APPLE__
    // Mac 的 MoltenVK 通常需要开启子集兼容扩展
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    
    // 启用扩展！
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create logical device!");
        return;
    }
    HR_LOG_INFO("VulkanContext: Logical Device created successfully.");

    vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
    HR_LOG_INFO("VulkanContext: Graphics Queue retrieved.");
    
    vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
    HR_LOG_INFO("VulkanContext: Present Queue retrieved.");
}


void VulkanContext::CreateSwapchain(uint32_t width, uint32_t height) {
    // 1. 设置画面的基本属性：大小、颜色格式（我们使用标准的 8位 SRGB 颜色空间）
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_Surface;

    // 为了避免过于复杂的底层查询，我们针对 Mac / 通用 PC 写死几个最常用的标准值
    createInfo.minImageCount = 3; // 开启三缓冲 (Triple Buffering)，大幅提升帧率稳定性
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB; 
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = { width, height };
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 我们要把它当作颜色渲染目标

    QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    // 如果画图和显示不是同一个车间，我们需要让图片在车间之间共享
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // 不做任何画面翻转（如手机屏幕旋转）
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;   // 忽略 Alpha 通道，不与操作系统窗口混合
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;               // FIFO = 开启垂直同步(V-Sync)，Mac 下兼容性最好
    createInfo.clipped = VK_TRUE; // 被其他窗口挡住的像素不渲染

    if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create Swapchain!");
        return;
    }
    HR_LOG_INFO("VulkanContext: Swapchain (Triple Buffering) created successfully.");

    // 【新增】保存格式和分辨率，后续非常重要
    m_SwapchainImageFormat = VK_FORMAT_B8G8R8A8_SRGB;
    m_SwapchainExtent = { width, height };

    // 【新增】把显卡在 Swapchain 里自动帮我们建好的图片 (VkImage) 拿出来
    uint32_t imageCount;
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_SwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());
}

void VulkanContext::CreateImageViews() {
    m_SwapchainImageViews.resize(m_SwapchainImages.size());

    for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_SwapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;     // 明确告诉它是 2D 图像
        createInfo.format = m_SwapchainImageFormat;      // 保持和 Swapchain 相同的颜色格式

        // 颜色通道映射（通常保持默认的 1:1 映射即可）
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 描述图像的用途：它是颜色图，没有 Mipmap，只有 1 层
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS) {
            HR_LOG_ERROR("VulkanContext: Failed to create Image Views!");
            return;
        }
    }
    HR_LOG_INFO("VulkanContext: Swapchain Image Views created.");
}

void VulkanContext::CreateRenderPass() {
    // 1. 颜色附件 (Attachment) 描述：我们要画的这块画布有什么特性？
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_SwapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // 无多重采样抗锯齿 (MSAA)
    
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // 关键：渲染前，清空这块内存的内容
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 关键：渲染后，保留内容以便显示到屏幕上
    
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 我们目前不用模板测试
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      // 渲染前，我们不在乎图像原本是什么排布
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 渲染后，必须转换成能被屏幕显示的排布格式

    // 2. 颜色附件引用：给具体的 Subpass 用的
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // 对应上面 colorAttachment 的索引
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 渲染时，让显卡将其优化为颜色写入模式

    // 3. 子通道 (Subpass) 描述
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // 这是一条图形管线，不是 Compute 管线
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // 4. 正式创建 Render Pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create Render Pass!");
        return;
    }
    HR_LOG_INFO("VulkanContext: Render Pass created.");
}

void VulkanContext::CreateGraphicsPipeline() {
    // 1. 读取我们离线编译好的 Shader 二进制代码
    // 注意路径：因为我们在 build 目录下运行 ./bin/HybridRenderer，所以相对路径包含 bin/
    auto vertShaderCode = ReadFile("bin/shaders/shader.vert.spv");
    auto fragShaderCode = ReadFile("bin/shaders/shader.frag.spv");

    if (vertShaderCode.empty() || fragShaderCode.empty()) {
        HR_LOG_ERROR("VulkanContext: Failed to load shader files! Check your paths.");
        return;
    }

    // 2. 将二进制代码封装成 Vulkan 模块
    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    // 3. 配置顶点着色器阶段
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main"; // 入口函数名

    // 4. 配置片段着色器阶段
    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // 5. 顶点输入状态 (Vertex Input)
    // 目前我们把顶点数据硬编码在 Shader 里了，所以这里暂时为空
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // 6. 拓扑图元装配 (Input Assembly)
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 每三个点画一个三角形
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 7. 动态状态 (Dynamic State)
    // 允许我们在录制绘制命令时动态修改视口和裁剪矩形，而不用重新创建整个管线
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 视口与裁剪矩形状态配置
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; // 虽然是动态的，但仍需告诉 Vulkan 我们用几个视口
    viewportState.scissorCount = 1;

    // 8. 光栅化器 (Rasterizer) - 图形学核心
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // 填充多边形（也可设为 LINE 用于线框模式）
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;   // 背面剔除
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // 顺时针为正面
    rasterizer.depthBiasEnable = VK_FALSE;

    // 9. 多重采样 (Multisampling) - 暂不开启抗锯齿
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 10. 颜色混合 (Color Blending) - 决定新像素如何覆盖画板上的老像素
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // 暂时关闭混合 (Alpha Blend)

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 11. 管线布局 (Pipeline Layout) - 用于后续传递 Uniform Buffer (比如 MVP 矩阵)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create pipeline layout!");
        return;
    }

    // ==========================================
    // 最终组装装配线 (Create Graphics Pipeline)
    // ==========================================
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = m_RenderPass; // 必须绑定对应的 Render Pass
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS) {
        HR_LOG_ERROR("VulkanContext: Failed to create graphics pipeline!");
        return;
    }
    HR_LOG_INFO("VulkanContext: Graphics Pipeline created successfully.");

    // Shader Module 只是在创建管线时需要，烤死进管线后就可以立刻销毁了
    vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
}

void VulkanContext::Cleanup() {
    // 【C++ 架构规范】：安全的资源销毁
    // 销毁顺序必须与创建顺序严格相反。
    // 注意：VkPhysicalDevice 是物理客观存在的显卡，不需要我们去 Destroy，
    // 我们只需要销毁我们在内存中创建的逻辑对象 (m_Instance)。

    if (m_GraphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
        HR_LOG_INFO("VulkanContext: Graphics Pipeline destroyed.");
    }
    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        HR_LOG_INFO("VulkanContext: Render Pass destroyed.");
    }
    for (auto imageView : m_SwapchainImageViews) {
        vkDestroyImageView(m_Device, imageView, nullptr);
    }
        HR_LOG_INFO("VulkanContext: Swapchain Image Views destroyed.");

    if (m_Swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        HR_LOG_INFO("VulkanContext: Swapchain destroyed.");
    }

    if (m_Device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
        HR_LOG_INFO("VulkanContext: Logical Device destroyed.");
    }

    if (m_Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr); // Surface 由 Instance 销毁
        HR_LOG_INFO("VulkanContext: Surface destroyed.");
    }

    if (m_Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE; // 销毁后置空，防止野指针悬挂
        HR_LOG_INFO("VulkanContext: Vulkan Instance destroyed.");
    }
}

} // namespace Renderer
