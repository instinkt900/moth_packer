cmake_minimum_required(VERSION 3.15)

project(moth_packer_tests VERSION 1.0 DESCRIPTION "Unit tests for moth_packer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Catch2 3 REQUIRED)
find_package(spdlog REQUIRED)

set(MOTH_PACKER_BUILD_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(.. moth_packer)

set(SOURCES
    src/test_packer.cpp
    src/test_collect.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/stb
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    moth_packer
    spdlog::spdlog
    Catch2::Catch2WithMain
)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /WX)
    target_link_options(${PROJECT_NAME} PRIVATE /ignore:4099)
endif()

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})
