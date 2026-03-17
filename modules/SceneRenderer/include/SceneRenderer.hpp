#pragma once
#include "GpuSystem.hpp"
#include <vulkan/vulkan.h>

namespace Genesis {
    class SceneRenderer {
    public:
        void init(GpuContext& ctx, uint32_t width, uint32_t height);
        void cleanup(VkDevice device);

        // This is what the Editor calls to show this scene in a window
        VkDescriptorSet get_descriptor_set() { return _descriptorSet; }

        void record_commands(VkCommandBuffer cmd);

    private:
        uint32_t find_memory_type(VkPhysicalDevice physDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void create_image_resources(GpuContext& ctx, uint32_t width, uint32_t height);

        VkImage _image;
        VkDeviceMemory _imageMemory;
        VkImageView _imageView;
        VkSampler _sampler;
        VkDescriptorSet _descriptorSet;

        VkRenderPass _renderPass;
        VkFramebuffer _framebuffer;

        uint32_t _width, _height;

        VkPipelineLayout _pipelineLayout;
        VkPipeline _graphicsPipeline;

        void create_pipeline(VkDevice device);
    };
}