#pragma once
#include "GpuSystem.hpp"
#include <vulkan/vulkan.h>

#include "RenderPayload.hpp"

namespace Genesis {

    /// Thread-safe payload snapshot carrying runtime data across frame queues.
    struct SceneSnapshot : public IRenderPayload {
        float sphereRadius;
        float sphereColor[3];
        // Future: std::vector<ObjectData> objects;
    };

    class SceneRenderer {
    public:
        // --- Core Lifecycle ---
        void init(GpuContext& ctx, uint32_t width, uint32_t height);
        void cleanup(VkDevice device);

        // --- Command Execution ---
        void record_commands(VkCommandBuffer cmd, const RenderPacket& packet);

        // --- Hardware Resource Accessors ---
        VkImageView get_output_view() const { return _imageView; }
        VkSampler get_sampler() const { return _sampler; }
        VkDescriptorSet get_descriptor_set() { return _descriptorSet; }

        // --- Frame Geometry Accessors ---
        uint32_t get_width() const { return _width; }
        uint32_t get_height() const { return _height; }

    private:
        // --- Internal Pipeline Allocations ---
        uint32_t find_memory_type(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void create_image_resources(GpuContext& ctx, uint32_t width, uint32_t height);
        void create_pipeline(VkDevice device);

        // Backing Texture VRAM Targets
        VkImage _image = VK_NULL_HANDLE;
        VkDeviceMemory _imageMemory = VK_NULL_HANDLE;
        VkImageView _imageView = VK_NULL_HANDLE;
        VkSampler _sampler = VK_NULL_HANDLE;
        VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;

        // Render Targets & Frame Geometry
        VkRenderPass _renderPass = VK_NULL_HANDLE;
        VkFramebuffer _framebuffer = VK_NULL_HANDLE;
        uint32_t _width = 0;
        uint32_t _height = 0;

        // Graphics Pipeline States
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
        VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
    };

} // namespace Genesis