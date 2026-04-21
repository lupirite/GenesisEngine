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
    m_scene.init(ctx, 1280, 720);

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

    while (m_packetsInFlight > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
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
            std::lock_guard<std::mutex> lock(m_imguiMutex);
            vkDeviceWaitIdle(ctx.device);
            m_gui.invalidate_texture();

            // Clear stale packets
            {
                std::lock_guard<std::mutex> qLock(m_queueMutex);
                while(!m_packetQueue.empty()) { m_packetQueue.pop(); m_packetsInFlight--; }
            }

            for (int i = 0; i < GenesisRenderer::MAX_FRAMES_IN_FLIGHT; i++) {
                vkWaitForFences(ctx.device, 1, &m_renderer.get_fence(i), VK_TRUE, UINT64_MAX);
            }

            if (packet.width > 0 && packet.height > 0) {
                m_scene.cleanup(ctx.device);
                m_scene.init(ctx, packet.width, packet.height);
                m_gui.update_texture_descriptor(m_scene, ctx);
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_imguiMutex);
            if (packet.width > 0 && packet.height > 0) {
                m_renderer.draw_frame(ctx, m_gpu, m_scene, m_editor, packet);
            }
        }

        auto renderEnd = std::chrono::high_resolution_clock::now();
        m_gui.set_render_time(std::chrono::duration<float, std::milli>(renderEnd - renderStart).count());
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

        glfwPollEvents();

        if (m_packetsInFlight >= 3) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Re-sync on Wakeup
        if (s_framebufferResized) {
            RenderPacket wakeup;
            wakeup.needsResize = true;
            wakeup.width = curW; wakeup.height = curH;
            wakeup.scenePayload = std::make_unique<SceneSnapshot>();
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_packetQueue.push(std::move(wakeup));
                m_packetsInFlight++;
            }
            m_queueSignal.notify_one();
            while (m_packetsInFlight > 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            s_framebufferResized = false;
        }

        auto logicStart = std::chrono::high_resolution_clock::now();
        auto resizeStatus = m_gui.check_resize(m_scene);

        {
            std::lock_guard<std::mutex> lock(m_imguiMutex);
            m_editor.new_frame();
            if (m_gui.get_scene_texture_id() != (ImTextureID)0) {
                m_gui.render_ui(m_scene, ctx);
            }
            ImGui::Render();
        }

        RenderPacket packet;
        packet.time = (float)glfwGetTime();
        packet.imguiDrawData = ImGui::GetDrawData();
        packet.needsResize = resizeStatus.needed;
        packet.width = resizeStatus.width;
        packet.height = resizeStatus.height;

        auto snapshot = std::make_unique<SceneSnapshot>();
        auto& uiState = m_gui.get_state();
        
        snapshot->sphereRadius = uiState.sphereRadius;

        snapshot->sphereColor[0] = uiState.sphereColor[0];
        snapshot->sphereColor[1] = uiState.sphereColor[1];
        snapshot->sphereColor[2] = uiState.sphereColor[2];

        packet.scenePayload = std::move(snapshot);

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(std::move(packet));
            m_packetsInFlight++;
        }

        auto logicEnd = std::chrono::high_resolution_clock::now();
        m_gui.set_cpu_time(std::chrono::duration<float, std::milli>(logicEnd - logicStart).count());
        m_queueSignal.notify_one();
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