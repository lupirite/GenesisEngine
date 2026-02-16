cmake_minimum_required(VERSION 3.21)
project(GenesisND VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find Vulkan first
find_package(Vulkan REQUIRED)

# 2. CREATE the target (Crucial: Do this BEFORE target_include_directories)
# Adjust the path to where your main.cpp actually is
add_executable(GenesisND src/core/main.cpp)

# 3. CONFIGURE the target
target_include_directories(GenesisND PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/shared
)

target_link_libraries(GenesisND PRIVATE Vulkan::Vulkan)

# 4. Math Parity Check (Will show in CLion's CMake tab)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Warning: Using ${CMAKE_CXX_COMPILER_ID}. Switch to Clang for bit-perfect math.")
endif()