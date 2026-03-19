#include <stdexcept>

#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"

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

    Genesis::SceneRenderer myScene;
    try {
        myScene.init(ctx, 1280, 720);
    } catch (const std::exception& e) {
        printf("CRITICAL ERROR: %s\n", e.what());
        return -1;
    }

    VkFence renderFence;
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    vkCreateFence(ctx.device, &fenceInfo, nullptr, &renderFence);

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // 1. Start the Editor Frame (Calculates UI logic, doesn't draw yet)
        editor.new_frame();

        // Create a transparent overlay in the top-left corner
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f); // Translucent
        if (ImGui::Begin("Stats Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("Genesis");//" | %s", ctx.gpuName.c_str());
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(400, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f); // Translucent
        if (ImGui::Begin("Test Window", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("Test");//" | %s", ctx.gpuName.c_str());
            ImGui::Separator();
            ImGui::Text("TESTING");

            ImGui::Image((ImTextureID)myScene.get_descriptor_set(), ImVec2(1280, 720));

            if (ImGui::CollapsingHeader("Help")) {
                ImGui::Text("Hello");
            }
            ImGui::End();
        }

        ImGui::ShowDemoWindow();

        // 2. Acquire Image from Swapchain
        uint32_t imageIndex;

        vkResetFences(ctx.device, 1, &renderFence);

        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX, VK_NULL_HANDLE, renderFence, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ImGui::EndFrame();
            gpu.recreate_swapchain();
            continue;
        }

        vkWaitForFences(ctx.device, 1, &renderFence, VK_TRUE, UINT64_MAX);


        // 3. START RECORDING (This fixes the "Recording State" error)
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo);

        float timeValue = (float)glfwGetTime();

        myScene.record_commands(ctx.commandBuffer, timeValue);

        // 4. Start Render Pass
        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;

        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(ctx.commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 5. DRAW THE EDITOR (Must be inside the Begin/End block)
        editor.render(ctx.commandBuffer);

        vkCmdEndRenderPass(ctx.commandBuffer);

        // 6. FINISH RECORDING
        vkEndCommandBuffer(ctx.commandBuffer);

        // 7. Submit to GPU
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx.commandBuffer;

        if (vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        // --- THIS IS THE PRESENT INFO AREA ---
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &ctx.swapchain;
        presentInfo.pImageIndices = &imageIndex; // The index we got from vkAcquireNextImageKHR
        presentInfo.pResults = nullptr; // Optional

        // Flip the "Page" to the monitor
        vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo);

        // 8. Wait (Keep this for now so the CPU doesn't outrun the GPU)
        vkDeviceWaitIdle(ctx.device);
    }

    // 4. Cleanup
    myScene.cleanup(ctx.device);
    editor.shutdown(ctx.device);

    vkDestroyFence(ctx.device, renderFence, nullptr);
    gpu.cleanup();

    return 0;
}