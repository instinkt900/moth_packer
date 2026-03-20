cmake_minimum_required(VERSION 3.27)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" MOTH_PACKER_VERSION)
string(STRIP "${MOTH_PACKER_VERSION}" MOTH_PACKER_VERSION)

project(moth_packer VERSION ${MOTH_PACKER_VERSION} DESCRIPTION "Standalone texture packer CLI for moth_ui layouts")

message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCES
    src/main.cpp
    src/packer.cpp
    src/stb_impl.c
)

find_package(moth_ui REQUIRED)
find_package(CLI11 REQUIRED)
find_package(spdlog REQUIRED)

add_executable(${PROJECT_NAME} ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
target_include_directories(${PROJECT_NAME} PRIVATE src external/stb)
target_link_libraries(${PROJECT_NAME} moth_ui::moth_ui)
target_link_libraries(${PROJECT_NAME} CLI11::CLI11)
target_link_libraries(${PROJECT_NAME} spdlog::spdlog)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/W3 /WX")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Wall -Werror")
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
