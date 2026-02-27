cmake_minimum_required(VERSION 3.21)
project(GenesisND VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# External Dependencies
find_package(Vulkan REQUIRED)

add_compile_options(-fno-fast-math)

# Create a global list to store our module names
set_property(GLOBAL PROPERTY GENESIS_MODULE_LIST "")

# 1. Find and Add all subdirectories in modules/
file(GLOB_RECURSE MODULE_CMAKES "modules/*/CMakeLists.txt")

foreach(MODULE_FILE ${MODULE_CMAKES})
    get_filename_component(MODULE_DIR ${MODULE_FILE} DIRECTORY)
    add_subdirectory(${MODULE_DIR})
endforeach()

# Target Creation
add_executable(GenesisND src/core/main.cpp)

# 2. Retrieve the list of all modules that registered themselves
get_property(TOTAL_MODULES GLOBAL PROPERTY GENESIS_MODULE_LIST)

# 3. Link them all at once
target_link_libraries(GenesisND PRIVATE
        Vulkan::Vulkan
        ${TOTAL_MODULES}
)

# Include and Link
target_include_directories(GenesisND PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include/shared
)
target_link_libraries(GenesisND PRIVATE Vulkan::Vulkan)

# 1. Find all modules
# This looks for any CMakeLists.txt inside modules/official/ or modules/project/
file(GLOB_RECURSE MODULE_LIST "modules/*/CMakeLists.txt")

# Strict "Genesis Laws" (Target-Specific)
target_compile_options(GenesisND PRIVATE
        -Wall
        -Wextra
        -Wconversion
        -Wfloat-conversion
        -Werror=conversion
        -Werror=float-conversion
        -Wno-unused-parameter  # Optional: depends on how much you hate warnings
)

# Compiler Validation
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(AUTHOR_WARNING "Genesis Build System: Using ${CMAKE_CXX_COMPILER_ID}. Clang is recommended for bit-perfect cross-platform parity.")
endif()