project(GenesisEditor)

# 1. Define ImGui Source Files
set(IMGUI_DIR "third_party/imgui")
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)

# 2. Create the Editor Library
add_library(GenesisEditor STATIC
        ${IMGUI_SOURCES}
        src/GenesisEditor.cpp
)

# 3. Include Paths
# Note: PRIVATE for backends if you don't want main.cpp to see them directly, 
# but PUBLIC is safer for now to avoid red text.
target_include_directories(GenesisEditor PUBLIC
        include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

# 4. Link Dependencies
# We link GpuSystem so we get Vulkan and GLFW automatically!
target_link_libraries(GenesisEditor PUBLIC
        Vulkan::Vulkan
        GpuSystem       # This gives you GLFW handles for free
        GenesisMath     # Ensure this matches the name in the Math module
)

# --- THE AUTO-REGISTER MAGIC ---
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisEditor)