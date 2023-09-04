cmake_minimum_required(VERSION 3.20)

project(template_scan LANGUAGES C)

add_custom_command(
    OUTPUT  template_scan.c
    COMMAND carburetta ${CMAKE_CURRENT_SOURCE_DIR}/template_scan.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/template_scan.c --h
    DEPENDS carburetta template_scan.cbrt
)

add_executable(template_scan ${CMAKE_CURRENT_BINARY_DIR}/template_scan.c)

set_target_properties(template_scan PROPERTIES
    CXX_STANDARD 14
    DEBUG_POSTFIX "_d"
)