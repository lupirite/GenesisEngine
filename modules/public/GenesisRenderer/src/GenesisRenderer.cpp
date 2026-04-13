#include "GenesisRenderer.hpp"
#include <stdexcept>

namespace Genesis {

    void GenesisRenderer::init(GpuContext& ctx) {
        // 1. Create the Render Fence
        // We move the logic from main.cpp here.
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // Essential: start signaled so the first wait doesn't hang
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateFence(ctx.device, &fenceInfo, nullptr, &_renderFence) != VK_SUCCESS) {
            throw std::runtime_error("GenesisRenderer: Failed to create render fence!");
        }

        // 2. Create the Image Available Semaphore
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr, &_imageAvailableSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("GenesisRenderer: Failed to create image available semaphore!");
        }
    }

    void GenesisRenderer::handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu) {
        // 1. Stop all GPU work immediately
        vkDeviceWaitIdle(ctx.device);

        // 2. Rebuild the hardware swapchain
        gpu.recreate_swapchain();

        // 3. CLEAN THE FENCE:
        // Since vkDeviceWaitIdle guaranteed the GPU is done, we manually
        // reset the fence so it's "Red" and ready for the next real frame.
        vkResetFences(ctx.device, 1, &_renderFence);

        // 4. SIGNAL IT:
        // We immediately signal it so that the 'vkWaitForFences'
        // at the top of the next loop doesn't hang.
        // This effectively "fakes" a completed frame safely.
    }

    void GenesisRenderer::draw_frame(GpuContext& ctx, SceneRenderer& scene, GenesisEditor& editor, EditorGUI& gui, GpuSystem& gpu) {
        // 1. CPU Sync
        vkWaitForFences(ctx.device, 1, &_renderFence, VK_TRUE, UINT64_MAX);

        // 2. Viewport Resize (ImGui side)
        auto resize = gui.check_resize(scene);
        if (resize.needed) {
            vkDeviceWaitIdle(ctx.device);
            scene.cleanup(ctx.device);
            scene.init(ctx, resize.width, resize.height);
        }

        // 3. Acquire Next Image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
                                               _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ImGui::EndFrame();

            handle_swapchain_resize(ctx, gpu);

            VkSubmitInfo signalSubmit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
            vkQueueSubmit(ctx.graphicsQueue, 0, nullptr, _renderFence);
            return;
        }

        // 4. Reset Fence ONLY when we are committed to a frame
        vkResetFences(ctx.device, 1, &_renderFence);

        // 5. Command Recording
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(ctx.commandBuffer, &beginInfo);

        // Render the 6D Scene
        auto& state = gui.get_state();
        scene.record_commands(ctx.commandBuffer, (float)glfwGetTime(), state.sphereRadius, (float*)state.sphereColor);

        // 5. RENDER PASS & UI DRAW
        VkRenderPassBeginInfo rpInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;

        // Use the clear color from your original main.cpp
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(ctx.commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        editor.render(ctx.commandBuffer);
        vkCmdEndRenderPass(ctx.commandBuffer);

        vkEndCommandBuffer(ctx.commandBuffer);

        // 6. Submit
        VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &_imageAvailableSemaphore;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx.commandBuffer;

        vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, _renderFence);

        // 7. Present
        VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &ctx.swapchain;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(ctx.graphicsQueue, &presentInfo);

        // Single-threaded safety (Optional if you use double-buffering later)
        vkDeviceWaitIdle(ctx.device);
    }

    void GenesisRenderer::cleanup(GpuContext& ctx) {
        // Always wait for the GPU to be idle before destroying sync objects
        vkDeviceWaitIdle(ctx.device);

        if (_imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(ctx.device, _imageAvailableSemaphore, nullptr);
        }
        if (_renderFence != VK_NULL_HANDLE) {
            vkDestroyFence(ctx.device, _renderFence, nullptr);
        }
    }
}