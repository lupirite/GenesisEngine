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