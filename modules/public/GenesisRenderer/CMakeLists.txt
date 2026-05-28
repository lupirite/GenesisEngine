project(GenesisRenderer LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Manages multi-frame CPU-GPU synchronization, swapchains, and render loops
add_library(GenesisRenderer STATIC
        src/GenesisRenderer.cpp
        include/GenesisRenderer.hpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
# Reaches into core modules and vendor backend paths for rendering context
target_include_directories(GenesisRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Coordinates execution with the hardware context, UI structures, and pipelines
target_link_libraries(GenesisRenderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
        SceneRenderer
        EditorGUI
)

# ============================================================================
# 4. Global Engine Registration Pass
# ============================================================================
# Appends this module up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisRenderer)