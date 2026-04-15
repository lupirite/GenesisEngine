#pragma once
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"
#include <imgui.h>

namespace Genesis {
    // This struct is your "RenderPacket" bridge
    struct EditorState {
        float sphereRadius = 0.5f;
        float sphereColor[3] = { 0.25f, 0.1f, 0.9f };
    };

    struct ResizeRequest {
        bool needed = false;
        uint32_t width;
        uint32_t height;
    };

    class EditorGUI {
    public:
        ResizeRequest check_resize(SceneRenderer& scene); // New function
        void render_ui(SceneRenderer& scene, const GpuContext& ctx); // Keep this for drawing

        const EditorState& get_state() const { return _state; }

        void update_texture_descriptor(SceneRenderer& scene, const GpuContext& ctx);

    private:
        ImTextureID sceneTextureID = (ImTextureID)0;
        EditorState _state;
        ImVec2 _lastViewportSize = ImVec2(0, 0);
        VkImageView lastRegisteredView = VK_NULL_HANDLE;

        void draw_stats_overlay();
        void draw_scene_settings();
        void draw_viewport(SceneRenderer& scene, GpuContext& ctx);

        float viewportWidth = 1280.0f;
        float viewportHeight = 720.0f;
    };
}