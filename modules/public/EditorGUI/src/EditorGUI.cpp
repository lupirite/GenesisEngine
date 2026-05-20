#include "EditorGUI.hpp"

#include <imgui_impl_vulkan.h>
#include <iostream>

#include "../../GenesisRenderer/include/GenesisRenderer.hpp"

namespace Genesis {
    void EditorGUI::render_ui(SceneRenderer& scene, const GpuContext& ctx, bool isSizeMismatched) {
        // Draw the Stats first
        draw_stats_overlay();
        draw_scene_settings();

        ImVec2 presetPos = ImVec2(400.0f, 150.0f);   // X, Y coordinates from top-left
        ImVec2 presetSize = ImVec2(640.0f, 360.0f); // Default Width, Height

        ImGui::SetNextWindowPos(presetPos, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(presetSize, ImGuiCond_Appearing);

        // Now draw the Viewport
        ImGui::Begin("Scene Viewport");
        _lastViewportSize = ImGui::GetContentRegionAvail();

        ImVec2 viewportSize = _lastViewportSize;

        bool isWindowHovered = ImGui::IsWindowHovered();
        bool isWindowFocused = ImGui::IsWindowFocused();
        bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

        // 2. The explicit condition: The user is clicking/dragging the window frame,
        // OR your custom layout system is waiting to finalize a commit.
        bool isActivelyResizing = (isWindowHovered || isWindowFocused) && isMouseDown;

        if (sceneTextureID == (ImTextureID)0) {
            // Draw a placeholder dark gray window background so the window layout exists
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

            if (viewAspect <= MAX_ASPECT and !isActivelyResizing) {
                // WINDOW IS NARROWER THAN CANVAS: We clip the sides
                // Calculate how much of the width to show (0.5 is the center)


                wasCroppedAspect = true;
                croppedViewportSize = viewportSize;

                // Draw filling the whole viewport height/width
                ImGui::Image(sceneTextureID, viewportSize, uv0, uv1);
            }
            else {
                // WINDOW IS WIDER THAN CANVAS: Letterbox (Black bars on sides)
                if (!isActivelyResizing)
                    wasCroppedAspect = false;

                float displayWidth = viewportSize.y * MAX_ASPECT;

                if (wasCroppedAspect) {
                    displayWidth = viewportSize.y * (croppedViewportSize.x/croppedViewportSize.y);
                }

                float offsetX = (viewportSize.x - displayWidth) * 0.5f;

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);

                uv0 = ImVec2(0.0f, 0.0f);
                ImVec2 uv1 = ImVec2(1.0f, 1.0f);

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

        // 1. Create the descriptor set ONLY ONCE
        if (sceneDescriptorSet == VK_NULL_HANDLE) {
            sceneDescriptorSet = (VkDescriptorSet)ImGui_ImplVulkan_AddTexture(
                newSampler, newView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else {
            // 2. REUSE the existing descriptor set by updating it
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
        // We keep the sceneDescriptorSet handle, just mark the view as dirty
        lastRegisteredView = VK_NULL_HANDLE;
    }

    void EditorGUI::draw_stats_overlay() {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f); // Slightly darker for readability

        if (ImGui::Begin("Stats Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav))
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "GENESIS PROFILER");
            ImGui::Separator();

            // 1. Thread Performance
            ImGui::Text("Main Thread (Logic):  %.3f ms", m_cpuTime);
            ImGui::Text("Render Thread (Sub): %.3f ms", m_renderTime);

            // 2. GPU Estimate (Until we add QueryPools)
            float totalFrame = (m_cpuTime > m_renderTime) ? m_cpuTime : m_renderTime;
            ImGui::Text("Estimated Latency:   %.1f ms", totalFrame);

            ImGui::Separator();

            // 3. FPS Counters
            float mainFps = ImGui::GetIO().Framerate;
            ImGui::Text("UI Update Rate:      %.1f FPS", mainFps);

            // Visual indicator for "The Budget"
            // 60 FPS = 16.6ms budget.
            float budgetProgress = m_renderTime / 16.66f;
            ImVec4 barColor = ImVec4(budgetProgress, 1.0f - budgetProgress, 0.0f, 1.0f);
            ImGui::ProgressBar(budgetProgress, ImVec2(-1.0f, 0.0f), "");
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::Text("GPU Budget");

            ImGui::End();
        }
    }

    void EditorGUI::draw_scene_settings() {
        ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver); // Offset from stats
        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("Scene Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Scene Settings");
            ImGui::Separator();

            // We use the variables stored in our EditorState struct now!
            ImGui::DragFloat("Sphere Radius", &_state.sphereRadius, 0.005f, 0.0f, 1.0f);
            ImGui::ColorEdit3("Sphere Color", _state.sphereColor);
            ImGui::End();
        }
    }

    ResizeRequest EditorGUI::check_resize(SceneRenderer& scene, bool isCommit) {
        ResizeRequest request;
        ImVec2 currentSize = _lastViewportSize;

        // 1. Safety check to ensure we have a valid viewport
        if (currentSize.x > 0.1f && currentSize.y > 0.1f) {

            // 2. Always report the current window size.
            // The renderer needs this every frame to set the VkViewport correctly.
            request.width = (uint32_t)currentSize.x;
            request.height = (uint32_t)currentSize.y;

            // 3. Only trigger the "Heavy" re-allocation if we have a 'Commit'
            // (mouse released) AND the resolution actually changed.
            if (isCommit) {
                if (request.width != scene.get_width() || request.height != scene.get_height()) {
                    request.needed = true;
                }
            }
        }
        return request;
    }
}
