#include "GenesisEditor.hpp"
#include "../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../third_party/imgui/backends/imgui_impl_vulkan.h"
#include <stdexcept>

#include "GpuSystem.hpp"

namespace Genesis {

    void GenesisEditor::init(const GpuContext& ctx) {
        // 1. Configure the Dedicated UI Descriptor Pool
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        // Allows ImGui to freely allocate and free texture IDs dynamically as tabs open/close
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;

        if (vkCreateDescriptorPool(ctx.device, &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("Genesis Editor: Failed to create UI descriptor pool.");
        }

        // 2. Setup Primary Dear ImGui Context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        // 3. Initialize OS Window and Graphics API Platform Backends
        ImGui_ImplGlfw_InitForVulkan(ctx.window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = ctx.instance;
        init_info.PhysicalDevice = ctx.physDevice;
        init_info.Device = ctx.device;
        init_info.QueueFamily = ctx.graphicsQueueFamily;
        init_info.Queue = ctx.graphicsQueue;
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = 3; // Triple buffering optimization matching presentation structures
        init_info.ImageCount = 3;
        init_info.PipelineInfoMain.RenderPass = ctx.renderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.UseDynamicRendering = false; // Legacy explicit render pass fallback pattern

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            throw std::runtime_error("Genesis Editor: Failed to initialize Vulkan implementation backend.");
        }
    }

    void GenesisEditor::new_frame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void GenesisEditor::render_explicit(VkCommandBuffer commandBuffer, ImDrawData* drawData) {
        // Only submit draw lists into the command buffer if valid geometry exists
        if (drawData) {
            ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
        }
    }

    void GenesisEditor::shutdown(VkDevice device) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        if (imguiPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, imguiPool, nullptr);
            imguiPool = VK_NULL_HANDLE;
        }
    }

} // namespace Genesis