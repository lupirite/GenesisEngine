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

        // 1. Establish a fallback aspect-locked resolution to prevent blank canvas states
        int initHeight = 720;
        int initWidth = static_cast<int>(static_cast<float>(initHeight) * EditorGUI::MAX_ASPECT);
        m_scene.init(ctx, initWidth, initHeight);

        // 2. Intercept native Win32 window messages to smooth out resizing artifacts
        m_hwnd = glfwGetWin32Window(ctx.window);
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(window_proc_setup)));

        int startW, startH;
        glfwGetFramebufferSize(ctx.window, &startW, &startH);

        // 3. Set running state active before kicking off threads
        m_running = true;

        // 4. Generate and dispatch the first baseline layout setup packet
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

        // 5. Spawn background worker thread last once initialization data is fully queued
        m_renderThread = std::jthread(&GenesisCore::render_thread_worker, this);
        m_queueSignal.notify_one();
    }

    LRESULT CALLBACK GenesisCore::window_proc_setup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        GenesisCore* core = reinterpret_cast<GenesisCore*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

        if (core) {
            switch (uMsg) {
                case WM_ENTERSIZEMOVE:
                    // Trigger high-frequency paint events while dragging the frame
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

                case WM_PAINT: {
                    PAINTSTRUCT ps;
                    BeginPaint(hWnd, &ps);
                    core->force_engine_tick();
                    EndPaint(hWnd, &ps);
                    return 0;
                }
            }
        }
        return CallWindowProc(core->m_originalWndProc, hWnd, uMsg, wParam, lParam);
    }

    void GenesisCore::force_engine_tick() {
        if (!m_running || m_packetsInFlight >= 3) return;

        static auto lastTick = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();

        // Clamp frequency to mitigate queue saturation during live OS drag sequences
        if (std::chrono::duration<float, std::milli>(now - lastTick).count() < 8.0f) return;

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

        bool isSizeMismatched = (winW != m_scene.get_width() || winH != m_scene.get_height());

        // 1. Process active layout adjustments from the UI interface
        bool userLetGoOfUi = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        auto resizeStatus = m_gui.check_resize(m_scene, userLetGoOfUi);

        if (resizeStatus.needed) {
            m_needsRealResize = true;
        }

        // 2. Assemble UI draw lists
        m_editor.new_frame();
        m_gui.render_ui(m_scene, ctx, isSizeMismatched);
        ImGui::Render();

        // 3. Initialize fresh data packet for consumer thread distribution
        RenderPacket packet;
        packet.time = static_cast<float>(glfwGetTime());
        packet.imguiDrawData = ImGui::GetDrawData();

        ImVec2 viewportSize = m_gui.get_viewport_dimensions();

        // Handle layout safety fallbacks against negative or corrupted panel states
        if (std::isnan(viewportSize.x) || std::isnan(viewportSize.y) ||
            viewportSize.x < 1.0f || viewportSize.y < 1.0f ||
            viewportSize.x > 8192.0f || viewportSize.y > 8192.0f)
        {
            packet.width = (winW > 0) ? static_cast<uint32_t>(winW) : 1280;
            packet.height = (winH > 0) ? static_cast<uint32_t>(winH) : 720;
            packet.needsResize = false;
        }
        else {
            float wVal = (std::min)(viewportSize.x, EditorGUI::MAX_ASPECT * viewportSize.y);
            packet.width = static_cast<uint32_t>(wVal);
            packet.height = static_cast<uint32_t>(viewportSize.y);
            packet.needsResize = m_needsRealResize;
        }

        m_needsRealResize = false;

        // 4. Serialize a snapshot of current uniform parameters
        auto snapshot = std::make_unique<SceneSnapshot>();
        auto& uiState = m_gui.get_state();

        snapshot->sphereRadius = uiState.sphereRadius;
        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];
        packet.scenePayload = std::move(snapshot);

        // 5. Submit transaction block into flight queue
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

            // Pop the oldest packet from the front of the tracking queue
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_queueSignal.wait(lock, [this] { return !m_packetQueue.empty() || !m_running; });
                if (!m_running && m_packetQueue.empty()) break;

                packet = std::move(m_packetQueue.front());
                m_packetQueue.pop();
            }

            auto renderStart = std::chrono::high_resolution_clock::now();

            // Reallocate Vulkan structural framing properties exclusively during finalized layout steps
            if (packet.needsResize) {
                vkDeviceWaitIdle(ctx.device);
                m_scene.cleanup(ctx.device);
                m_scene.init(ctx, packet.width, packet.height);
                m_gui.update_texture_descriptor(m_scene, ctx);
            }

            if (packet.width > 0 && packet.height > 0 && packet.width < 8192 && packet.height < 8192) {
                // Record draw calls and execute frame render pass calculations
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

            // Halt updating loops if the context window is minimized
            if (curW == 0 || curH == 0) {
                glfwWaitEvents();
                continue;
            }

            // Impose backpressure limitations to prevent producing faster than consuming (Triple Buffering cap)
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