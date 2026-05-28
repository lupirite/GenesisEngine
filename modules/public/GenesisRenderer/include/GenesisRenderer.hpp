#pragma once
#include "GpuSystem.hpp"
#include "SceneRenderer.hpp"
#include "GenesisEditor.hpp"
#include "EditorGUI.hpp"

namespace Genesis {

    class GenesisRenderer {
    public:
        // --- Core Lifecycle Systems ---
        void init(GpuContext& ctx);
        void cleanup(GpuContext& ctx);

        // --- Frame Rendering Pipeline ---
        void draw_frame(GpuContext& ctx, GpuSystem& gpu, SceneRenderer& scene, GenesisEditor& editor, const RenderPacket& packet);
        void render_explicit(VkCommandBuffer cmd, ::ImDrawData* drawData);

        // --- Hardware Throttling Queries ---
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        VkFence& get_fence(int index) { return _renderFences[index]; }

    private:
        static constexpr int FRAME_OVERLAP = 2;

        // Tracking state that increments monotonically each frame loop
        int _frameNumber = 0;

        // CPU-GPU Synchronization Throttles
        VkFence _renderFences[FRAME_OVERLAP] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

        // --- Private Internal Architecture ---
        // Returns the current active virtual execution ring slot (0 or 1)
        int current_frame() const { return _frameNumber % FRAME_OVERLAP; }

        void create_semaphores(GpuContext& ctx);
        void handle_swapchain_resize(GpuContext& ctx, GpuSystem& gpu);
    };

} // namespace Genesis