project(EditorGUI)

# 1. Define the library
# We'll put our UI logic and state here
add_library(EditorGUI STATIC
        src/EditorGUI.cpp
        include/EditorGUI.hpp
)

# 2. Include Paths
# We need to reach into GpuSystem for the context and ImGui for the widgets
target_include_directories(EditorGUI PUBLIC
        include
        src
        ${CMAKE_SOURCE_DIR}/modules/public/SceneRenderer/include
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui/backends
        ${CMAKE_SOURCE_DIR}/modules/public/GpuSystem/include
)

# 3. Link Dependencies
# EditorGUI "talks" to the SceneRenderer to draw the viewport
target_link_libraries(EditorGUI PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
        SceneRenderer
)

# 4. Register the module
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST EditorGUI)