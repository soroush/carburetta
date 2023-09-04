cmake_minimum_required(VERSION 3.20)

project(carburetta 
    VERSION 0.8.17
    LANGUAGES C
)

include(CTest)

# Options
option(BUILD_EXAMPLES   "Build and install example programs"    ON)
option(BUILD_TESTING    "Build and run unit tests"              ON)

set(TARGET_EXECUTABLE ${PROJECT_NAME})

add_executable(${TARGET_EXECUTABLE}
    src/carburetta_context.c
    src/carburetta_context.h
    src/carburetta.c
    src/chain.c
    src/chain.h
    src/decomment.c
    src/decomment.h
    src/dfa.c
    src/dfa.h
    src/emit_c.c
    src/emit_c.h
    src/grammar_table.c
    src/grammar_table.h
    src/indented_printer.c
    src/indented_printer.h
    src/lalr.c
    src/lalr.h
    src/line_assembly.c
    src/line_assembly.h
    src/line_defs.c
    src/line_defs.h
    src/mode.c
    src/mode.h
    src/mul.c
    src/mul.h
    src/nfa.c
    src/nfa.h
    src/parse_input.c
    src/parse_input.h
    src/prd_gram.c
    src/prd_gram.h
    src/prd_grammar.c
    src/prd_grammar.h
    src/regex_grammar.c
    src/regex_grammar.h
    src/report_error.c
    src/report_error.h
    src/rex_parse.c
    src/rex_parse.h
    src/rex_set_range.c
    src/rex_set_range.h
    src/rex.c
    src/rex.h
    src/scanner.c
    src/scanner.h
    src/snippet.c
    src/snippet.h
    src/symbol.c
    src/symbol.h
    src/temp_output.c
    src/temp_output.h
    src/tokenizer.c
    src/tokenizer.h
    src/tokens.c
    src/tokens.h
    src/typestr.c
    src/typestr.h
    src/uc_cat_ranges.c
    src/uc_cat_ranges.h
    src/version.h
    src/xlalr.c
    src/xlalr.h
    src/xlts.c
    src/xlts.h
)

# Prepate the target
set_target_properties(${TARGET_EXECUTABLE} PROPERTIES
    VERSION 0.8.17
    CXX_STANDARD 14
    DEBUG_POSTFIX "_d"
)

target_include_directories(${TARGET_EXECUTABLE}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

# Installation
include(GNUInstallDirs)

install(TARGETS ${TARGET_EXECUTABLE}
    EXPORT "${TARGET_EXECUTABLE}Targets"
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(EXPORT "${TARGET_EXECUTABLE}Targets"
    FILE "${TARGET_EXECUTABLE}Targets.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_EXECUTABLE}
)

# Config and Versioning

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_EXECUTABLE}
)

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_EXECUTABLE}ConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${TARGET_EXECUTABLE}
)

# Example projects
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTING)
    add_subdirectory(tester)
endif()