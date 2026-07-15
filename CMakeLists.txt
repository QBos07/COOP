cmake_minimum_required(VERSION 3.25)
project(COOP VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(COOP_ENABLE_SANITIZERS "Enable address/undefined sanitizers" ON)

add_library(coop src/coop.c)
add_library(COOP::coop ALIAS coop)

target_include_directories(coop
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(coop PRIVATE -Wall -Wextra -pedantic -pedantic-errors)

if(COOP_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(coop PRIVATE -fsanitize=address,undefined)
    target_link_options(coop PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()

find_package(cmocka CONFIG QUIET)
if(cmocka_FOUND)
    set(COOP_CMOCKA_TARGET cmocka::cmocka)
else()
    find_package(cmocka QUIET)
    if(cmocka_FOUND)
        set(COOP_CMOCKA_TARGET cmocka)
    else()
        find_library(COOP_CMOCKA_LIB cmocka REQUIRED)
        add_library(COOP::cmocka UNKNOWN IMPORTED)
        set_target_properties(COOP::cmocka PROPERTIES IMPORTED_LOCATION "${COOP_CMOCKA_LIB}")
        set(COOP_CMOCKA_TARGET COOP::cmocka)
    endif()
endif()

add_executable(coop_tests tests/test_coop.c)
target_link_libraries(coop_tests PRIVATE COOP::coop ${COOP_CMOCKA_TARGET})
target_compile_options(coop_tests PRIVATE -Wall -Wextra -pedantic -pedantic-errors)
if(COOP_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(coop_tests PRIVATE -fsanitize=address,undefined)
    target_link_options(coop_tests PRIVATE -fsanitize=address,undefined)
endif()

add_test(NAME coop_runtime_tests COMMAND coop_tests)
