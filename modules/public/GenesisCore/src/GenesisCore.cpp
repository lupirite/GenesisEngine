#include "GenesisCore.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>

namespace Genesis {
    GenesisCore::GenesisCore() {
        init();
    }

    GenesisCore::~GenesisCore() {
        cleanup();
    }

    void GenesisCore::init() {
        m_gpu.init();
        auto& ctx = m_gpu.get_context();

        m_renderer.init(ctx);
        m_editor.init(ctx);

        // 1. Set a solid baseline canvas resolution so the scene isn't blank
        int initHeight = 720;
        int initWidth = (int)((float)(initHeight) * EditorGUI::MAX_ASPECT);
        m_scene.init(ctx, initWidth, initHeight);

        // 2. Set up native Win32 window hooking hooks
        m_hwnd = glfwGetWin32Window(ctx.window);
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
        m_originalWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)window_proc_setup);

        // Get the actual outer application window dimensions
        int startW, startH;
        glfwGetFramebufferSize(ctx.window, &startW, &startH);

        // 3. Set your running state flag to true BEFORE starting any thread
        m_running = true;

        // 4. Create and push the baseline setup packet directly into the queue
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);

            m_packetQueue.emplace();
            auto& setupPacket = m_packetQueue.back();

            setupPacket.needsResize = true;
            setupPacket.width = static_cast<uint32_t>(startW);
            setupPacket.height = static_cast<uint32_t>(startH);
            setupPacket.scenePayload = std::make_unique<SceneSnapshot>();

            m_packetsInFlight++;
        }

        // 5. CRUCIAL: Spawn the background render thread LAST, after the packet is queued
        m_renderThread = std::jthread(&GenesisCore::render_thread_worker, this);
        m_queueSignal.notify_one();
    }

    LRESULT CALLBACK GenesisCore::window_proc_setup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        GenesisCore* core = (GenesisCore*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        if (core) {
            switch (uMsg) {
                case WM_ENTERSIZEMOVE:
                    SetTimer(hWnd, 1, 8, NULL);
                    break;

                case WM_EXITSIZEMOVE:
                    KillTimer(hWnd, 1);
                    core->force_engine_tick();
                    break;

                case WM_TIMER:
                    core->force_engine_tick();
                    return 0;
                case WM_WINDOWPOSCHANGING:
                case WM_WINDOWPOSCHANGED:
                case WM_SIZING:
                case WM_MOVING:

                    return 0;
                case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    BeginPaint(hWnd, &ps);
                    core->force_engine_tick();
                    EndPaint(hWnd, &ps);
                }
                    return 0;
            }
        }
        return CallWindowProc(core->m_originalWndProc, hWnd, uMsg, wParam, lParam);
    }

    void GenesisCore::force_engine_tick() {
        if (!m_running || m_packetsInFlight >= 3) return;

        static auto lastTick = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration<float, std::milli>(now - lastTick).count() < 8.0f) return;

        // Use a standard lock. Because we moved the Scene Init and (hopefully)
        // the VSync Present out of the lock in the worker, this will return
        // almost instantly now.
        {
            std::lock_guard<std::mutex> lock(m_imguiMutex);
            produce_frame();
        }
        lastTick = now;
    }

    void GenesisCore::produce_frame() {
        auto& ctx = m_gpu.get_context();

        int winW, winH;
        glfwGetFramebufferSize(ctx.window, &winW, &winH);

        // Get current actual window dimensions

        bool isSizeMismatched = (winW != m_scene.get_width() || winH != m_scene.get_height());

        // 1. Update UI (ImGui needs to know the real window size to stay clickable)
        // Pass 'false' to check_resize during dragging so it doesn't trigger a scene-reinit
        // 1. Check if the user is actively dragging or just let go of the ImGui viewport window
        // If the user lets go of the mouse button while over the UI layout, commit the resize!
        bool userLetGoOfUi = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        auto resizeStatus = m_gui.check_resize(m_scene, userLetGoOfUi);

        // If the inner panel genuinely changed layout sizes, set our trigger flag
        if (resizeStatus.needed) {
            m_needsRealResize = true;
        }

        // 2. Prepare ImGui Frame
        m_editor.new_frame();
        m_gui.render_ui(m_scene, ctx, isSizeMismatched);

        ImGui::Render();

        RenderPacket packet;
        packet.time = (float)glfwGetTime();
        packet.imguiDrawData = ImGui::GetDrawData();

        // THE MAGIC:
        // width/height = the Window size (for the swapchain/viewport)
        // sceneWidth/sceneHeight = the internal resolution (fixed until let go)
        ImVec2 viewportSize = m_gui.get_viewport_dimensions();
        if (std::isnan(viewportSize.x) || std::isnan(viewportSize.y) ||
        viewportSize.x < 1.0f || viewportSize.y < 1.0f ||
        viewportSize.x > 8192.0f || viewportSize.y > 8192.0f)
        {
            // Use your initial backup dimensions or window frame sizes
            packet.width = (winW > 0) ? static_cast<uint32_t>(winW) : 1280;
            packet.height = (winH > 0) ? static_cast<uint32_t>(winH) : 720;
            packet.needsResize = false; // Block the heavy scene reinit if layout is invalid
        }
        else {
            float wVal = (std::min)(viewportSize.x, EditorGUI::MAX_ASPECT*viewportSize.y);
            packet.width = static_cast<uint32_t>(wVal);
            packet.height = static_cast<uint32_t>(viewportSize.y);
            packet.needsResize = m_needsRealResize;
        }

        m_needsRealResize = false;

        // 4. Capture Scene State
        auto snapshot = std::make_unique<SceneSnapshot>();
        auto& uiState = m_gui.get_state();
        snapshot->sphereRadius = uiState.sphereRadius;
        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];
        packet.scenePayload = std::move(snapshot);

        if (packet.needsResize) {
            std::cout << "[Debug Thread] Resize requested! Packet Size: "
                      << packet.width << "x" << packet.height
                      << " | m_needsRealResize flag status: " << m_needsRealResize
                      << std::endl;
        }

        // 5. Push to Render Thread
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(std::move(packet));
            m_packetsInFlight++;
        }
        m_queueSignal.notify_one();
    }

    void GenesisCore::render_thread_worker() {
        auto& ctx = m_gpu.get_context();

        while (m_running) {
            RenderPacket packet;
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueSignal.wait(lock, [this] { return !m_packetQueue.empty() || !m_running; });
                if (!m_running && m_packetQueue.empty()) break;

                packet = std::move(m_packetQueue.front());
                m_packetQueue.pop();
            }

            auto renderStart = std::chrono::high_resolution_clock::now();

            // 2. Handle the VIEWPORT (Scene)
            // This should only happen if the IMGUI window changed or the move is "Final"
            if (packet.needsResize) { // viewport needs resize
                vkDeviceWaitIdle(ctx.device);
                m_scene.cleanup(ctx.device);
                m_scene.init(ctx, packet.width, packet.height);
                m_gui.update_texture_descriptor(m_scene, ctx);
            }

            if (packet.width > 0 && packet.height > 0 && packet.width < 8192 && packet.height < 8192) {

                // We only lock while we are recording/using the packet's ImGui data.
                // If your m_renderer.draw_frame waits for VSync at the end,
                // try to move that "Present" call OUTSIDE of this lock block.
                {
                    std::lock_guard<std::mutex> lock(m_imguiMutex);
                    m_renderer.draw_frame(ctx, m_gpu, m_scene, m_editor, packet);
                }

                auto renderEnd = std::chrono::high_resolution_clock::now();
                m_gui.set_render_time(std::chrono::duration<float, std::milli>(renderEnd - renderStart).count());
            }

            m_packetsInFlight--;
        }
    }

    void GenesisCore::run() {
        auto& ctx = m_gpu.get_context();

        while (!glfwWindowShouldClose(ctx.window)) {
            int curW, curH;
            glfwGetFramebufferSize(ctx.window, &curW, &curH);

            if (curW == 0 || curH == 0) {
                glfwWaitEvents();
                //s_framebufferResized = true;
                continue;
            }

            if (m_packetsInFlight >= 3) {
                std::this_thread::sleep_for(std::chrono::microseconds(500));
                continue;
            }

            glfwPollEvents();

            {
                auto logicStart = std::chrono::high_resolution_clock::now();

                std::lock_guard<std::mutex> lock(m_imguiMutex);
                produce_frame();

                auto logicEnd = std::chrono::high_resolution_clock::now();
                m_gui.set_cpu_time(std::chrono::duration<float, std::milli>(logicEnd - logicStart).count());
            }
        }
    }

    void GenesisCore::cleanup() {
        m_running = false;
        m_queueSignal.notify_all();
        if (m_renderThread.joinable()) m_renderThread.join();

        auto& ctx = m_gpu.get_context();
        m_renderer.cleanup(ctx);
        m_scene.cleanup(ctx.device);
        m_editor.shutdown(ctx.device);
        m_gpu.cleanup();
    }

} // namespace Genesis