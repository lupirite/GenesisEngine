#pragma once
#include <vulkan/vulkan.h>

#include "GpuSystem.hpp"
#include "imgui.h"

struct ImDrawData;

namespace Genesis {

    class GenesisEditor {
    public:
        // This is your "Init Function"
        void init(const GpuContext& ctx);

        void shutdown(VkDevice device);

        // Call these every frame in your main loop
        void new_frame();

        void render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData);

    private:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    };

}