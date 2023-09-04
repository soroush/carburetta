cmake_minimum_required(VERSION 3.20)

project(inireader LANGUAGES C)

add_custom_command(
    OUTPUT  iniparser.c
    COMMAND carburetta ${CMAKE_CURRENT_SOURCE_DIR}/iniparser.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/iniparser.c --h
    DEPENDS carburetta iniparser.cbrt
)

add_executable(inireader 
    ${CMAKE_CURRENT_BINARY_DIR}/iniparser.c
    ini_reader.h
    main.c
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