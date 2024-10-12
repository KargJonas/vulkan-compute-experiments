cmake_minimum_required (VERSION 3.8)

project (VulkanTutorial)

find_package (glfw3 REQUIRED)
find_package (glm REQUIRED)
find_package (Vulkan REQUIRED)

find_package (PkgConfig)
# pkg_get_variable (STB_INCLUDEDIR stb includedir)
# if (NOT STB_INCLUDEDIR)
#   unset (STB_INCLUDEDIR)
#   find_path (STB_INCLUDEDIR stb_image.h PATH_SUFFIXES stb)
# endif ()
# if (NOT STB_INCLUDEDIR)
#   message (FATAL_ERROR "stb_image.h not found")
# endif ()

add_executable (glslang::validator IMPORTED)
find_program (GLSLANG_VALIDATOR "glslangValidator" HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
set_property (TARGET glslang::validator PROPERTY IMPORTED_LOCATION "${GLSLANG_VALIDATOR}")

function (add_shaders_target TARGET)
  cmake_parse_arguments ("SHADER" "" "CHAPTER_NAME" "SOURCES" ${ARGN})
  set (SHADERS_DIR ${SHADER_CHAPTER_NAME}/shaders)
  add_custom_command (
    OUTPUT ${SHADERS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
  set (SHADERS ${SHADERS_DIR}/frag.spv ${SHADERS_DIR}/vert.spv)
  # Some chapters may have compute shaders in addition to vertex and fragment shaders,
  # so we conditionally check this and add them to the target
  string(FIND "${SHADER_SOURCES}" "${CHAPTER_SHADER}.comp" COMPUTE_SHADER_INDEX)
  if (${COMPUTE_SHADER_INDEX} GREATER -1)  
    set (SHADERS ${SHADERS} ${SHADERS_DIR}/comp.spv)
  endif()
  add_custom_command (
    OUTPUT ${SHADERS}
    COMMAND glslang::validator
    ARGS --target-env vulkan1.0 ${SHADER_SOURCES} --quiet
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "Compiling Shaders"
    VERBATIM
    )
  add_custom_target (${TARGET} DEPENDS ${SHADERS})
endfunction ()

function (add_chapter CHAPTER_NAME)
  cmake_parse_arguments (CHAPTER "" "SHADER" "LIBS;TEXTURES;MODELS" ${ARGN})

  add_executable (${CHAPTER_NAME} ${CHAPTER_NAME}.cpp)
  set_target_properties (${CHAPTER_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CHAPTER_NAME})
  set_target_properties (${CHAPTER_NAME} PROPERTIES CXX_STANDARD 17)
  target_link_libraries (${CHAPTER_NAME} Vulkan::Vulkan glfw)
  # target_include_directories (${CHAPTER_NAME} PRIVATE ${STB_INCLUDEDIR})

  if (DEFINED CHAPTER_SHADER)
    set (CHAPTER_SHADER_TARGET ${CHAPTER_NAME}_shader)
    file (GLOB SHADER_SOURCES ${CHAPTER_SHADER}.frag ${CHAPTER_SHADER}.vert ${CHAPTER_SHADER}.comp)
    add_shaders_target (${CHAPTER_SHADER_TARGET} CHAPTER_NAME ${CHAPTER_NAME} SOURCES ${SHADER_SOURCES})
    add_dependencies (${CHAPTER_NAME} ${CHAPTER_SHADER_TARGET})
  endif ()
  if (DEFINED CHAPTER_LIBS)
    target_link_libraries (${CHAPTER_NAME} ${CHAPTER_LIBS})
  endif ()
  if (DEFINED CHAPTER_MODELS)
    file (COPY ${CHAPTER_MODELS} DESTINATION ${CMAKE_BINARY_DIR}/${CHAPTER_NAME}/models)
  endif ()
  if (DEFINED CHAPTER_TEXTURES)
    file (COPY ${CHAPTER_TEXTURES} DESTINATION ${CMAKE_BINARY_DIR}/${CHAPTER_NAME}/textures)
  endif ()
endfunction ()

add_chapter (31_compute_shader
  SHADER 31_shader_compute
  LIBS glm::glm)
