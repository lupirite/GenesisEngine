project(SceneRenderer)

# 1. Define the library
# It's good practice to list headers here so IDEs like CLion
# track them for "Refactor" and "Find Usages"
add_library(SceneRenderer STATIC
        src/SceneRenderer.cpp
        include/SceneRenderer.hpp
)

# 2. THE FIX: Include Paths
# This tells anyone who links SceneRenderer (like main.cpp)
# to look in the 'include' folder.
target_include_directories(SceneRenderer PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # Paths to find ImGui headers (adjust if your structure differs)
        ${CMAKE_SOURCE_DIR}/modules/GenesisEditor/third_party/imgui
        ${CMAKE_SOURCE_DIR}/modules/GenesisEditor/third_party/imgui/backends
)

# 3. Link Dependencies
target_link_libraries(SceneRenderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
)

set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST SceneRenderer)