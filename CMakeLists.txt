cmake_minimum_required(VERSION 3.28)

project(MidnightRanger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(fetch_sdl)
include(fetch_sdl_image)
include(fetch_sdl_shadercross)

add_executable(game src/main.cpp src/pipelines.cpp src/draw.cpp)
target_link_libraries(game PUBLIC SDL3::SDL3)
target_link_libraries(game PUBLIC SDL3_image::SDL3_image)
target_link_libraries(game PUBLIC SDL3_shadercross::SDL3_shadercross)
target_include_directories(game PRIVATE include)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Content $<TARGET_FILE_DIR:game>/Content
)
