#include "GenesisEditor.hpp"
#include "../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../third_party/imgui/backends/imgui_impl_vulkan.h"
#include <stdexcept>

#include "GpuSystem.hpp"

namespace Genesis {

    void GenesisEditor::init(const GpuContext& ctx) {
        // 1. Descriptor Pool
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Highly recommended
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;

        if (vkCreateDescriptorPool(ctx.device, &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("Genesis Editor: Failed to create descriptor pool");
        }

        // 2. Setup Context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark(); // Makes it look like a pro engine

        // 3. Backends (Using the ctx struct)
        ImGui_ImplGlfw_InitForVulkan(ctx.window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = ctx.instance;
        init_info.PhysicalDevice = ctx.physDevice;
        init_info.Device = ctx.device;
        init_info.QueueFamily = ctx.graphicsQueueFamily;
        init_info.Queue = ctx.graphicsQueue;
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;

        //init_info.RenderPass = ctx.renderPass;

        //ImGui_ImplVulkan_Init(&init_info, ctx.renderPass);

        ImGui_ImplVulkan_Init(&init_info);

        // 4. URGENT: Upload Fonts
        // Older ImGui needed CreateFontsTexture(), newer ones handle it via NewFrame,
        // but a manual upload ensures the font shows up on frame 1.
    }

void GenesisEditor::new_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void GenesisEditor::render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void GenesisEditor::shutdown(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (imguiPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
    }
}

} // namespace Genesis