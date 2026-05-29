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
#include "GpuSystem.hpp"
#include "SceneRenderer.hpp"
#include "Editor.hpp"
#include "EditorGUI.hpp"

namespace Genesis {

    class Renderer {
    public:
        // --- Core Lifecycle Systems ---
        void init(GpuContext& ctx);
        void cleanup(GpuContext& ctx);

        // --- Frame Rendering Pipeline ---
        void draw_frame(GpuContext& ctx, GpuSystem& gpu, SceneRenderer& scene, Editor& editor, const RenderPacket& packet);
        void render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData);

        // --- Hardware Throttling Queries ---
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        VkFence& get_fence(int index) { return _renderFences[index]; }

    private:
        static constexpr int FRAME_OVERLAP = 2;

        // Tracking state that increments monotonically each frame loop
        int _frameNumber = 0;

        // CPU-GPU Synchronization Throttles
        VkFence _renderFences[FRAME_OVERLAP] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        // --- Private Internal Architecture ---
        // Returns the current active virtual execution ring slot (0 or 1)
        int current_frame() const { return _frameNumber % FRAME_OVERLAP; }

        void create_semaphores(GpuContext& ctx);
        void handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu);
    };

} // namespace Genesis