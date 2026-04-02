#include <stdexcept>

#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"
#include "EditorGUI.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

int main() {
    Genesis::GpuSystem gpu;
    gpu.init();
    auto& ctx = gpu.get_context();

    Genesis::GenesisEditor editor;
    editor.init(ctx);

    Genesis::EditorGUI gui; // Our new organized module
    Genesis::SceneRenderer myScene;
    myScene.init(ctx, 1280, 720);

    VkFence renderFence;
    VkFenceCreateInfo fenceInfo = {}; // Zero-initialize the struct
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    // THE FIX: It must be VK_FENCE_CREATE_SIGNALED_BIT
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(ctx.device, &fenceInfo, nullptr, &renderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render fence!");
    }

    VkSemaphore imageAvailableSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    if (vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphore!");
    }

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        vkWaitForFences(ctx.device, 1, &renderFence, VK_TRUE, UINT64_MAX);

        // --- THE NEW RESIZE GUARD ---
        auto resize = gui.check_resize(myScene);
        if (resize.needed) {
            vkDeviceWaitIdle(ctx.device); // Stop everything!
            myScene.cleanup(ctx.device);
            myScene.init(ctx, resize.width, resize.height);
        }

        editor.new_frame();
        gui.render_ui(myScene, ctx); // Now this only DRAWS, it doesn't delete things.

        // ... rest of your Acquire/Record/Submit logic ...

        // 3. ACQUIRE
        uint32_t imageIndex;
        // Wait for the PREVIOUS frame to actually finish before we reset the fence

        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
                                               imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ImGui::EndFrame();
            vkDeviceWaitIdle(ctx.device);
            gpu.recreate_swapchain();
            continue;
        }


        vkResetFences(ctx.device, 1, &renderFence);

        // 4. RECORDING
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo);

        auto& state = gui.get_state();
        if (myScene.get_width() > 1) {
            myScene.record_commands(ctx.commandBuffer, (float)glfwGetTime(),
                                    state.sphereRadius, (float*)state.sphereColor);
        }

        // 5. RENDER PASS & UI DRAW
        VkRenderPassBeginInfo rpInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;

        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(ctx.commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        editor.render(ctx.commandBuffer);
        vkCmdEndRenderPass(ctx.commandBuffer);

        vkEndCommandBuffer(ctx.commandBuffer);

        // 6. SUBMIT & PRESENT
        VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore; // Wait for Acquire to finish
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx.commandBuffer;

        vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, renderFence);

        VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &ctx.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo);

        // --- THE EMERGENCY BRAKE ---
        // Keeps the CPU from running ahead of the GPU.
        // Essential for single-threaded stability!
        vkDeviceWaitIdle(ctx.device);
    }

    vkDeviceWaitIdle(ctx.device); // One final wait to be safe

    vkDestroySemaphore(ctx.device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(ctx.device, renderFence, nullptr); // Destroy fence ONCE
    myScene.cleanup(ctx.device);                      // Cleanup scene ONCE
    editor.shutdown(ctx.device);
    gpu.cleanup();

    return 0;
}