cmake_minimum_required(VERSION 3.10)
project(ComputeShaderDemo)

set(CMAKE_CXX_STANDARD 14)

find_package(Vulkan REQUIRED)

add_executable(ComputeShaderDemo src/main.cpp)

target_link_libraries(ComputeShaderDemo Vulkan::Vulkan)

## GLSL -> SPIR-V:

find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

function(add_shader TARGET SHADER)
    set(SPIRV "${CMAKE_BINARY_DIR}/shaders/${SHADER}.spv")
    set(GLSL "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/${SHADER}.comp")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSLANG_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL}
        IMPLICIT_DEPENDS CXX ${GLSL}
        COMMENT "Compiling ${GLSL} to SPIR-V"
        VERBATIM
    )
    set_source_files_properties(${SPIRV} PROPERTIES GENERATED TRUE)
    add_custom_target(${SHADER} DEPENDS ${SPIRV})
    add_dependencies(${TARGET} ${SHADER})
    source_group("Shaders" FILES ${GLSL})
endfunction()

# Ensure the shaders directory exists in the binary directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders)

# Call the function to compile shaders
add_shader(ComputeShaderDemo "add")
