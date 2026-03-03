#include "GpuSystem.hpp"
#include <GLFW/glfw3.h> // This fixes glfwWindowShouldClose and glfwPollEvents
#include <vulkan/vulkan.h> // This fixes VkClearValue

int main() {
    Genesis::GpuSystem gpu;
    gpu.init();
    auto& ctx = gpu.get_context();

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // If you see a Dark Grey window, the GpuSystem is 100% working!
        // (This is the minimum code to see a result)
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};

        // This is where you'd normally start your render pass...
    }

    gpu.cleanup();
    return 0;
}