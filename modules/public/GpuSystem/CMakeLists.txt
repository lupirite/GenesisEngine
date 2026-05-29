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

project(GpuSystem LANGUAGES CXX)

# ============================================================================
# 1. Vendor Dependency Configuration (GLFW Optimization Flags)
# ============================================================================
# Suppress redundant vendor targets to maintain clean, fast build-pacing
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Incorporate raw window system platform layers
add_subdirectory(third_party/glfw)

# ============================================================================
# 2. Target Library Definition
# ============================================================================
# Bundles context initialization, bootstrap helpers, and window creation together
add_library(GpuSystem STATIC
        src/GpuSystem.cpp
        third_party/VkBootstrap.cpp
)

# ============================================================================
# 3. Header Include Search Paths Configuration
# ============================================================================
target_include_directories(GpuSystem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# 4. Direct Interface Dependency Linking Phase
# ============================================================================
target_link_libraries(GpuSystem PUBLIC
        Vulkan::Vulkan
        glfw
)

# ============================================================================
# 5. Global Engine Registration Pass
# ============================================================================
# Appends this hardware foundation layer up into the compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GpuSystem)