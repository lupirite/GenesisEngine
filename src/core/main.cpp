#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

int main() {
    // 1. Setup GPU
    Genesis::GpuSystem gpu;
    gpu.init();
    auto& ctx = gpu.get_context();

    // 2. Setup Editor
    Genesis::GenesisEditor editor;
    editor.init(ctx);

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // --- NEW: START EDITOR FRAME ---
        editor.new_frame();

        // Create a simple test window
        ImGui::Begin("Genesis Engine Control");
        ImGui::Text("Hello, RTX 2060!");
        if (ImGui::Button("Power Down")) {
            glfwSetWindowShouldClose(ctx.window, true);
        }
        ImGui::End();

        // 3. Vulkan Rendering (The "Heartbeat")
        uint32_t imageIndex;
        vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);

        // Record Commands
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo);

        VkRenderPassBeginInfo rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;

        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(ctx.commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // --- NEW: DRAW THE EDITOR ---
        editor.render(ctx.commandBuffer);

        vkCmdEndRenderPass(ctx.commandBuffer);
        vkEndCommandBuffer(ctx.commandBuffer);

        // Submit to GPU
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx.commandBuffer;
        vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &ctx.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo);

        vkDeviceWaitIdle(ctx.device);
    }

    // 4. Cleanup
    editor.shutdown(ctx.device);
    gpu.cleanup();

    return 0;
}