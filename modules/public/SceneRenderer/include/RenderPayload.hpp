#include <memory>

struct ImDrawData;

namespace Genesis { // this is JUST for the viewport, not info about the main window
    // The generic base class
    struct IRenderPayload {
        virtual ~IRenderPayload() = default;
    };

    // The high-level packet the Renderer expects
    struct RenderPacket {
        float time = 0.0f;
        uint32_t width = 0;
        uint32_t height = 0;
        bool needsResize = false;

        RenderPacket() = default;
        // Ensure move constructor is clean
        RenderPacket(RenderPacket&&) noexcept = default;
        RenderPacket& operator=(RenderPacket&&) noexcept = default;

        // Delete copy to prevent the "double-free" of the unique_ptr
        RenderPacket(const RenderPacket&) = delete;

        bool isFinal = false;
        // The specific scene data (erased as the interface)
        std::unique_ptr<IRenderPayload> scenePayload;
        ::ImDrawData* imguiDrawData = nullptr;
    };
}
