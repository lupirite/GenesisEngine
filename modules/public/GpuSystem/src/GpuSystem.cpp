#include "GpuSystem.hpp"
#include "VkBootstrap.h" // The magic helper
#include <iostream>

namespace Genesis {

    void GpuSystem::init() {
        // 1. Create the Window
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        context.window = glfwCreateWindow(1280, 720, "Genesis Engine", nullptr, nullptr);

        // 2. Initialize Vulkan Instance
        vkb::InstanceBuilder builder;
        auto inst_ret = builder.set_app_name("Genesis Engine")
            .request_validation_layers() // Essential for debugging!
            .use_default_debug_messenger()
            .build();

        vkb::Instance vkb_inst = inst_ret.value();
        context.instance = vkb_inst.instance;

        // 3. Select Physical Device & Logical Device
        // We need a surface to tell Vulkan which GPU can talk to our window
        glfwCreateWindowSurface(context.instance, context.window, nullptr, &context.surface);

        vkb::PhysicalDeviceSelector selector{ vkb_inst };
        auto phys_ret = selector.set_surface(context.surface)
            .set_minimum_version(1, 2) // C++20 likes Vulkan 1.2+
            .select();

        vkb::PhysicalDevice vkb_phys = phys_ret.value();
        context.physDevice = vkb_phys.physical_device;

        vkb::DeviceBuilder device_builder{ vkb_phys };
        auto dev_ret = device_builder.build();

        vkb::Device vkb_device = dev_ret.value();
        context.device = vkb_device.device;

        // 4. Get the Graphics Queue
        context.graphicsQueue = vkb_device.get_queue(vkb::QueueType::graphics).value();
        context.graphicsQueueFamily = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

        std::cout << "GPU Initialized: " << vkb_phys.properties.deviceName << std::endl;

        // 5. Create the Swapchain
        vkb::SwapchainBuilder swapchain_builder{ vkb_phys, vkb_device, context.surface };
        auto swap_ret = swapchain_builder
            .set_desired_extent(1280, 720)
            .set_desired_format({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .build();

        vkb::Swapchain vkb_swapchain = swap_ret.value();
        context.swapchain = vkb_swapchain.swapchain;
        context.swapchainImages = vkb_swapchain.get_images().value();
        context.swapchainImageViews = vkb_swapchain.get_image_views().value();
        context.swapchainImageFormat = vkb_swapchain.image_format;
        context.swapchainExtent = vkb_swapchain.extent;

        // 6. Create a simple RenderPass (The "Instructions" for the GPU)
        VkAttachmentDescription color_attachment = {};
        color_attachment.format = context.swapchainImageFormat;
        color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear screen to color
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Ready for monitor

        VkAttachmentReference color_attachment_ref = {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;

        VkRenderPassCreateInfo render_pass_info = {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &color_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        vkCreateRenderPass(context.device, &render_pass_info, nullptr, &context.renderPass);

        context.framebuffers.resize(context.swapchainImageViews.size());

        for (size_t i = 0; i < context.swapchainImageViews.size(); i++) {
            VkImageView attachments[] = { context.swapchainImageViews[i] };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = context.renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = context.swapchainExtent.width;
            framebufferInfo.height = context.swapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(context.device, &framebufferInfo, nullptr, &context.framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer");
            }
        }

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = context.graphicsQueueFamily; // Make sure this is in your struct!
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &context.commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = context.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(context.device, &allocInfo, &context.commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        // Create a Global Descriptor Pool for the whole engine
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        if (vkCreateDescriptorPool(context.device, &pool_info, nullptr, &context.descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create global descriptor pool");
        }

        // Also grab the properties for later optimization
        vkGetPhysicalDeviceProperties(context.physDevice, &context.gpuProperties);
    }

    void GpuSystem::cleanup() {
        vkDeviceWaitIdle(context.device);

        vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr);

        vkDestroyCommandPool(context.device, context.commandPool, nullptr);

        for (auto framebuffer : context.framebuffers) {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
        }

        // 2. Destroy Image Views
        for (auto imageView : context.swapchainImageViews) {
            vkDestroyImageView(context.device, imageView, nullptr);
        }

        // 3. The rest in reverse order of creation
        vkDestroyRenderPass(context.device, context.renderPass, nullptr);
        vkDestroySwapchainKHR(context.device, context.swapchain, nullptr);

        vkDestroyDevice(context.device, nullptr);
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
        vkDestroyInstance(context.instance, nullptr);

        glfwDestroyWindow(context.window);
        glfwTerminate();
    }

} // namespace Genesis