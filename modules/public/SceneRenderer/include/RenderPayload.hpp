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
#include <memory>
#include <cstdint>

struct ImDrawData;

namespace Genesis {

    /// Pure virtual interface for specialized scene data payloads shipped to the renderer.
    struct IRenderPayload {
        virtual ~IRenderPayload() = default;
    };

    /// Thread-safe transaction packet containing front-end states for back-end consumption.
    struct RenderPacket {
        // --- Frame Metrics ---
        float time = 0.0f;
        uint32_t width = 0;
        uint32_t height = 0;

        // --- Allocation & Lifecycles ---
        bool needsResize = false;
        bool isFinal = false;

        // --- Type-Erased Frame Payloads ---
        std::unique_ptr<IRenderPayload> scenePayload;
        ::ImDrawData* imguiDrawData = nullptr;

        // --- Rule of 5 Special Member Functions ---
        RenderPacket() = default;
        ~RenderPacket() = default;

        // Clean move semantics for effortless transfer through queue-pumping segments
        RenderPacket(RenderPacket&&) noexcept = default;
        RenderPacket& operator=(RenderPacket&&) noexcept = default;

        // Strict non-copyable design guarantees single ownership and eliminates double-free risks
        RenderPacket(const RenderPacket&) = delete;
        RenderPacket& operator=(const RenderPacket&) = delete;
    };

} // namespace Genesis