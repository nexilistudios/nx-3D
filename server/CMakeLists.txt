cmake_minimum_required(VERSION 3.30)
project(nx-3D-server)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../nexilis/nexilis/build")
include(../cmake/Nexilis.cmake)

if (NOT NEXILIS_USE_SUBMODULES)
    find_package(Boost REQUIRED COMPONENTS json)
endif()

# Collect all sources by globbing recursively so new files are picked up
# automatically. CONFIGURE_DEPENDS makes CMake re-run the configure step
# whenever the globbed files change, so added/removed sources do not need a
# manual reconfigure. The build dir lives inside the source tree, so anything
# under a build/ folder is excluded.
file(GLOB_RECURSE NX3D_SERVER_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(FILTER NX3D_SERVER_SOURCES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/build/")

add_executable(nx-3D-server ${NX3D_SERVER_SOURCES})
target_include_directories(nx-3D-server PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
# Packages that install the assets elsewhere point the binary at them here.
set(NX3D_DATA_DIR "" CACHE STRING "Runtime directory containing assets/ (default: the source tree)")
if(NX3D_DATA_DIR)
    set(PROJECT_ROOT "${NX3D_DATA_DIR}")
else()
    set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
target_compile_definitions(nx-3D-server PRIVATE PROJECT_ROOT="${PROJECT_ROOT}")
find_package(Bullet REQUIRED)
find_package(glm REQUIRED)
target_include_directories(nx-3D-server PRIVATE ${BULLET_INCLUDE_DIRS})
target_link_libraries(nx-3D-server PRIVATE Nexilis::nexilis ${BULLET_LIBRARIES} glm::glm)

# The server uses boost::json directly, so link it explicitly instead of
# relying on Nexilis::nexilis. An installed Nexilis built with vendored Boost
# only exposes boost_json's headers; the library itself is a transitive
# dependency of libnexilis, which the linker rejects ("DSO missing from
# command line").
if(NEXILIS_USE_SUBMODULES)
    if(NX3D_USE_SYSTEM_NEXILIS)
        find_library(NX3D_BOOST_JSON_LIBRARY boost_json REQUIRED
                HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib lib64)
        target_link_libraries(nx-3D-server PRIVATE "${NX3D_BOOST_JSON_LIBRARY}")
    endif()
else()
    target_link_libraries(nx-3D-server PRIVATE Boost::json)
endif()

if(WIN32)
    target_link_libraries(nx-3D-server PRIVATE ws2_32 mswsock)
endif()
