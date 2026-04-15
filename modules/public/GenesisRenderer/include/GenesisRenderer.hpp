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
        void render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData);

        // This replaces the messy main loop logic
        void draw_frame(GpuContext& ctx, GpuSystem& gpu, SceneRenderer& scene, GenesisEditor& editor, const RenderPacket& packet);

        static constexpr int MAX_FRAMES_IN_FLIGHT = 2; // Match your FRAME_OVERLAP value
        VkFence& get_fence(int index) { return _renderFences[index]; }

    private:
        static constexpr int FRAME_OVERLAP = 2;
        int _frameNumber = 0; // Increments every frame

        // Two of everything
        VkFence _renderFences[FRAME_OVERLAP];
        VkSemaphore _presentSemaphores[FRAME_OVERLAP];
        VkSemaphore _renderSemaphores[FRAME_OVERLAP];

        // Helper to get 0 or 1
        int current_frame() { return _frameNumber % FRAME_OVERLAP; }

        // Helper for the resize logic
        void handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu);

        void create_semaphores(GpuContext& ctx);

        int current_frame() const { return _frameNumber % FRAME_OVERLAP; }
    };
}