#include "Renderer.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <GLFW/glfw3.h>

#include "imgui_impl_vulkan.h"
#include <imgui.h>

namespace Genesis {

    void Renderer::init(GpuContext& ctx) {
        VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // 1. Allocate sync fences to handle CPU-GPU pacing
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            vkCreateFence(ctx.device, &fenceInfo, nullptr, &_renderFences[i]);
        }

        create_semaphores(ctx);
    }

    void Renderer::create_semaphores(GpuContext& ctx) {
        uint32_t imageCount = static_cast<uint32_t>(ctx.swapchainImages.size());
        uint32_t count = std::max(imageCount, static_cast<uint32_t>(FRAME_OVERLAP));

        ctx.presentSemaphores.resize(count, VK_NULL_HANDLE);
        ctx.renderSemaphores.resize(count, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        // Allocate semaphores across the complete tracking scope to guarantee clean execution slots
        for (uint32_t i = 0; i < count; i++) {
            vkCreateSemaphore(ctx.device, &semInfo, nullptr, &ctx.presentSemaphores[i]);
            vkCreateSemaphore(ctx.device, &semInfo, nullptr, &ctx.renderSemaphores[i]);
        }
    }

    void Renderer::handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu) {
        // Stall execution until existing pipeline queues are fully drained
        vkDeviceWaitIdle(ctx.device);

        // Reconstruct platform swapchain textures matching updated hardware window allocations
        gpu.recreate_swapchain();
        ctx.swapchainExtent = gpu.get_extent();

        // Note: Synchronization primitives are tied to flight thresholds, not window sizing bounds.
        // Reusing them here removes runtime kernel reallocation overhead.
    }

    void Renderer::render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData) {
        if (drawData) {
            ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
        }
    }

    void Renderer::draw_frame(GpuContext& ctx, GpuSystem& gpu, SceneRenderer& scene, Editor& editor, const RenderPacket& packet) {
        if (ctx.presentSemaphores.empty() || ctx.renderSemaphores.empty()) {
            return;
        }

        // 0. Catch external window resize queries before processing synchronization tokens
        if (ctx.framebufferResized) {
            handle_swapchain_resize(ctx, gpu);
            ctx.framebufferResized = false;
        }

        int i = _frameNumber % MAX_FRAMES_IN_FLIGHT;

        // 1. Blocks the CPU thread if the targeted virtual ring slot is still busy on the GPU
        vkWaitForFences(ctx.device, 1, &_renderFences[i], VK_TRUE, UINT64_MAX);

        // 2. Fetch the next available index from the active Vulkan Swapchain
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(ctx.device, ctx.swapchain, 0,
                                               ctx.presentSemaphores[i], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            ctx.framebufferResized = false;
            handle_swapchain_resize(ctx, gpu);
            return;
        }

        // 3. Clear synchronization locks to prepare for the current frame iteration
        vkResetFences(ctx.device, 1, &_renderFences[i]);

        // 4. Open command buffer streams
        VkCommandBuffer cmd = ctx.commandBuffers[i];
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);

        // Enforce safe structural boundaries across internal resolution queries
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

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = { renderW, renderH };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // --- GENESIS DECOUPLING START ---
        // Execute the offscreen 3D scene engine passes using isolated packet constraints
        scene.record_commands(cmd, packet);
        // --- GENESIS DECOUPLING END ---

        // 5. Open Main Swapchain Render Pass for final user interface compositing
        VkRenderPassBeginInfo rpInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        rpInfo.renderPass = ctx.renderPass;
        rpInfo.framebuffer = ctx.framebuffers[imageIndex];
        rpInfo.renderArea.extent = ctx.swapchainExtent;

        VkClearValue clearColor = { {{ 0.1f, 0.1f, 0.1f, 1.0f }} };
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        // 6. Project layout coordinates to match outer hardware dimensions so UI overlay bounds are unclipped
        VkViewport uiViewport{};
        uiViewport.x = 0.0f;
        uiViewport.y = 0.0f;
        uiViewport.width = static_cast<float>(ctx.swapchainExtent.width);
        uiViewport.height = static_cast<float>(ctx.swapchainExtent.height);
        uiViewport.minDepth = 0.0f;
        uiViewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &uiViewport);

        VkRect2D uiScissor{};
        uiScissor.offset = { 0, 0 };
        uiScissor.extent = ctx.swapchainExtent;
        vkCmdSetScissor(cmd, 0, 1, &uiScissor);

        // Splice serialization frame metrics into the explicit presentation target
        editor.render_explicit(cmd, packet.imguiDrawData);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        // 7. Dispatch payload to asynchronous GPU processing pipelines
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

        // 8. Present final calculated frame layouts back to the display engine
        VkPresentInfoKHR present = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &ctx.renderSemaphores[imageIndex];
        present.swapchainCount = 1;
        present.pSwapchains = &ctx.swapchain;
        present.pImageIndices = &imageIndex;

        vkQueuePresentKHR(ctx.graphicsQueue, &present);

        _frameNumber++;
    }

    void Renderer::cleanup(GpuContext& ctx) {
        // Guarantee pipeline loops are totally dark before freeing memory heaps
        vkDeviceWaitIdle(ctx.device);

        // Release synchronization structures safely
        for (int i = 0; i < FRAME_OVERLAP; i++) {
            if (_renderFences[i] != VK_NULL_HANDLE) {
                vkDestroyFence(ctx.device, _renderFences[i], nullptr);
                _renderFences[i] = VK_NULL_HANDLE;
            }
        }

        for (auto semaphore : ctx.presentSemaphores) {
            vkDestroySemaphore(ctx.device, semaphore, nullptr);
        }
        for (auto semaphore : ctx.renderSemaphores) {
            vkDestroySemaphore(ctx.device, semaphore, nullptr);
        }

        ctx.presentSemaphores.clear();
        ctx.renderSemaphores.clear();
    }
}