#     Genesis Engine - A custom hardware-accelerated software engine.
#     Copyright (C) 2026 Lupirite (contact me at lupirite@gmail.com)
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <https://www.gnu.org/licenses/>.

project(Editor LANGUAGES CXX)

# ============================================================================
# 1. Dear ImGui Core & Backend Source Inventory
# ============================================================================
set(IMGUI_DIR "third_party/imgui")
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# ============================================================================
# 2. Target Library Definition
# ============================================================================
# Compiles vendor user-interface widgets together with Genesis wrapper layer wrappers
add_library(Editor STATIC
        ${IMGUI_SOURCES}
        src/Editor.cpp
)

# ============================================================================
# 3. Header Include Search Paths Configuration
# ============================================================================
# Exposes vendor headers publicly to ensure viewport widgets remain unclipped
target_include_directories(Editor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}/backends
)

# ============================================================================
# 4. Direct Interface Dependency Linking Phase
# ============================================================================
# Inherits window system backends, math utilities, and Vulkan context handles
target_link_libraries(Editor PUBLIC
        Vulkan::Vulkan
        GpuSystem
)

# ============================================================================
# 5. Global Engine Registration Pass
# ============================================================================
# Appends this module up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST Editor)