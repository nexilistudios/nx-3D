cmake_minimum_required(VERSION 3.30)
project(nx_3D_client)

set(CMAKE_EXPORT_COMPILE_COMMANDS)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../spear/engine spear_engine)
add_executable(nx_3D_client main.cc)
target_link_libraries(nx_3D_client spear)

