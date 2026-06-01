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

#include <GLFW/glfw3.h> // <--- MAKE SURE THIS IS FIRST!
#include "InputSystem.hpp"
#include <cstring>

#include "imgui.h"

namespace Genesis {

    void InputSystem::init(GLFWwindow* window) {
        if (!window) return;

        // Store this instance inside the window data payload for callback extraction
        glfwSetWindowUserPointer(window, this);

        // Register hardware event callbacks with GLFW
        glfwSetKeyCallback(window, InputSystem::key_callback);
        glfwSetCursorPosCallback(window, InputSystem::mouse_callback);

        // Initialize historical position references to prevent single-frame massive jumps
        double startX, startY;
        glfwGetCursorPos(window, &startX, &startY);

        m_backendBuffer.mouseX = startX;
        m_backendBuffer.mouseY = startY;
        m_snapshotBuffer.mouseX = startX;
        m_snapshotBuffer.mouseY = startY;
        m_previousSnapshot.mouseX = startX;
        m_previousSnapshot.mouseY = startY;
    }

    void InputSystem::bind_action(std::string_view actionName, int glfwKeyCode) {
        if (glfwKeyCode >= 0 && glfwKeyCode < 512) {
            m_actionBindings[std::string(actionName)] = glfwKeyCode;
        }
    }

    void InputSystem::bind_axis(std::string_view axisName, int glfwMouseOrAxisID) {
        // Reserved for matching custom joystick/mouse structural profiles if needed later.
        // For standard mouse look, we can implicitly map standard axes names directly.
        m_actionBindings[std::string(axisName)] = glfwMouseOrAxisID;
    }

    void InputSystem::update_snapshot() {
        // Roll old frame context over into history tracker
        m_previousSnapshot = m_snapshotBuffer;

        auto& io = ImGui::GetIO();

        // UI Mask Pass: If ImGui needs keyboard context, swallow engine game actions
        if (io.WantCaptureKeyboard) {
            std::memset(m_snapshotBuffer.keys, 0, sizeof(m_snapshotBuffer.keys));
        } else {
            std::memcpy(m_snapshotBuffer.keys, m_backendBuffer.keys, sizeof(m_snapshotBuffer.keys));
        }

        // UI Mask Pass: Manage cursor delta pooling based on active viewport coverage
        if (io.WantCaptureMouse) {
            m_snapshotBuffer.mouseX = m_backendBuffer.mouseX;
            m_snapshotBuffer.mouseY = m_backendBuffer.mouseY;
            m_snapshotBuffer.mouseDeltaX = 0.0;
            m_snapshotBuffer.mouseDeltaY = 0.0;
        } else {
            m_snapshotBuffer.mouseX = m_backendBuffer.mouseX;
            m_snapshotBuffer.mouseY = m_backendBuffer.mouseY;

            // Extract the accumulated asynchronous movement deltas
            m_snapshotBuffer.mouseDeltaX = m_backendBuffer.mouseDeltaX;
            m_snapshotBuffer.mouseDeltaY = m_backendBuffer.mouseDeltaY;
        }

        // Clear backend delta accumulations to prepare for the next asynchronous sampling phase
        m_backendBuffer.mouseDeltaX = 0.0;
        m_backendBuffer.mouseDeltaY = 0.0;
    }

    bool InputSystem::get_action(std::string_view actionName) const {
        auto it = m_actionBindings.find(std::string(actionName));
        if (it == m_actionBindings.end()) return false;

        return m_snapshotBuffer.keys[it->second];
    }

    bool InputSystem::get_action_down(std::string_view actionName) const {
        auto it = m_actionBindings.find(std::string(actionName));
        if (it == m_actionBindings.end()) return false;

        int code = it->second;
        return m_snapshotBuffer.keys[code] && !m_previousSnapshot.keys[code];
    }

    bool InputSystem::get_action_up(std::string_view actionName) const {
        auto it = m_actionBindings.find(std::string(actionName));
        if (it == m_actionBindings.end()) return false;

        int code = it->second;
        return !m_snapshotBuffer.keys[code] && m_previousSnapshot.keys[code];
    }

    void InputSystem::get_axis(std::string_view axisName, float& outX, float& outY) const {
        // Standardized axis query lookup for typical delta structures
        if (axisName == "MouseLook" || axisName == "MouseDelta") {
            outX = static_cast<float>(m_snapshotBuffer.mouseDeltaX);
            outY = static_cast<float>(m_snapshotBuffer.mouseDeltaY);
            return;
        }
        outX = 0.0f;
        outY = 0.0f;
    }

    // ============================================================================
    // Asynchronous Hardware ISR Layer (Invoked on Main thread via GLFW event pool)
    // ============================================================================
    void InputSystem::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key < 0 || key >= 512) return;

        InputSystem* instance = reinterpret_cast<InputSystem*>(glfwGetWindowUserPointer(window));
        if (!instance) return;

        if (action == GLFW_PRESS) {
            instance->m_backendBuffer.keys[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            instance->m_backendBuffer.keys[key] = false;
        }
    }

    void InputSystem::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        InputSystem* instance = reinterpret_cast<InputSystem*>(glfwGetWindowUserPointer(window));
        if (!instance) return;

        // Accumulate directional velocity vectors until the frame synchronization step claims them
        instance->m_backendBuffer.mouseDeltaX += (xpos - instance->m_backendBuffer.mouseX);
        instance->m_backendBuffer.mouseDeltaY += (ypos - instance->m_backendBuffer.mouseY);

        instance->m_backendBuffer.mouseX = xpos;
        instance->m_backendBuffer.mouseY = ypos;
    }

} // namespace Genesis