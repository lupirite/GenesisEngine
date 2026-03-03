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

        VkSwapchainKHR swapchain;
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        VkFormat swapchainImageFormat;
        VkExtent2D swapchainExtent;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> framebuffers;
    };

    class GpuSystem {
    public:
        void init();
        void cleanup();

        GpuContext& get_context() { return context; }

    private:
        GpuContext context;
        //void create_instance();
        //void setup_device();
        //void create_window();
    };
}