#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

namespace Genesis {

    struct GpuContext {
        GLFWwindow* window;
        VkInstance instance;
        VkSurfaceKHR surface;
        VkDevice device;
        VkPhysicalDevice physDevice;
        VkQueue graphicsQueue;
        uint32_t graphicsQueueFamily;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;

        VkSwapchainKHR swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkFormat swapchainImageFormat;
        VkExtent2D swapchainExtent;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> framebuffers;

        VkDescriptorPool descriptorPool; // For binding textures to ImGui/Shaders
        VkPhysicalDeviceProperties gpuProperties; // To check limits (max texture size, etc.)

        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    };

    class GpuSystem {
    public:
        void init();

        void recreate_swapchain();

        void cleanup();

        GpuContext& get_context() { return context; }

    private:
        GpuContext context;
        //void create_instance();
        //void setup_device();
        //void create_window();
    };
}