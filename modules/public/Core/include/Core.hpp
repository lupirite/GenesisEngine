#pragma once

#include "GpuSystem.hpp"
#include "Editor.hpp"
#include "SceneRenderer.hpp"
#include "EditorGUI.hpp"
#include "Renderer.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <Windows.h>
#endif

namespace Genesis {

    class Core {
    public:
        // --- Core Lifecycle ---
        Core();
        ~Core();

        /// Blocks the calling thread and spins up the primary engine runtime loops.
        void run();

        /// Manually forces a synchronous frame processing pass (used for window resize stalling).
        void force_engine_tick();

    private:
        // --- Internal Setup & Processing Steps ---
        void init();
        void main_loop();
        void cleanup();

        // --- Multi-Threaded Task Workers ---
        /// Main Thread Task: Samples inputs and packs frame state parameters into transactional payloads.
        void produce_frame();

        /// Worker Thread Task: Consumes processed packets and submits hardware command streams to the GPU.
        void render_thread_worker();

        // --- Core Sub-System Dependencies ---
        GpuSystem       m_gpu;
        Renderer m_renderer;
        Editor   m_editor;
        EditorGUI       m_gui;
        SceneRenderer   m_scene;

        // --- Threading & Concurrency Infrastructure ---
        std::jthread            m_renderThread;
        std::queue<RenderPacket> m_packetQueue;
        std::mutex              m_queueMutex;
        std::condition_variable m_queueSignal;
        std::mutex              m_imguiMutex; // Guards shared backend ImGui draw structures

        // --- Synchronization Flags & Atomic State ---
        std::atomic<bool> m_running{ true };
        std::atomic<int>  m_packetsInFlight{ 0 };
        bool              m_needsRealResize = false;
        static bool       s_framebufferResized;

#ifdef _WIN32
        // --- Native Win32 Custom Window Procedure Hooking ---
        HWND    m_hwnd = nullptr;
        WNDPROC m_originalWndProc = nullptr;

        /// Static interception hook that captures Windows message loops before passing them back to GLFW.
        static LRESULT CALLBACK window_proc_setup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
    };

} // namespace Genesis