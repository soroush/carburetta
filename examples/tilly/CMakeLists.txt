cmake_minimum_required(VERSION 3.20)

project(tilly LANGUAGES CXX)

foreach(F IN ITEMS directive_input_splitter directives line_continuation multi_line_comments snippets tiles)
add_custom_command(
    OUTPUT  ${F}.cpp
    COMMAND carburetta --x-utf8 ${CMAKE_CURRENT_SOURCE_DIR}/${F}.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/${F}.cpp --h
    DEPENDS carburetta ${F}.cbrt
)
endforeach()

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_BINARY_DIR}/directive_input_splitter.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/directives.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/line_continuation.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/multi_line_comments.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/snippets.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/tiles.cpp
    directive_input_splitter_defs.h
    directives_defs.h
    line_continuation_defs.h
    multi_line_comments_defs.h
    situs.h
    situs.cpp
    tiles_defs.h
    tilly_parser.cpp
    tilly_parser.h
    tilly.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14
    DEBUG_POSTFIX "_d"
)