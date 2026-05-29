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

project(Renderer LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Manages multi-frame CPU-GPU synchronization, swapchains, and render loops
add_library(Renderer STATIC
        src/Renderer.cpp
        include/Renderer.hpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
# Reaches into core modules and vendor backend paths for rendering context
target_include_directories(Renderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Coordinates execution with the hardware context, UI structures, and pipelines
target_link_libraries(Renderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        Editor
        SceneRenderer
        EditorGUI
)

# ============================================================================
# 4. Global Engine Registration Pass
# ============================================================================
# Appends this module up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST Renderer)