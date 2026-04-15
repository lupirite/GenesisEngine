#include <memory>

struct ImDrawData;

namespace Genesis {
    // The generic base class
    struct IRenderPayload {
        virtual ~IRenderPayload() = default;
    };

    // The high-level packet the Renderer expects
    struct RenderPacket {
        float time;
        bool needsResize = false;
        // The specific scene data (erased as the interface)
        std::unique_ptr<IRenderPayload> scenePayload;
        ::ImDrawData* imguiDrawData = nullptr;
    };
}
