#pragma once
#include "GpuSystem.hpp"
#include "SceneRenderer.hpp"
#include "GenesisEditor.hpp"
#include "EditorGUI.hpp"

namespace Genesis {
    class GenesisRenderer {
    public:
        void init(GpuContext& ctx);
        void cleanup(GpuContext& ctx);

        // This replaces the messy main loop logic
        void draw_frame(GpuContext& ctx, SceneRenderer& scene, GenesisEditor& editor, EditorGUI& gui, GpuSystem& gpu);

    private:
        VkFence _renderFence;
        VkSemaphore _imageAvailableSemaphore;

        // Helper for the resize logic
        void handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu);
    };
}