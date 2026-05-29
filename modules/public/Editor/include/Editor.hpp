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
#include <vulkan/vulkan.h>
#include "GpuSystem.hpp"
#include "imgui.h"

struct ImDrawData;

namespace Genesis {

    class Editor {
    public:
        // --- Core Lifecycle Systems ---
        void init(const GpuContext& ctx);
        void shutdown(VkDevice device);

        // --- Frame Step Operations ---
        // Prepares backend input buffers and creates a blank state layout
        void new_frame();

        // Records UI geometry draw calls directly into the active engine command stream
        void render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData);

    private:
        // Isolated descriptor allocator block assigned exclusively to Dear ImGui's font and viewport states
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    };

} // namespace Genesis