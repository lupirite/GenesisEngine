#include "GenesisRenderer.hpp"
#include <stdexcept>
#include <string>
#include <GLFW/glfw3.h> // For glfwGetTime

namespace Genesis {

    void GenesisRenderer::init(GpuContext& ctx) {
        VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // 1. Create the Fences (Class members _renderFences)
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkCreateFence(ctx.device, &fenceInfo, nullptr, &_renderFences[i]);
        }

        create_semaphores(ctx);
    }

    void GenesisRenderer::create_semaphores(GpuContext& ctx) {
        uint32_t imageCount = (uint32_t)ctx.swapchainImages.size();
        uint32_t count = std::max(imageCount, (uint32_t)FRAME_OVERLAP);

        ctx.presentSemaphores.resize(count, VK_NULL_HANDLE);
        ctx.renderSemaphores.resize(count, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        // Use 'count' instead of 'imageCount' to ensure no NULL handles are left in the active range
        for (uint32_t i = 0; i < count; i++) {
            vkCreateSemaphore(ctx.device, &semInfo, nullptr, &ctx.presentSemaphores[i]);
            vkCreateSemaphore(ctx.device, &semInfo, nullptr, &ctx.renderSemaphores[i]);
        }
    }

    void GenesisRenderer::handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu) {
        vkDeviceWaitIdle(ctx.device);

        // 1. Safe Cleanup of old semaphores
        for (auto& s : ctx.presentSemaphores) {
            if (s != VK_NULL_HANDLE) {
                vkDestroySemaphore(ctx.device, s, nullptr);
                s = VK_NULL_HANDLE;
            }
        }
        for (auto& s : ctx.renderSemaphores) {
            if (s != VK_NULL_HANDLE) {
                vkDestroySemaphore(ctx.device, s, nullptr);
                s = VK_NULL_HANDLE;
            }
        }

        // Clear the vectors so create_semaphores starts fresh
        ctx.presentSemaphores.clear();
        ctx.renderSemaphores.clear();

        // 2. Rebuild hardware
        gpu.recreate_swapchain();

        // 3. Re-create for the new image count
        create_semaphores(ctx);
    }

    void GenesisRenderer::draw_frame(GpuContext& ctx, SceneRenderer& scene, GenesisEditor& editor, EditorGUI& gui, GpuSystem& gpu) {

        if (ctx.presentSemaphores.empty() || ctx.renderSemaphores.empty()) {
            return;
        }

        int i = _frameNumber % FRAME_OVERLAP;

        // 1. Wait for this specific frame's slot to be ready
        vkWaitForFences(ctx.device, 1, &_renderFences[i], VK_TRUE, UINT64_MAX);

        // 2. Acquire Image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, UINT64_MAX,
                                       ctx.presentSemaphores[i], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ImGui::EndFrame();
            handle_swapchain_resize(ctx, gpu);
            // REMOVED: vkQueueSubmit line. It was causing the fence signal error.
            return; // Just exit. The next frame loop will call vkWaitForFences on a fresh state.
        }

        // 3. Viewport Resize (Internal Scene)
        auto resize = gui.check_resize(scene);
        if (resize.needed) {
            vkDeviceWaitIdle(ctx.device);
            scene.cleanup(ctx.device);
            scene.init(ctx, resize.width, resize.height);
        }

        // 4. Reset Fence - We are officially starting GPU work
        vkResetFences(ctx.device, 1, &_renderFences[i]);

        // 5. Recording
        VkCommandBuffer cmd = ctx.commandBuffers[i];
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);

        auto& state = gui.get_state();
        // IMPORTANT: Use the command buffer 'cmd' for this frame, NOT ctx.commandBuffer
        scene.record_commands(cmd, (float)glfwGetTime(), state.sphereRadius, (float*)state.sphereColor);

        VkRenderPassBeginInfo rpInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        editor.render(cmd);
        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        // 6. Submit
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &ctx.presentSemaphores[i];
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &ctx.renderSemaphores[imageIndex];

        if (vkQueueSubmit(ctx.graphicsQueue, 1, &submit, _renderFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        // 7. Present
        VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &ctx.renderSemaphores[imageIndex];
        present.swapchainCount = 1;
        present.pSwapchains = &ctx.swapchain;
        present.pImageIndices = &imageIndex;

        vkQueuePresentKHR(ctx.graphicsQueue, &present);

        _frameNumber++;
    }

    void GenesisRenderer::cleanup(GpuContext& ctx) {
        // 1. Wait for GPU to be completely finished with all frames
        vkDeviceWaitIdle(ctx.device);

        // 2. Destroy the Fences (The CPU throttles)
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            if (_renderFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(ctx.device, _renderFences[i], nullptr);
            }
        }

        // 3. Destroy the Semaphores (The GPU/Swapchain throttles)
        // We loop through the vectors we stored in the context
        for (auto semaphore : ctx.presentSemaphores) {
            vkDestroySemaphore(ctx.device, semaphore, nullptr);
        }
        for (auto semaphore : ctx.renderSemaphores) {
            vkDestroySemaphore(ctx.device, semaphore, nullptr);
        }

        // 4. Clear the vectors (Good practice to avoid dangling handles)
        ctx.presentSemaphores.clear();
        ctx.renderSemaphores.clear();
    }
}