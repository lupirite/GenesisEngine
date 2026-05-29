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
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace Genesis {

    /// Central hardware data pod containing runtime graphics states and context resources.
    struct GpuContext {
        // --- Core Platform Framework ---
        GLFWwindow* window = nullptr;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties gpuProperties{};

        // --- Hardware Processing Queues ---
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamily = 0;

        // --- Asynchronous Execution Rings ---
        static constexpr int FRAME_OVERLAP = 2;
        VkCommandPool commandPools[FRAME_OVERLAP] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
        VkCommandBuffer commandBuffers[FRAME_OVERLAP] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        // --- Window Presentation Swapchain ---
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent{ 0, 0 };
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;

        // --- Structural Layouts & Framing ---
        VkRenderPass renderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> framebuffers;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        // --- Pipeline Synchronization Primitives ---
        std::vector<VkSemaphore> presentSemaphores;
        std::vector<VkSemaphore> renderSemaphores;
        bool framebufferResized = false;
    };

    class GpuSystem {
    public:
        // --- Core Lifecycle Systems ---
        void init();
        void recreate_swapchain();
        void cleanup();

        // --- State Accessors ---
        GpuContext& get_context() { return context; }
        VkExtent2D get_extent() const { return m_swapchainExtent; }

    private:
        VkExtent2D m_swapchainExtent{ 0, 0 };
        GpuContext context;
    };

} // namespace Genesis