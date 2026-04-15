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

        if (sceneTextureID) {
            // This is the ONLY place we should draw the image
            ImGui::Image(sceneTextureID, ImVec2(viewportWidth, viewportHeight));
        } else {
            ImGui::Text("No Scene Texture Linked");
            ImGui::Text("Check if update_texture_descriptor was called.");
        }

        ImGui::End();
    }

    void EditorGUI::update_texture_descriptor(SceneRenderer& scene, const GpuContext& ctx) {
        VkImageView newView = scene.get_output_view();
        VkSampler newSampler = scene.get_sampler();

        if (newView == VK_NULL_HANDLE || newSampler == VK_NULL_HANDLE) {
            return;
        }

        if (newView == lastRegisteredView) return;

        // Assign the new ID. This should only happen when the view actually changes!
        sceneTextureID = (ImTextureID)(intptr_t)ImGui_ImplVulkan_AddTexture(
            newSampler,
            newView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        lastRegisteredView = newView;
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