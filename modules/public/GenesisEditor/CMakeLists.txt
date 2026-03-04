project(Genesis_Editor)

# 1. Define ImGui Source Files
# You need the core files + the Vulkan/GLFW (or Win32) backends
set(IMGUI_DIR "third_party/imgui")
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui.h             # Added
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_demo.cpp

        # Backends
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.h    # Added
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.h      # Added
)

# 2. Create the Editor Library
add_library(Genesis_Editor STATIC
        ${IMGUI_SOURCES}
        src/GenesisEditor.cpp
)

# 3. Include Paths
target_include_directories(Genesis_Editor PUBLIC
        include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

find_package(glfw3 REQUIRED)

# 4. Link Dependencies
# The Editor needs Vulkan and your Math module!
target_link_libraries(Genesis_Editor PUBLIC
        Vulkan::Vulkan
        glfw
        Genesis_Math
)

# --- THE AUTO-REGISTER MAGIC ---
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST Genesis_Editor)