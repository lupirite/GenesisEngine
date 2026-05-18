#include "GenesisRenderer.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <GLFW/glfw3.h> // For glfwGetTime

#include "imgui_impl_vulkan.h"

#include <imgui.h>

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
        // 1. Wait for the GPU to finish all pending work before we tear down the output buffers
        vkDeviceWaitIdle(ctx.device);

        // 2. Rebuild the Swapchain
        // This calls the low-level Vulkan functions to destroy the old VkSwapchainKHR,
        // query the new window dimensions, and create the new VkImages.
        gpu.recreate_swapchain();

        // 3. Update the render extent
        // Ensure our renderer's internal understanding of the screen matches the new swapchain
        ctx.swapchainExtent = gpu.get_extent();

        // 4. Verification
        // We do NOT recreate semaphores or fences here. They are linked to MAX_FRAMES_IN_FLIGHT,
        // not the window size. Reusing them saves significant CPU overhead during the resize loop.
    }

    void GenesisRenderer::render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData) {
        if (drawData) {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }
    }

    void GenesisRenderer::draw_frame(GpuContext& ctx, GpuSystem& gpu, SceneRenderer& scene, GenesisEditor& editor, const RenderPacket& packet) {

        if (ctx.presentSemaphores.empty() || ctx.renderSemaphores.empty()) {
            return;
        }

        // 0. Handle WINDOW (Swapchain) Resizes
        // This should be triggered by a separate flag or by checking the GpuContext directly
        if (ctx.framebufferResized) {
            handle_swapchain_resize(ctx, gpu);
            ctx.framebufferResized = false;
        }

        int i = _frameNumber % MAX_FRAMES_IN_FLIGHT;

        // 1. Wait for this specific frame's slot to be ready
        vkWaitForFences(ctx.device, 1, &_renderFences[i], VK_TRUE, UINT64_MAX);

        // 2. Acquire Image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, 0,
                                       ctx.presentSemaphores[i], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Note: ImGui::EndFrame is now handled by the packet producer
            ctx.framebufferResized = false;
            handle_swapchain_resize(ctx, gpu);
            return;
        }

        // 3. Reset Fence
        vkResetFences(ctx.device, 1, &_renderFences[i]);

        // 4. Recording
        VkCommandBuffer cmd = ctx.commandBuffers[i];
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);

        uint32_t renderW = std::clamp(packet.width, 1u, 16384u);
        uint32_t renderH = std::clamp(packet.height, 1u, 16384u);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(renderW);
        viewport.height = static_cast<float>(renderH);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        // 3. Configure Scissor to match
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = { renderW, renderH };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // --- GENESIS DECOUPLING START ---
        // Record your 3D graphics scene to your offscreen image attachment
        scene.record_commands(cmd, packet);
        // --- GENESIS DECOUPLING END ---

        // 4. Begin Main Swapchain Render Pass (Where ImGui draws its final composition over the window)
        VkRenderPassBeginInfo rpInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent; // This tracks the real application window bounds
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 5. Override Viewport/Scissor to match the Swapchain size so ImGui doesn't clip
        VkViewport uiViewport{};
        uiViewport.x = 0.0f;
        uiViewport.y = 0.0f;
        uiViewport.width = static_cast<float>(ctx.swapchainExtent.width);
        uiViewport.height = static_cast<float>(ctx.swapchainExtent.height);
        uiViewport.minDepth = 0.0f;
        uiViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &uiViewport);

        VkRect2D uiScissor{};
        uiScissor.offset = {0, 0};
        uiScissor.extent = ctx.swapchainExtent;
        vkCmdSetScissor(cmd, 0, 1, &uiScissor);

        // Pass the snapshot of the UI draw data to the editor composition pass
        editor.render_explicit(cmd, packet.imguiDrawData);

        vkCmdEndRenderPass(cmd);

        vkEndCommandBuffer(cmd);

        // 5. Submit
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

        // 6. Present
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
