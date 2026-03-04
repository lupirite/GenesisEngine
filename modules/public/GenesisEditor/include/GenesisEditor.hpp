#pragma once
#include <vulkan/vulkan.h>
#include "imgui.h"

namespace Genesis {

    class GenesisEditor {
    public:
        // This is your "Init Function"
        void init(GLFWwindow* window,
                  VkInstance instance,
                  VkPhysicalDevice physDevice,
                  VkDevice device,
                  uint32_t queueFamily,
                  VkQueue graphicsQueue,
                  VkRenderPass renderPass);

        void shutdown(VkDevice device);

        // Call these every frame in your main loop
        void new_frame();
        void render(VkCommandBuffer commandBuffer);

    private:
        VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    };

}