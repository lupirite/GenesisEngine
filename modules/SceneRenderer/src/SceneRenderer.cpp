#include "SceneRenderer.hpp"
#include "../third_party/imgui/backends/imgui_impl_vulkan.h"
#include <stdexcept>
#include <fstream>
#include <vector>

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("Failed to open shader: " + filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

namespace Genesis {

    uint32_t SceneRenderer::find_memory_type(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("SceneRenderer: Failed to find suitable memory type!");
    }

    void SceneRenderer::create_image_resources(GpuContext& ctx, uint32_t width, uint32_t height) {
        // 1. Create the Image (The raw data container)
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Standard color format
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(ctx.device, &imageInfo, nullptr, &_image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create scene image!");
        }

        // 2. Allocate Memory for the Image
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(ctx.device, _image, &memReqs);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = find_memory_type(ctx.physDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate image memory!");
        }

        vkBindImageMemory(ctx.device, _image, _imageMemory, 0);
    }

    void SceneRenderer::init(GpuContext& ctx, uint32_t width, uint32_t height) {
        _width = width;
        _height = height;

        // 1. Create the Image and Memory (Using the helpers we wrote)
        create_image_resources(ctx, width, height);

        // 2. Create Image View
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Match the image format
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(ctx.device, &viewInfo, nullptr, &_imageView) != VK_SUCCESS) {
            throw std::runtime_error("SceneRenderer: Failed to create image view!");
        }

        // 3. Create Sampler (Tells the GPU how to stretch/shrink the texture)
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(ctx.device, &samplerInfo, nullptr, &_sampler) != VK_SUCCESS) {
            throw std::runtime_error("SceneRenderer: Failed to create sampler!");
        }

        // 4. Register with ImGui (This is the "Magic" part)
        // We use the helper function from the ImGui Vulkan backend
        _descriptorSet = ImGui_ImplVulkan_AddTexture(_sampler, _imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // 1. Define the Attachment (Our private texture)
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Crucial for ImGui!

        VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        // 2. Define the Subpass
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        // 3. Create RenderPass
        VkRenderPassCreateInfo rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments = &colorAttachment;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;

        vkCreateRenderPass(ctx.device, &rpInfo, nullptr, &_renderPass);

        // 4. Create Framebuffer (Linking the RenderPass to our Image View)
        VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbInfo.renderPass = _renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &_imageView;
        fbInfo.width = _width;
        fbInfo.height = _height;
        fbInfo.layers = 1;

        vkCreateFramebuffer(ctx.device, &fbInfo, nullptr, &_framebuffer);
        create_pipeline(ctx.device);
    }

    void Genesis::SceneRenderer::create_pipeline(VkDevice device) {
        auto vertCode = readFile("modules/SceneRenderer/shaders/gradient.vert.spv");
        auto fragCode = readFile("modules/SceneRenderer/shaders/gradient.frag.spv");

        VkShaderModule vertModule = createShaderModule(device, vertCode);
        VkShaderModule fragModule = createShaderModule(device, fragCode);

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertModule;
        stages[0].pName = "main";

        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragModule;
        stages[1].pName = "main";

        // Pipeline Layout (Where your future Uniforms/Push Constants will live)
        VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &_pipelineLayout);

        // Minimal Pipeline State
        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;

        // Fixed functions: No vertex input (we generate in shader), Triangle list
        VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        pipelineInfo.pVertexInputState = &vertexInput;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pipelineInfo.pInputAssemblyState = &inputAssembly;

        // Viewport & Scissor (Match your texture size)
        VkViewport viewport{ 0.0f, 0.0f, (float)_width, (float)_height, 0.0f, 1.0f };
        VkRect2D scissor{ {0, 0}, {_width, _height} };
        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        pipelineInfo.pViewportState = &viewportState;

        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.lineWidth = 1.0f;
        pipelineInfo.pRasterizationState = &rasterizer;

        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        pipelineInfo.pMultisampleState = &multisampling;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // Ensure this line uses the '&' to pass the address
        pipelineInfo.pColorBlendState = &colorBlending;

        pipelineInfo.layout = _pipelineLayout;
        pipelineInfo.renderPass = _renderPass;

        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline);

        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
    }

    void SceneRenderer::record_commands(VkCommandBuffer cmd) {
        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = _renderPass;
        rpInfo.framebuffer = _framebuffer;
        rpInfo.renderArea.offset = { 0, 0 };
        rpInfo.renderArea.extent = { _width, _height };

        // Let's go with a nice Genesis Purple/Blue for testing
        VkClearValue clearColor = {{{ 0.1f, 0.05f, 0.5f, 1.0f }}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        // The "Inline" contents mean we are providing commands directly here
        // rather than using secondary command buffers.
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind our shader pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

        // Draw 3 vertices (our giant triangle)
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
    }

    void SceneRenderer::cleanup(VkDevice device) {
        vkDestroyPipeline(device, _graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, _pipelineLayout, nullptr);
        vkDestroyFramebuffer(device, _framebuffer, nullptr);
        vkDestroyRenderPass(device, _renderPass, nullptr);
        vkDestroySampler(device, _sampler, nullptr);
        vkDestroyImageView(device, _imageView, nullptr);
        vkDestroyImage(device, _image, nullptr);
        vkFreeMemory(device, _imageMemory, nullptr);

        // Note: ImGui handles the cleanup of the DescriptorSet
        // when the Global Descriptor Pool is destroyed.
    }
}