cmake_minimum_required(VERSION 3.27)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" MOTH_PACKER_VERSION)
string(STRIP "${MOTH_PACKER_VERSION}" MOTH_PACKER_VERSION)

project(moth_packer VERSION ${MOTH_PACKER_VERSION} DESCRIPTION "Texture packer library and CLI for moth_ui layouts")

message("Building with CMake version: ${CMAKE_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MOTH_PACKER_BUILD_CLI "Build the moth_packer command-line tool" ON)

find_package(moth_ui REQUIRED)
find_package(spdlog REQUIRED)
if(MOTH_PACKER_BUILD_CLI)
    find_package(CLI11 REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(moth_packer STATIC src/packer.cpp src/stb_impl.c)
set_target_properties(moth_packer PROPERTIES VERSION ${PROJECT_VERSION})
target_include_directories(moth_packer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        src
        external/stb
)
target_link_libraries(moth_packer
    PUBLIC  moth_ui::moth_ui
    PRIVATE spdlog::spdlog
)

if(MSVC)
    target_compile_definitions(moth_packer PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(moth_packer PRIVATE /W3 /WX)
else()
    target_compile_options(moth_packer PRIVATE -Wall -Werror)
endif()

install(TARGETS moth_packer
    EXPORT moth_packerTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(EXPORT moth_packerTargets
    FILE moth_packerTargets.cmake
    NAMESPACE moth_packer::
    DESTINATION lib/cmake/moth_packer
)
install(DIRECTORY include/ DESTINATION include)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------

if(MOTH_PACKER_BUILD_CLI)
    add_executable(moth_packer_cli src/main.cpp)
    set_target_properties(moth_packer_cli PROPERTIES OUTPUT_NAME moth_packer)
    target_compile_definitions(moth_packer_cli PRIVATE MOTH_PACKER_VERSION="${MOTH_PACKER_VERSION}")
    target_link_libraries(moth_packer_cli PRIVATE moth_packer CLI11::CLI11 spdlog::spdlog)

    if(MSVC)
        target_compile_definitions(moth_packer_cli PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(moth_packer_cli PRIVATE /W3 /WX)
    else()
        target_compile_options(moth_packer_cli PRIVATE -Wall -Werror)
    endif()

    install(TARGETS moth_packer_cli RUNTIME DESTINATION bin)
endif()
