project(GenesisCore)

# 1. Create the Core Library
# This library handles the threading, the main loop, and the packet queue logic
add_library(GenesisCore STATIC
        src/GenesisCore.cpp
)

# 2. Include Paths
# We include the root "include" directory so it can find GenesisCore.hpp
# and the other module headers it depends on.
target_include_directories(GenesisCore PUBLIC
        include
)

# 3. Link Dependencies
# GenesisCore is the "Grand Central Station." It needs to see everything
# to coordinate the logic and render threads.
target_link_libraries(GenesisCore PUBLIC
        Vulkan::Vulkan
        GpuSystem           # For GpuContext and GLFW window
        GenesisRenderer     # For draw_frame and fences
        SceneRenderer       # For the SDF rendering and textures
        GenesisEditor       # For ImGui initialization
        EditorGUI           # For the UI state and resizing logic
)

# --- THE AUTO-REGISTER MAGIC ---
# This ensures that when your top-level CMake runs, it knows to
# include and link GenesisCore into the final binary.
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisCore)