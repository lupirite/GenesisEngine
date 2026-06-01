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

project(InputSystem LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Manages multi-threaded action-mapped hardware event propagation
add_library(InputSystem STATIC
        src/InputSystem.cpp
        include/InputSystem.hpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
# Publicly exports its include directory so downstream modules inherit paths
target_include_directories(InputSystem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Inherits Vulkan APIs and GLFW window abstraction wrappers directly
target_link_libraries(InputSystem PUBLIC
        Vulkan::Vulkan
        GpuSystem
        Editor
)

# ============================================================================
# 4. Global Engine Registration Pass
# ============================================================================
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST InputSystem)
