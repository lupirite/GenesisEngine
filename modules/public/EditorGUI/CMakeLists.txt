project(EditorGUI LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Encapsulates layout data tracking and UI canvas interaction logic
add_library(EditorGUI STATIC
        src/EditorGUI.cpp
        include/EditorGUI.hpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
# Connects the interface to hardware contexts, offscreen renderers, and ImGui hooks
target_include_directories(EditorGUI PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Allows the UI layer to communicate directly with graphics sub-systems
target_link_libraries(EditorGUI PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
        SceneRenderer
)

# ============================================================================
# 4. Global Engine Registration Pass
# ============================================================================
# Appends this module up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST EditorGUI)