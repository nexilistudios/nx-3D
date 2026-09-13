cmake_minimum_required(VERSION 3.30)
project(nx-3D-server)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../nexilis/nexilis/build")
find_package(Nexilis REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS json)

# Collect all sources by globbing recursively so new files are picked up
# automatically. CONFIGURE_DEPENDS makes CMake re-run the configure step
# whenever the globbed files change, so added/removed sources do not need a
# manual reconfigure. The build dir lives inside the source tree, so anything
# under a build/ folder is excluded.
file(GLOB_RECURSE NX3D_SERVER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(FILTER NX3D_SERVER_SOURCES EXCLUDE REGEX "/build/")

add_executable(nx-3D-server ${NX3D_SERVER_SOURCES})
target_include_directories(nx-3D-server PRIVATE "${CMAKE_SOURCE_DIR}/..")
target_link_libraries(nx-3D-server PRIVATE Nexilis::nexilis Boost::json)
