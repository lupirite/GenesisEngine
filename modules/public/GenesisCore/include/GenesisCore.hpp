#pragma once

#include "GpuSystem.hpp"
#include "GenesisEditor.hpp"
#include "SceneRenderer.hpp"
#include "EditorGUI.hpp"
#include "GenesisRenderer.hpp"

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

    class GenesisCore {
    public:
        GenesisCore();
        ~GenesisCore();

        void run();

        void force_engine_tick();

    private:
        void init();
        void main_loop();
        void render_thread_worker();
        void cleanup();

        void produce_frame();

        bool m_needsRealResize = false;

        // Components
        GpuSystem m_gpu;
        GenesisRenderer m_renderer;
        GenesisEditor m_editor;
        EditorGUI m_gui;
        SceneRenderer m_scene;

        // Threading & Sync
        std::jthread m_renderThread;
        std::queue<RenderPacket> m_packetQueue;
        std::mutex m_queueMutex;
        std::condition_variable m_queueSignal;
        std::mutex m_imguiMutex;

        std::atomic<bool> m_running{true};
        std::atomic<int> m_packetsInFlight{0};

        static bool s_framebufferResized;
        static void framebuffer_size_callback(GLFWwindow* window, int width, int height);

        // Native Win32 handle
        HWND m_hwnd;
        // Storage for the old GLFW procedure so we can pass messages back
        WNDPROC m_originalWndProc;

        // The static "Bridge" that Windows talks to
        static LRESULT CALLBACK window_proc_setup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    };

} // namespace Genesis