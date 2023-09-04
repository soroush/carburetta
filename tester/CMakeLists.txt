cmake_minimum_required(VERSION 3.20)

project(tester LANGUAGES C CXX)

foreach(FILE IN ITEMS t1 t2 t4 t5 t6 t7)
add_custom_command(
    OUTPUT  ${FILE}.c
    COMMAND carburetta --x-utf8 ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/${FILE}.c --h
    DEPENDS carburetta ${FILE}.cbrt
)
endforeach()

# Exclude --x-utf8 from Test Case #3
foreach(FILE IN ITEMS t3)
add_custom_command(
    OUTPUT  ${FILE}.c
    COMMAND carburetta ${CMAKE_CURRENT_SOURCE_DIR}/${FILE}.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/${FILE}.c --h
    DEPENDS carburetta ${FILE}.cbrt
)
endforeach()

foreach(FILE IN ITEMS t8 t9 t10 t11 t12 t13 t14 t15)
add_custom_command(
    OUTPUT  ${FILE}.cpp
    COMMAND carburetta --x-utf8 ${CMAKE_CURRENT_SOURCE_DIR}/cpp/${FILE}.cbrt --c ${CMAKE_CURRENT_BINARY_DIR}/${FILE}.cpp --h
    DEPENDS carburetta cpp/${FILE}.cbrt
)
endforeach()

add_executable(tester
    ${CMAKE_CURRENT_BINARY_DIR}/t1.c
    ${CMAKE_CURRENT_BINARY_DIR}/t2.c
    ${CMAKE_CURRENT_BINARY_DIR}/t3.c
    ${CMAKE_CURRENT_BINARY_DIR}/t4.c
    ${CMAKE_CURRENT_BINARY_DIR}/t5.c
    ${CMAKE_CURRENT_BINARY_DIR}/t6.c
    ${CMAKE_CURRENT_BINARY_DIR}/t7.c
    ${CMAKE_CURRENT_BINARY_DIR}/t8.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t9.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t10.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t11.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t12.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t13.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t14.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/t15.cpp
    tester.c
)

target_include_directories(tester
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

set_target_properties(tester PROPERTIES
    CXX_STANDARD 14
    DEBUG_POSTFIX "_d"
)

# The test
add_test(NAME carburetta_test COMMAND tester)