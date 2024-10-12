cmake_minimum_required(VERSION 3.10)
project(ComputeShaderDemo)

set(CMAKE_CXX_STANDARD 14)

find_package(Vulkan REQUIRED)

add_executable(ComputeShaderDemo src/main.cpp)

target_link_libraries(ComputeShaderDemo Vulkan::Vulkan)
