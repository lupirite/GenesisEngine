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

cmake_minimum_required(VERSION 3.21)
project(GenesisND VERSION 0.1.0 LANGUAGES CXX)

# --- Global Compiler Settings ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fno-fast-math)

# Compiler Validation Warning Configuration
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(AUTHOR_WARNING "Genesis Build System: Using ${CMAKE_CXX_COMPILER_ID}. Clang is recommended for bit-perfect cross-platform parity.")
endif()

# --- External System Packages ---
find_package(Vulkan REQUIRED)

# Initialize the global tracking interface property for our modules
set_property(GLOBAL PROPERTY GENESIS_MODULE_LIST "")

# ============================================================================
# 1. Automatic Dynamic Sub-Module Discovery Pass
# ============================================================================
file(GLOB_RECURSE MODULE_CMAKES "modules/*/CMakeLists.txt")

foreach(MODULE_FILE ${MODULE_CMAKES})
    # Protect pipeline builds from traversing nested third-party dependencies
    if(MODULE_FILE MATCHES "third_party")
        continue()
    endif()

    get_filename_component(MODULE_DIR ${MODULE_FILE} DIRECTORY)
    add_subdirectory(${MODULE_DIR})
endforeach()

# ============================================================================
# 2. Main Native Executable Construction
# ============================================================================
add_executable(GenesisND src/core/main.cpp
)

# Header Location Paths Configuration
target_include_directories(GenesisND PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/shared
)

# Enforce uniform language compliance down into target boundaries
target_compile_features(GenesisND PUBLIC cxx_std_20)

# ============================================================================
# 3. Dynamic Dependency Linking Phase
# ============================================================================
get_property(TOTAL_MODULES GLOBAL PROPERTY GENESIS_MODULE_LIST)
message(STATUS "Genesis Modules Found and Validated: ${TOTAL_MODULES}")

target_link_libraries(GenesisND PRIVATE
        Vulkan::Vulkan
        ${TOTAL_MODULES}
)

# ============================================================================
# 4. Strict Code Quality & Optimization Profiles ("Genesis Laws")
# ============================================================================
target_compile_options(GenesisND PRIVATE
        -Wall
        #-Wextra
        -Wconversion
        -Wfloat-conversion
        -Werror=conversion
        -Werror=float-conversion
        -Wno-unused-parameter
)