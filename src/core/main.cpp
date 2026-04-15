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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#define LOG_GRAPHICS 0

// Global or static flag for window resizing
bool g_FramebufferResized = false;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    g_FramebufferResized = true;
}

std::queue<Genesis::RenderPacket> g_PacketQueue;
std::mutex g_QueueMutex;
std::condition_variable g_QueueSignal;
std::atomic<bool> g_Running{true};
std::atomic<int> g_PacketsInFlight{0};
std::mutex g_ImGuiMutex;

void render_thread_worker(Genesis::GpuContext& ctx,
                          Genesis::GpuSystem& gpu,
                          Genesis::GenesisRenderer& renderer,
                          Genesis::SceneRenderer& scene,
                          Genesis::GenesisEditor& editor,
                          Genesis::EditorGUI& gui) {

    double lastTime = glfwGetTime();
    int frameCount = 0;
    double fps = 0;

    while (g_Running) {
        Genesis::RenderPacket packet;

        // 1. Wait for work
        {
            std::unique_lock<std::mutex> lock(g_QueueMutex);
            g_QueueSignal.wait(lock, [] { return !g_PacketQueue.empty() || !g_Running; });

            if (!g_Running && g_PacketQueue.empty()) break;

            packet = std::move(g_PacketQueue.front());
            g_PacketQueue.pop();
        }

        // 2. Handle Scene Resizing (Thread-Safe logic)
        // We do this BEFORE drawing so the texture is ready for the current frame
        if (packet.needsResize) {
            std::lock_guard<std::mutex> lock(g_ImGuiMutex);
            if (LOG_GRAPHICS) printf("[RENDER THREAD] Mutex Locked. Invalidating texture...\n");

            if (LOG_GRAPHICS) printf("[RENDER THREAD] Starting Resize. Waiting for Idle...\n");
            // 1. Wait for the entire GPU to finish current work
            vkDeviceWaitIdle(ctx.device);

            // 2. Invalidate UI so Main Thread doesn't try to draw the "Ghost"
            gui.invalidate_texture();

            {
                std::lock_guard<std::mutex> qLock(g_QueueMutex);
                while(!g_PacketQueue.empty()) {
                    g_PacketQueue.pop();
                    g_PacketsInFlight--;
                }
            }

            // 3. THE FLUSH: Wait for all frame slots to be officially done
            for (int i = 0; i < Genesis::GenesisRenderer::MAX_FRAMES_IN_FLIGHT; i++) {
                vkWaitForFences(ctx.device, 1, &renderer.get_fence(i), VK_TRUE, UINT64_MAX);
            }

            int w, h;
            glfwGetFramebufferSize(ctx.window, &w, &h);

            // 4. Safe to destroy now—nothing is using the old sampler!
            if (LOG_GRAPHICS) printf("[RENDER THREAD] Cleaning up Scene. Destroying Sampler: %p\n", (void*)scene.get_sampler());
            scene.cleanup(ctx.device);
            scene.init(ctx, (uint32_t)w, (uint32_t)h);

            if (LOG_GRAPHICS) printf("[RENDER THREAD] Scene Re-init. New Sampler: %p\n", (void*)scene.get_sampler());

            gui.update_texture_descriptor(scene, ctx);
            if (LOG_GRAPHICS) printf("[RENDER THREAD] Resize Complete. Mutex Released.\n");
        }

        // 3. The heavy Vulkan work
        // All command recording and queue submission happens here
        {
            std::lock_guard<std::mutex> lock(g_ImGuiMutex); // This is safe as long as Main thread releases it
            renderer.draw_frame(ctx, gpu, scene, editor, packet);
        }

        frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0) { // Update every second
            fps = double(frameCount) / (currentTime - lastTime);
            printf("[RENDER FPS] %.2f\n", fps);
            frameCount = 0;
            lastTime = currentTime;
        }
        // After the lock is released, decrement packets in flight
        g_PacketsInFlight--;
    }
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

    std::jthread renderThread(render_thread_worker,
                          std::ref(ctx),
                          std::ref(gpu),
                          std::ref(renderer),
                          std::ref(myScene),
                          std::ref(editor),
                          std::ref(gui));

    g_FramebufferResized = true;

    {
        Genesis::RenderPacket setupPacket;
        setupPacket.needsResize = true; // Force descriptor link
        setupPacket.scenePayload = std::make_unique<Genesis::SceneSnapshot>();

        {
            std::lock_guard<std::mutex> lock(g_QueueMutex);
            g_PacketQueue.push(std::move(setupPacket));
            g_PacketsInFlight++;
        }
        g_QueueSignal.notify_one();
    }

    // WAIT for the worker to finish that first setup packet
    // This ensures the GUI has its texture BEFORE the first ImGui::Render()
    while (g_PacketsInFlight > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();

        // 1. Throttle (Backpressure)
        // If we have more than 2 frames waiting, just wait a millisecond
        if (g_PacketsInFlight >= 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
            continue;
        }

        // 2. Check for UI-driven resizes
        auto resizeStatus = gui.check_resize(myScene);

        // 3. Prepare UI (The Producer phase)
        {
            std::lock_guard<std::mutex> lock(g_ImGuiMutex);
            editor.new_frame();

            // --- THE STABILIZER ---
            // If the worker just finished a resize, the scene is ready.
            // We link it here on the Main Thread where ImGui is active.
            if (LOG_GRAPHICS) printf("[MAIN THREAD] Rendering UI. Current sceneTextureID: %p\n", (void*)gui.get_scene_texture_id());

            // SAFETY CHECK: Only render the scene viewport if we have a valid handle
            if (gui.get_scene_texture_id() != (ImTextureID)0) {
                gui.render_ui(myScene, ctx);
            } else {
                // Fallback UI so the window isn't just a black void during init
                ImGui::Begin("Scene Viewport");
                ImGui::Text("Initializing Genesis Renderer...");
                ImGui::End();
            }

            ImGui::Render();
        }

        // 4. Assemble Packet
        Genesis::RenderPacket packet;
        packet.time = (float)glfwGetTime();
        packet.imguiDrawData = ImGui::GetDrawData();
        packet.needsResize = g_FramebufferResized || resizeStatus.needed;
        g_FramebufferResized = false;

        // ... Populate Snapshot ...
        auto snapshot = std::make_unique<Genesis::SceneSnapshot>();
        auto& uiState = gui.get_state();
        snapshot->sphereRadius = uiState.sphereRadius;
        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];
        packet.scenePayload = std::move(snapshot);

        // 5. Hand-off
        {
            std::lock_guard<std::mutex> lock(g_QueueMutex);
            g_PacketQueue.push(std::move(packet));
            g_PacketsInFlight++;
        }
        g_QueueSignal.notify_one();
    }

    // 5. Clean Shutdown
    g_Running = false;
    g_QueueSignal.notify_all();

    // Force the main thread to wait until the render thread actually stops
    if (renderThread.joinable()) {
        renderThread.join();
    }

    // Now it is 100% safe to destroy Vulkan resources
    renderer.cleanup(ctx);
    myScene.cleanup(ctx.device);
    editor.shutdown(ctx.device);
    gpu.cleanup();
}