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

project(Core LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Manages multi-threaded pipeline synchronization, pacing, and core loop execution
add_library(Core STATIC
        src/Core.cpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
target_include_directories(Core PUBLIC
        include
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Aggregates all subsystems to coordinate logic and render worker threads
target_link_libraries(Core PUBLIC
        Vulkan::Vulkan
        GpuSystem
        Renderer
        SceneRenderer
        Editor
        EditorGUI
)

# ============================================================================
# 4. Global Engine Registration Pass
# ============================================================================
# Appends this hub up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST Core)