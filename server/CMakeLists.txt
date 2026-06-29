cmake_minimum_required(VERSION 3.30)
project(nx-3D-server)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/../nexilis/nexilis/build")
find_package(Nexilis REQUIRED CONFIG)

add_executable(nx-3D-server server.cc gun/gun_manager.cc)
target_include_directories(nx-3D-server PRIVATE "${CMAKE_SOURCE_DIR}/..")
target_link_libraries(nx-3D-server PRIVATE Nexilis::nexilis)
