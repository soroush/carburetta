cmake_minimum_required(VERSION 3.20)

project(calc LANGUAGES C)

add_custom_command(
    OUTPUT  calc.c
    COMMAND carburetta ${CMAKE_CURRENT_SOURCE_DIR}/calc.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/calc.c --h
    DEPENDS carburetta calc.cbrt
)

add_executable(calc ${CMAKE_CURRENT_BINARY_DIR}/calc.c)

set_target_properties(calc PROPERTIES
    CXX_STANDARD 14
    DEBUG_POSTFIX "_d"
)