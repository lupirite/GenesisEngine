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