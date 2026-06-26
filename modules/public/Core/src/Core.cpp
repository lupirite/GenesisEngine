//     Genesis Engine - A custom hardware-accelerated software engine.
//     Copyright (C) 2026 Lupirite (contact me at lupirite@gmail.com)
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Core.hpp"
#include <chrono>
#include <iostream>
#include <algorithm>

#include "imgui_internal.h"

namespace Genesis {

    Core::Core() {
        init();
    }

    Core::~Core() {
        cleanup();
    }

    void Core::init() {
        m_gpu.init();
        auto& ctx = m_gpu.get_context();

        m_input.init(m_gpu.get_window());

        // Define the action bindings dynamically
        m_input.bind_action("MoveForward", GLFW_KEY_W);
        m_input.bind_action("MoveBackward", GLFW_KEY_S);
        m_input.bind_action("MoveUp", GLFW_KEY_Q);
        m_input.bind_action("MoveDown", GLFW_KEY_E);
        m_input.bind_action("MoveRight", GLFW_KEY_D);
        m_input.bind_action("MoveLeft", GLFW_KEY_A);
        m_input.bind_action("Jump", GLFW_KEY_SPACE);
        m_input.bind_action("ToggleEditor", GLFW_KEY_GRAVE_ACCENT); // tilde key

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
        m_renderThread = std::jthread(&Core::render_thread_worker, this);
        m_queueSignal.notify_one();
    }

    LRESULT CALLBACK Core::window_proc_setup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        Core* core = reinterpret_cast<Core*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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

    void Core::force_engine_tick() {
        if (!m_running || m_packetsInFlight >= 3) return;

        static auto lastTick = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::high_resolution_clock::now();

        // Clamp frequency to mitigate queue saturation during live OS drag sequences
        if (std::chrono::duration<float, std::milli>(now - lastTick).count() < 8.0f) return;

        {
            std::lock_guard<std::mutex> lock(m_imguiMutex);
            m_input.update_snapshot();
            produce_frame();
        }
        lastTick = now;
    }

    void Core::produce_frame() {
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

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            // Double-check if the specifically hovered window is your game viewport
            // (Assuming your m_gui uses "Game Viewport" as its window title)
            if (ImGui::FindWindowByName("Scene Viewport") == ImGui::GetCurrentContext()->HoveredWindow) {
                m_input.set_mouse_grab(true);
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            m_input.set_mouse_grab(false);
        }

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

        snapshot->camPos[0] = uiState.camPos[0];
        snapshot->camPos[1] = uiState.camPos[1];
        snapshot->camPos[2] = uiState.camPos[2];

        snapshot->camRot[0] = uiState.camRot[0];
        snapshot->camRot[1] = uiState.camRot[1];
        snapshot->camRot[2] = uiState.camRot[2];
        snapshot->camRot[3] = uiState.camRot[3];

        packet.scenePayload = std::move(snapshot);

        // 5. Submit transaction block into flight queue
        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(std::move(packet));
            m_packetsInFlight++;
        }
        m_queueSignal.notify_one();
    }

    void Core::render_thread_worker() {
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

    void Core::run() {
        auto& ctx = m_gpu.get_context();

        auto lastFrameTime = std::chrono::high_resolution_clock::now();

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
                // 1. CALCULATE DELTA TIME
                auto currentFrameTime = std::chrono::high_resolution_clock::now();
                float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
                lastFrameTime = currentFrameTime;

                // Cap deltaTime to avoid massive position warping during a heavy hitch or breakpoint debugging
                if (deltaTime > 0.1f) deltaTime = 0.1f;

                auto logicStart = std::chrono::high_resolution_clock::now();

                // --- SCOPE 1: Safely snapshot input data and update mouse grab ---
                {
                    std::lock_guard<std::mutex> lock(m_imguiMutex);
                    m_input.update_snapshot();
                }

                // 3. Drive intuitive game/tools logic
                if (m_input.get_action_down("ToggleEditor")) {
                    //m_gui.toggle_visible();
                }

                auto& uiState = m_gui.get_state();

                float moveSpeed = 4.0f; // Units per second
                float frameMove = moveSpeed * deltaTime;

                // 1. Transform your camera quaternion into a safe local glm::quat
                // (Ensure uiState.camRot matches your internal glm::quat format)
                glm::quat cameraRotation = uiState.camRot;

                // 2. Extract direction vectors relative to the camera's current rotation
                // In OpenGL/Vulkan, Forward is typically down the negative Z-axis (-Z)
                glm::vec3 localForward = cameraRotation * glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 localRight   = cameraRotation * glm::vec3(1.0f, 0.0f, 0.0f);
                glm::vec3 localUp      = cameraRotation * glm::vec3(0.0f, 1.0f, 0.0f);

                // 3. Convert your current position payload into a temporary vec3 for easier tracking
                glm::vec3 currentPos(uiState.camPos[0], uiState.camPos[1], uiState.camPos[2]);

                // 4. Accrue translations along your directional vectors based on inputs
                if (m_input.get_action("MoveForward")) {
                    currentPos += localForward * frameMove;
                }
                if (m_input.get_action("MoveBackward")) {
                    currentPos -= localForward * frameMove;
                }
                if (m_input.get_action("MoveRight")) {
                    currentPos += localRight * frameMove;
                }
                if (m_input.get_action("MoveLeft")) {
                    currentPos -= localRight * frameMove;
                }
                if (m_input.get_action("MoveUp")) {
                    currentPos += localUp * frameMove;
                }
                if (m_input.get_action("MoveDown")) {
                    currentPos -= localUp * frameMove;
                }

                // 5. Commit the fully offset positional values back to your UI state
                m_gui.set_camera_x(currentPos.x);
                m_gui.set_camera_y(currentPos.y);
                m_gui.set_camera_z(currentPos.z);

                float mouseX = 0.0f;
                float mouseY = 0.0f;

                m_input.get_axis("MouseLook", mouseX, mouseY);

                // 2. Define look sensitivity
                const float sensitivity = 0.004f;
                float yawChange   = -mouseX * sensitivity;
                float pitchChange = mouseY * sensitivity;

                glm::quat localPitch = glm::angleAxis(pitchChange, glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat localYaw   = glm::angleAxis(yawChange,   glm::vec3(0.0f, 1.0f, 0.0f));

                // Optional: Read Roll inputs (Q/E keys) for a true space game feel
                float rollChange = 0.0f;
                if (m_input.get_action("RollLeft"))  rollChange += 2.0f * deltaTime;
                if (m_input.get_action("RollRight")) rollChange -= 2.0f * deltaTime;
                glm::quat localRoll = glm::angleAxis(rollChange, glm::vec3(0.0f, 0.0f, 1.0f));

                m_gui.set_camera_rot(uiState.camRot * localPitch * localYaw * localRoll);
                m_gui.set_camera_rot(glm::normalize(uiState.camRot));

                // --- SCOPE 3: Lock only when calling the UI generator pipeline ---
                {
                    std::lock_guard<std::mutex> lock(m_imguiMutex);
                    produce_frame();
                }

                auto logicEnd = std::chrono::high_resolution_clock::now();
                m_gui.set_cpu_time(std::chrono::duration<float, std::milli>(logicEnd - logicStart).count());            }
        }
    }

    void Core::cleanup() {
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