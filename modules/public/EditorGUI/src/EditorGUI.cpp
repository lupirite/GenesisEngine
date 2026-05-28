#include "EditorGUI.hpp"

#include <imgui_impl_vulkan.h>
#include <iostream>

#include "../../GenesisRenderer/include/GenesisRenderer.hpp"

namespace Genesis {

    void EditorGUI::render_ui(SceneRenderer& scene, const GpuContext& ctx, bool isSizeMismatched) {
        // Draw the diagnostic and adjustment overlays first
        draw_stats_overlay();
        draw_scene_settings();

        // Configure default viewport bounds on application boot
        ImVec2 presetPos = ImVec2(400.0f, 150.0f);
        ImVec2 presetSize = ImVec2(640.0f, 360.0f);

        ImGui::SetNextWindowPos(presetPos, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(presetSize, ImGuiCond_Appearing);

        // Mount the primary Scene Viewport panel
        ImGui::Begin("Scene Viewport");

        _lastViewportSize = ImGui::GetContentRegionAvail();
        ImVec2 viewportSize = _lastViewportSize;

        // Track user interaction with the window frame to identify manual resizes
        bool isWindowHovered = ImGui::IsWindowHovered();
        bool isWindowFocused = ImGui::IsWindowFocused();
        bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool isActivelyResizing = (isWindowHovered || isWindowFocused) && isMouseDown;

        if (sceneTextureID == (ImTextureID)0) {
            // Render a dark fallback canvas if the GPU frame targets are not yet initialized
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImVec2(ImGui::GetCursorScreenPos().x + viewportSize.x, ImGui::GetCursorScreenPos().y + viewportSize.y),
                IM_COL32(30, 30, 30, 255)
            );
        }
        else {
            float viewAspect = viewportSize.x / viewportSize.y;

            ImVec2 uv0 = ImVec2(0.0f, 0.0f);
            ImVec2 uv1 = ImVec2(1.0f, 1.0f);

            if (viewAspect <= MAX_ASPECT && !isActivelyResizing) {
                // WINDOW IS NARROWER THAN CANVAS: Cache sizing properties and clip edges
                wasCroppedAspect = true;
                croppedViewportSize = viewportSize;

                ImGui::Image(sceneTextureID, viewportSize, uv0, uv1);
            }
            else {
                // WINDOW IS WIDER THAN CANVAS: Clear clipping flags and apply letterboxing
                if (!isActivelyResizing) {
                    wasCroppedAspect = false;
                }

                float displayWidth = viewportSize.y * MAX_ASPECT;

                // Adjust the draw canvas bounds if fallback crop metrics are active
                if (wasCroppedAspect) {
                    displayWidth = viewportSize.y * (croppedViewportSize.x / croppedViewportSize.y);
                }

                // Center the bounding region inside the viewport window horizontally
                float offsetX = (viewportSize.x - displayWidth) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

                uv0 = ImVec2(0.0f, 0.0f);
                uv1 = ImVec2(1.0f, 1.0f);

                ImGui::Image(sceneTextureID, ImVec2(displayWidth, viewportSize.y), uv0, uv1);
            }
        }

        ImGui::End();
    }

    void EditorGUI::update_texture_descriptor(SceneRenderer& scene, const GpuContext& ctx) {
        VkImageView newView = scene.get_output_view();
        VkSampler newSampler = scene.get_sampler();

        if (newView == VK_NULL_HANDLE || newSampler == VK_NULL_HANDLE) return;
        if (newView == lastRegisteredView) return;

        // 1. Allocate descriptor set on initial boot frame
        if (sceneDescriptorSet == VK_NULL_HANDLE) {
            sceneDescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
                newSampler, newView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else {
            // 2. Mutate the existing descriptor layout to prevent VRAM allocations on resize
            VkDescriptorImageInfo desc_image[1] = {};
            desc_image[0].sampler = newSampler;
            desc_image[0].imageView = newView;
            desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write_desc[1] = {};
            write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[0].dstSet = sceneDescriptorSet;
            write_desc[0].descriptorCount = 1;
            write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[0].pImageInfo = desc_image;

            vkUpdateDescriptorSets(ctx.device, 1, write_desc, 0, nullptr);
        }

        sceneTextureID = (ImTextureID)sceneDescriptorSet;
        lastRegisteredView = newView;
    }

    void EditorGUI::invalidate_texture() {
        // Flag the active view target dirty to trigger a descriptor write update on the next frame
        lastRegisteredView = VK_NULL_HANDLE;
    }

    void EditorGUI::draw_stats_overlay() {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f);

        if (ImGui::Begin("Stats Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav))
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "GENESIS PROFILER");
            ImGui::Separator();

            // Thread Execution Profiles
            ImGui::Text("Main Thread (Logic):  %.3f ms", m_cpuTime);
            ImGui::Text("Render Thread (Sub): %.3f ms", m_renderTime);

            // Frame Latency Approximations
            float totalFrame = (m_cpuTime > m_renderTime) ? m_cpuTime : m_renderTime;
            ImGui::Text("Estimated Latency:   %.1f ms", totalFrame);

            ImGui::Separator();

            // Refresh Rates
            float mainFps = ImGui::GetIO().Framerate;
            ImGui::Text("UI Update Rate:      %.1f FPS", mainFps);

            // Compute the performance delta relative to a standard 60 FPS (16.66ms) target loop
            float budgetProgress = m_renderTime / 16.66f;
            ImVec4 barColor = ImVec4(budgetProgress, 1.0f - budgetProgress, 0.0f, 1.0f);
            ImGui::ProgressBar(budgetProgress, ImVec2(-1.0f, 0.0f), "");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::Text("GPU Budget");

            ImGui::End();
        }
    }

    void EditorGUI::draw_scene_settings() {
        ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (ImGui::Begin("Scene Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Scene Settings");
            ImGui::Separator();

            // Expose scene configurations mapped to the engine context state
            ImGui::DragFloat("Sphere Radius", &_state.sphereRadius, 0.005f, 0.0f, 1.0f);
            ImGui::ColorEdit3("Sphere Color", _state.sphereColor);
            ImGui::End();
        }
    }

    ResizeRequest EditorGUI::check_resize(SceneRenderer& scene, bool isCommit) {
        ResizeRequest request;
        ImVec2 currentSize = _lastViewportSize;

        // Protect allocations against fully collapsed/minimized panel dimensions
        if (currentSize.x > 0.1f && currentSize.y > 0.1f) {

            // Always dispatch current viewport space metrics to maintain accurate Vulkan viewports
            request.width = (uint32_t)currentSize.x;
            request.height = (uint32_t)currentSize.y;

            // Only issue a hard swapchain/render target recreation if the resize interaction has concluded
            if (isCommit) {
                if (request.width != scene.get_width() || request.height != scene.get_height()) {
                    request.needed = true;
                }
            }
        }
        return request;
    }
}