#include <stdexcept>

#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"
#include "EditorGUI.hpp"
#include "GenesisRenderer.hpp"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

int main() {
    Genesis::GpuSystem gpu;
    gpu.init();
    auto& ctx = gpu.get_context();

    Genesis::GenesisRenderer renderer;
    renderer.init(ctx);

    Genesis::GenesisEditor editor;
    editor.init(ctx);

    Genesis::EditorGUI gui;
    Genesis::SceneRenderer myScene;
    myScene.init(ctx, 1280, 720);

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // 1. Prepare UI
        editor.new_frame();
        gui.render_ui(myScene, ctx);

        // 2. Dispatch all rendering to the Renderer
        renderer.draw_frame(ctx, myScene, editor, gui, gpu);
    }

    renderer.cleanup(ctx);
    myScene.cleanup(ctx.device);
    editor.shutdown(ctx.device);
    gpu.cleanup();
}