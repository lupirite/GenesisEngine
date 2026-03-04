
# 1. Tell GLFW not to build extra junk
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# 2. Add the subdirectory (Relative to this CMakeLists.txt)
add_subdirectory(third_party/glfw)

add_library(GpuSystem STATIC
        src/GpuSystem.cpp
        third_party/VkBootstrap.cpp
)

target_link_libraries(GpuSystem PUBLIC
        Vulkan::Vulkan
        glfw
)

target_include_directories(GpuSystem PUBLIC
        include
        src # So GpuSystem.cpp can find VkBootstrap.h if it's in the same folder
        third_party
)

set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST GpuSystem)