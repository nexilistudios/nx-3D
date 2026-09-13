cmake_minimum_required(VERSION 3.30)
project(nx_3D_client)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Submodule build path, build submodule first.
set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../nexilis/nexilis/build/")

find_package(Nexilis REQUIRED CONFIG)

add_subdirectory(../spear/engine spear_engine)

file(GLOB_RECURSE NX3D_CLIENT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(FILTER NX3D_CLIENT_SOURCES EXCLUDE REGEX "/build/")

add_executable(nx_3D_client ${NX3D_CLIENT_SOURCES})
set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(nx_3D_client PRIVATE "${CMAKE_SOURCE_DIR}/..")
target_compile_definitions(nx_3D_client PRIVATE PROJECT_ROOT="${PROJECT_ROOT}")
target_link_libraries(nx_3D_client Nexilis::nexilis spear)

