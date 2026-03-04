#pragma once
#include <vulkan/vulkan.h>

#include "GpuSystem.hpp"
#include "imgui.h"

namespace Genesis {

    class GenesisEditor {
    public:
        // This is your "Init Function"
        void init(const GpuContext& ctx);

        void shutdown(VkDevice device);

        // Call these every frame in your main loop
        void new_frame();
        void render(VkCommandBuffer commandBuffer);

    private:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    };

}