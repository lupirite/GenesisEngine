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
        include
        src
        # Paths to find ImGui headers (adjust if your structure differs)
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui/backends
)

# 3. Link Dependencies
target_link_libraries(SceneRenderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
)

# Find the glslangValidator (comes with Vulkan SDK)
find_program(GLSL_VALIDATOR glslangValidator)

set(GENESIS_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

# Function to compile shaders automatically
function(compile_shader SHADER_PATH)
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME)
    set(SPIRV_OUTPUT "${GENESIS_SHADER_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSL_VALIDATOR} -V ${SHADER_PATH} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER_PATH}
            COMMENT "Compiling shader ${SHADER_NAME}"
    )
    # Ensure the library depends on these outputs
    target_sources(SceneRenderer PRIVATE ${SPIRV_OUTPUT})
endfunction()

target_compile_definitions(SceneRenderer PRIVATE
        SHADER_DIR="${GENESIS_SHADER_DIR}/"
)

# Compile your test shaders
compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/gradient.vert")
compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/gradient.frag")

set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST SceneRenderer)