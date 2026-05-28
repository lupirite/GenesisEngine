project(SceneRenderer LANGUAGES CXX)

# ============================================================================
# 1. Target Library Definition
# ============================================================================
# Houses isolated offscreen pipeline memory management, command tracking,
# and polymorphic frame snapshot transaction payload contracts
add_library(SceneRenderer STATIC
        src/SceneRenderer.cpp
        include/SceneRenderer.hpp
        include/RenderPayload.hpp
)

# ============================================================================
# 2. Header Include Search Paths Configuration
# ============================================================================
# Exposes internal frame targets and vendor pipeline layouts downwards
target_include_directories(SceneRenderer PUBLIC
        include
        src
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui
        ${CMAKE_SOURCE_DIR}/modules/public/GenesisEditor/third_party/imgui/backends
)

# ============================================================================
# 3. Direct Interface Dependency Linking Phase
# ============================================================================
# Pulls in the core Vulkan API layer, parent hardware context, and ImGui hooks
target_link_libraries(SceneRenderer PUBLIC
        Vulkan::Vulkan
        GpuSystem
        GenesisEditor
)

# ============================================================================
# 4. Asynchronous Automated SPIR-V Shader Compilation Pipeline
# ============================================================================
# Locates the glslangValidator utility provided within the active Vulkan SDK
find_program(GLSL_VALIDATOR glslangValidator)

set(GENESIS_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

# Custom macro baking raw GLSL code text into platform-portable binary spv assets
function(compile_shader SHADER_PATH)
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME)
    set(SPIRV_OUTPUT "${GENESIS_SHADER_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_OUTPUT}
            COMMAND ${GLSL_VALIDATOR} -V ${SHADER_PATH} -o ${SPIRV_OUTPUT}
            DEPENDS ${SHADER_PATH}
            COMMENT "Genesis Asset Pipeline: Compiling shader source -> ${SHADER_NAME}.spv"
    )

    # Force the parent target library compilation to hold until bytecode output stabilizes
    target_sources(SceneRenderer PRIVATE ${SPIRV_OUTPUT})
endfunction()

# Expose the relative asset folder destination to the C++ preprocessor string map
target_compile_definitions(SceneRenderer PRIVATE
        SHADER_DIR="${GENESIS_SHADER_DIR}/"
)

# Dispatch active engine shaders into the baking pipeline
compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/gradient.vert")
compile_shader("${CMAKE_CURRENT_SOURCE_DIR}/shaders/gradient.frag")

# ============================================================================
# 5. Global Engine Registration Pass
# ============================================================================
# Appends this asset and rendering module up into the core build pipeline
set_property(GLOBAL APPEND PROPERTY GENESIS_MODULE_LIST SceneRenderer)