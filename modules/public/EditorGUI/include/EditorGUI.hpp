//     Genesis Engine - A custom hardware-accelerated software engine.
//     Copyright (C) 2026 Lupirite (contact me at lupirite@gmail.com)
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include "Editor.hpp"
#include "SceneRenderer.hpp"
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Genesis {

    /// Holds the serialized snapshot data dispatched to the render worker thread.
    struct EditorState {
        float sphereRadius = 0.5f;
        float sphereColor[3] = { 0.25f, 0.1f, 0.9f };
        float camPos[3] = { 0, 0, 0};
        glm::quat camRot;
    };

    /// Encapsulates sizing transactions issued by the ImGui docking layout viewport.
    struct ResizeRequest {
        bool needed = false;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    class EditorGUI {
    public:
        void set_camera_z(float z) { _state.camPos[2] = z; }
        void set_camera_y(float y) { _state.camPos[1] = y; }
        void set_camera_x(float x) { _state.camPos[0] = x; }

        void set_camera_rot(glm::quat rot) { _state.camRot = rot; }

        // Enforces a hard physical constraint against runaway aspect ratios during window stretching
        static constexpr float MAX_ASPECT = 3.0f;

        // --- Performance & Metric Trackers ---
        void set_cpu_time(float ms) { m_cpuTime = ms; }
        void set_render_time(float ms) { m_renderTime = ms; }

        // --- Resource Accessors ---
        ImTextureID get_scene_texture_id() const { return sceneTextureID; }
        const EditorState& get_state() const { return _state; }

        ImVec2 get_viewport_dimensions() {
            // Guard against uninitialized or minimized layout states prior to first frame calculations
            if (_lastViewportSize.x <= 0.1f || _lastViewportSize.y <= 0.1f) {
                return ImVec2(0.0f, 0.0f);
            }
            return _lastViewportSize;
        }

        // --- Layout & Resize Operations ---
        ResizeRequest check_resize(SceneRenderer& scene, bool isCommit);
        void render_ui(SceneRenderer& scene, const GpuContext& ctx, bool isSizeMismatched);

        // --- Vulkan Backend Binding Hooks ---
        void update_texture_descriptor(SceneRenderer& scene, const GpuContext& ctx);
        void invalidate_texture();

    private:
        // Engine Performance States
        float m_cpuTime = 0.0f;
        float m_renderTime = 0.0f;

        // Layout Parameters
        float viewportWidth = 1280.0f;
        float viewportHeight = 720.0f;
        bool wasCroppedAspect = false;
        ImVec2 croppedViewportSize = ImVec2(1, 1);
        ImVec2 _lastViewportSize = ImVec2(0, 0);

        // State Tracking Objects
        EditorState _state;

        // Backend Vulkan Synchronization Handles
        VkDescriptorSet sceneDescriptorSet = VK_NULL_HANDLE;
        ImTextureID sceneTextureID = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(0));
        VkImageView lastRegisteredView = VK_NULL_HANDLE;

        // --- Private Panel Composition Pipeline ---
        void draw_stats_overlay();
        void draw_scene_settings();
        void draw_viewport(SceneRenderer& scene, GpuContext& ctx);
    };

} // namespace Genesis