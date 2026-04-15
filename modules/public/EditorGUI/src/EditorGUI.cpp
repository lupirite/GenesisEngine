#include "EditorGUI.hpp"

#include <imgui_impl_vulkan.h>

namespace Genesis {
    void EditorGUI::render_ui(SceneRenderer& scene, const GpuContext& ctx) {
        // Draw the Stats first
        draw_stats_overlay();
        draw_scene_settings();

        // Now draw the Viewport
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Viewport");

        // Capture size for the next frame's check_resize
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        _lastViewportSize = viewportPanelSize;

        viewportWidth = viewportPanelSize.x;
        viewportHeight = viewportPanelSize.y;

        if (sceneTextureID != (ImTextureID)0) {
            // We have a valid descriptor set, safe to draw
            ImGui::Image(sceneTextureID, ImVec2(viewportWidth, viewportHeight));
        } else {
            // The Render Thread has invalidated the texture for a resize
            ImGui::Text("Viewport Busy...");
            ImGui::Text("(Rebuilding Vulkan Resources)");
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
        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("Stats Overlay", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav))
        {
            ImGui::Text("Genesis");
            ImGui::Separator();
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
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

    ResizeRequest EditorGUI::check_resize(SceneRenderer& scene) {
        ResizeRequest request;

        // We need to 'peek' at the window size before drawing it
        // Or we can use the size from the PREVIOUS frame
        ImVec2 currentSize = _lastViewportSize; // Store this during render_ui

        if (currentSize.x > 0.1f && currentSize.y > 0.1f) {
            if ((uint32_t)currentSize.x != scene.get_width() ||
                (uint32_t)currentSize.y != scene.get_height()) {
                request.needed = true;
                request.width = (uint32_t)currentSize.x;
                request.height = (uint32_t)currentSize.y;
                }
        }
        return request;
    }
}