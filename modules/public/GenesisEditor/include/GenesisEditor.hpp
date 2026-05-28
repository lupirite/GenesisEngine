#pragma once
#include <vulkan/vulkan.h>
#include "GpuSystem.hpp"
#include "imgui.h"

struct ImDrawData;

namespace Genesis {

    class GenesisEditor {
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