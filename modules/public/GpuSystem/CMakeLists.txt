project(GpuSystem LANGUAGES CXX)

# ============================================================================
# 1. Vendor Dependency Configuration (GLFW Optimization Flags)
# ============================================================================
# Suppress redundant vendor targets to maintain clean, fast build-pacing
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Incorporate raw window system platform layers
add_subdirectory(third_party/glfw)

# ============================================================================
# 2. Target Library Definition
# ============================================================================
# Bundles context initialization, bootstrap helpers, and window creation together
add_library(GpuSystem STATIC
        src/GpuSystem.cpp
        third_party/VkBootstrap.cpp
)

# ============================================================================
# 3. Header Include Search Paths Configuration
# ============================================================================
target_include_directories(GpuSystem PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# 4. Direct Interface Dependency Linking Phase
# ============================================================================
target_link_libraries(GpuSystem PUBLIC
        Vulkan::Vulkan
        glfw
)

# ============================================================================
# 5. Global Engine Registration Pass
# ============================================================================
# Appends this hardware foundation layer up into the compilation pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GpuSystem)