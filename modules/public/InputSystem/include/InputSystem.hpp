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

#pragma once
#include <unordered_map>
#include <string>
#include <string_view>

namespace Genesis {

    enum class KeyState {
        None,
        Pressed,
        Held,
        Released
    };

    class InputSystem {
    public:
        // --- Setup & Configuration ---
        void init(struct GLFWwindow* window);

        /// Binds an abstract action name to a physical hardware key code.
        void bind_action(std::string_view actionName, int glfwKeyCode);
        void bind_axis(std::string_view axisName, int glfwMouseOrAxisID);

        // --- Thread Synchronization ---
        /// Copies the backend hardware states into the isolated logic snapshot.
        void update_snapshot();

        // --- Intuitive Queries for Game Logic & Tools ---
        /// True continuously every frame the action key is held down.
        bool get_action(std::string_view actionName) const;

        /// True ONLY on the exact frame the key transitions from up to down.
        bool get_action_down(std::string_view actionName) const;

        /// True ONLY on the exact frame the key transitions from down to up.
        bool get_action_up(std::string_view actionName) const;

        /// Returns continuous multi-dimensional movement delta vectors (e.g. Mouse X/Y).
        void get_axis(std::string_view axisName, float& outX, float& outY) const;

    private:
        // Raw hardware callback hooks (Must be static for GLFW to interface with them)
        static void key_callback(struct GLFWwindow* window, int key, int scancode, int action, int mods);
        static void mouse_callback(struct GLFWwindow* window, double xpos, double ypos);

        // Map containing lookups from "Jump" -> GLFW_KEY_SPACE
        std::unordered_map<std::string, int> m_actionBindings;

        // Double buffers for clean thread isolation
        struct InputStateBuffer {
            bool keys[512] = { false };
            double mouseX = 0.0, mouseY = 0.0;
            double mouseDeltaX = 0.0, mouseDeltaY = 0.0;
        };

        InputStateBuffer m_backendBuffer;   // Written to dynamically by hardware thread
        InputStateBuffer m_snapshotBuffer;  // Read from statically by logic loop
        InputStateBuffer m_previousSnapshot;// Used to calculate target single-frame triggers
    };

} // namespace Genesis