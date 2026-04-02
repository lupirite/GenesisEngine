#include "EditorGUI.hpp"

namespace Genesis {
    void EditorGUI::render_ui(SceneRenderer& scene, GpuContext& ctx) {
        draw_stats_overlay();
        draw_scene_settings();
        draw_viewport(scene, ctx);
    }

    void EditorGUI::draw_viewport(SceneRenderer& scene, GpuContext& ctx) {
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(400, 10), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            ImVec2 currentSize = ImGui::GetContentRegionAvail();

            // Handle Resize
            if (currentSize.x > 0.1f && currentSize.y > 0.1f) {
                if ((uint32_t)currentSize.x != scene.get_width() || (uint32_t)currentSize.y != scene.get_height()) {
                    vkDeviceWaitIdle(ctx.device);
                    scene.cleanup(ctx.device);
                    scene.init(ctx, (uint32_t)currentSize.x, (uint32_t)currentSize.y);
                }
            }
            ImGui::Image((ImTextureID)scene.get_descriptor_set(), currentSize);

            _lastViewportSize = currentSize;
        }
        ImGui::End();
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