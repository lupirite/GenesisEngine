project(GenesisRenderer)

# 1. Define the library
# We'll put our UI logic and state here
add_library(GenesisRenderer STATIC
        src/GenesisRenderer.cpp
        include/GenesisRenderer.hpp
)

# 2. Include Paths
# We need to reach into GpuSystem for the context and ImGui for the widgets
target_include_directories(GenesisRenderer PUBLIC
        include
        src
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui/backends
)

# 3. Link Dependencies
# GenesisRenderer "talks" to the SceneRenderer to draw the viewport
target_link_libraries(GenesisRenderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
        SceneRenderer
        EditorGUI
)

# 4. Register the module
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisRenderer)