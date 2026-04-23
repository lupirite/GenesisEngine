#include "GenesisCore.hpp"
#include <chrono>

namespace Genesis {

    bool GenesisCore::s_framebufferResized = false;

    void GenesisCore::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
        s_framebufferResized = true;
    }

    GenesisCore::GenesisCore() {
        init();
    }

    GenesisCore::~GenesisCore() {
        cleanup();
    }

    void GenesisCore::init() {
        m_gpu.init();
        auto& ctx = m_gpu.get_context();

        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);

        m_renderer.init(ctx);
        m_editor.init(ctx);
        int initHeight = 720;
        int initWidth = (int)((float)(initHeight)*EditorGUI::MAX_ASPECT);
        m_scene.init(ctx, initWidth, initHeight);

        // Start the worker
        m_renderThread = std::jthread(&GenesisCore::render_thread_worker, this);

        // Initial Setup Packet
        s_framebufferResized = true;
        int startW, startH;
        glfwGetFramebufferSize(ctx.window, &startW, &startH);

        RenderPacket setupPacket;
        setupPacket.needsResize = true;
        setupPacket.width = (uint32_t)startW;
        setupPacket.height = (uint32_t)startH;
        setupPacket.scenePayload = std::make_unique<SceneSnapshot>();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(std::move(setupPacket));
            m_packetsInFlight++;
        }
        m_queueSignal.notify_one();

        m_hwnd = glfwGetWin32Window(ctx.window);

        // Store "this" pointer in the HWND so the static callback can find the instance
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

        // Swap the procedure
        m_originalWndProc = (WNDPROC)SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, (LONG_PTR)window_proc_setup);

        while (m_packetsInFlight > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
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
                    // THE COMMIT: Now that the user let go, tell the engine to REALLY resize
                    core->m_needsRealResize = true;
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

        // 1. Update UI (ImGui needs to know the real window size to stay clickable)
        // Pass 'false' to check_resize during dragging so it doesn't trigger a scene-reinit
        auto resizeStatus = m_gui.check_resize(m_scene, false);

        // 2. Prepare ImGui Frame
        m_editor.new_frame();
        if (m_gui.get_scene_texture_id() != (ImTextureID)0) {
            m_gui.render_ui(m_scene, ctx);
        }
        ImGui::Render();

        RenderPacket packet;
        packet.time = (float)glfwGetTime();
        packet.imguiDrawData = ImGui::GetDrawData();

        // THE MAGIC:
        // width/height = the Window size (for the swapchain/viewport)
        // sceneWidth/sceneHeight = the internal resolution (fixed until let go)
        packet.width = (resizeStatus.width > 0) ? resizeStatus.width : (uint32_t)winW;
        packet.height = (resizeStatus.height > 0) ? resizeStatus.height : (uint32_t)winH;
        packet.needsResize = m_needsRealResize;

        m_needsRealResize = false;

        // 4. Capture Scene State
        auto snapshot = std::make_unique<SceneSnapshot>();
        auto& uiState = m_gui.get_state();
        snapshot->sphereRadius = uiState.sphereRadius;
        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];
        packet.scenePayload = std::move(snapshot);

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

            if (packet.needsResize) {
                vkDeviceWaitIdle(ctx.device);
                // ONLY re-init the heavy Scene buffer if we've committed the move
                m_scene.cleanup(ctx.device);

                if (m_needsRealResize) {
                    m_scene.init(ctx, packet.width, packet.height);
                } else {
                    uint32_t safetyWidth = static_cast<uint32_t>(packet.height * EditorGUI::MAX_ASPECT);
                    m_scene.init(ctx, safetyWidth, packet.height);
                }
                m_gui.update_texture_descriptor(m_scene, ctx);
            }

            if (packet.width > 0 && packet.height > 0) {
                auto renderStart = std::chrono::high_resolution_clock::now();

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
                s_framebufferResized = true;
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