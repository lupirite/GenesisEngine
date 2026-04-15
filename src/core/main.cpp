#include <stdexcept>
#include <memory> // Required for std::unique_ptr and std::make_unique

#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"
#include "EditorGUI.hpp"
#include "GenesisRenderer.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <imgui.h> // Required for ImGui::Render() and GetDrawData()

// Global or static flag for window resizing
bool g_FramebufferResized = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_FramebufferResized = true;
}

int main() {
    Genesis::GpuSystem gpu;
    gpu.init();
    auto& ctx = gpu.get_context();

    // Set the resize callback
    glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);

    Genesis::GenesisRenderer renderer;
    renderer.init(ctx);

    Genesis::GenesisEditor editor;
    editor.init(ctx);

    Genesis::EditorGUI gui;
    Genesis::SceneRenderer myScene;
    myScene.init(ctx, 1280, 720);

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // 1. Internal Viewport Resize (Moved out of Renderer for clarity)
        // This ensures the Scene texture matches the UI window size
        auto resize = gui.check_resize(myScene);
        if (resize.needed) {
            vkDeviceWaitIdle(ctx.device);
            myScene.cleanup(ctx.device);
            myScene.init(ctx, resize.width, resize.height);
        }

        // 2. Prepare UI (The Producer phase)
        editor.new_frame();
        gui.render_ui(myScene, ctx);

        // Finalize ImGui and generate draw data
        ImGui::Render();

        // 3. Create and Assemble the RenderPacket
        Genesis::RenderPacket packet;
        packet.time = (float)glfwGetTime();
        packet.imguiDrawData = ImGui::GetDrawData();
        packet.needsResize = g_FramebufferResized;
        g_FramebufferResized = false; // Reset the flag immediately

        // 4. Populate Scene-Specific Data
        // We create the snapshot here. The Renderer only sees the IRenderPayload interface.
        auto snapshot = std::make_unique<Genesis::SceneSnapshot>();
        auto& uiState = gui.get_state();

        snapshot->sphereRadius = uiState.sphereRadius;
        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];

        packet.scenePayload = std::move(snapshot);

        // 5. Dispatch to Renderer
        // Note: We still pass 'myScene' and 'editor' because they own the
        // Vulkan resources (pipelines, etc.) used for recording.
        renderer.draw_frame(ctx, gpu, myScene, editor, packet);
    }

    renderer.cleanup(ctx);
    myScene.cleanup(ctx.device);
    editor.shutdown(ctx.device);
    gpu.cleanup();
}