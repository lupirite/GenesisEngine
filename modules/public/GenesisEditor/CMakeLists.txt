project(GenesisEditor LANGUAGES CXX)

# ============================================================================
# 1. Dear ImGui Core & Backend Source Inventory
# ============================================================================
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

# ============================================================================
# 2. Target Library Definition
# ============================================================================
# Compiles vendor user-interface widgets together with Genesis wrapper layer wrappers
add_library(GenesisEditor STATIC
        ${IMGUI_SOURCES}
        src/GenesisEditor.cpp
)

# ============================================================================
# 3. Header Include Search Paths Configuration
# ============================================================================
# Exposes vendor headers publicly to ensure viewport widgets remain unclipped
target_include_directories(GenesisEditor PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${IMGUI_DIR}/backends
)

# ============================================================================
# 4. Direct Interface Dependency Linking Phase
# ============================================================================
# Inherits window system backends, math utilities, and Vulkan context handles
target_link_libraries(GenesisEditor PUBLIC
        Vulkan::Vulkan
        GpuSystem
)

# ============================================================================
# 5. Global Engine Registration Pass
# ============================================================================
# Appends this module up into the core build compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GenesisEditor)