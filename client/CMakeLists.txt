cmake_minimum_required(VERSION 3.30)
project(nx_3D_client)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fallback package location when NX3D_USE_SYSTEM_NEXILIS is explicitly enabled.
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../nexilis/nexilis/build/")

include(../cmake/Nexilis.cmake)

add_subdirectory(../spear/engine spear_engine)

get_target_property(_spear_links spear INTERFACE_LINK_LIBRARIES)

# nixpkgs advertises shaderc_combined through pkg-config but only ships
# shaderc_shared. Ubuntu ships shaderc_combined, so use it when available.
find_library(NX3D_SHADERC_COMBINED_LIBRARY NAMES shaderc_combined)
if(NOT NX3D_SHADERC_COMBINED_LIBRARY)
  find_library(NX3D_SHADERC_SHARED_LIBRARY NAMES shaderc_shared)
  if(NOT NX3D_SHADERC_SHARED_LIBRARY)
    message(FATAL_ERROR "Neither shaderc_combined nor shaderc_shared was found")
  endif()
  if(_spear_links)
    string(REPLACE "shaderc_combined" "shaderc_shared" _spear_links "${_spear_links}")
  endif()
endif()

if(UNIX AND NOT APPLE AND NX3D_SHADERC_COMBINED_LIBRARY AND _spear_links)
  # Ubuntu ships these as static archives. SPIRV-Tools-opt depends on symbols
  # in SPIRV-Tools, so the core archive must follow the optimizer archive.
  string(REPLACE
    "$<LINK_ONLY:SPIRV-Tools>;$<LINK_ONLY:SPIRV-Tools-opt>"
    "$<LINK_ONLY:SPIRV-Tools-opt>;$<LINK_ONLY:SPIRV-Tools>"
    _spear_links "${_spear_links}")
endif()
if(_spear_links)
  set_target_properties(spear PROPERTIES INTERFACE_LINK_LIBRARIES "${_spear_links}")
endif()

file(GLOB_RECURSE NX3D_CLIENT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
list(FILTER NX3D_CLIENT_SOURCES EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/build/")

add_executable(nx_3D_client ${NX3D_CLIENT_SOURCES})
# Packages that install the assets elsewhere point the binary at them here.
set(NX3D_DATA_DIR "" CACHE STRING "Runtime directory containing assets/ and spear/ (default: the source tree)")
if(NX3D_PORTABLE_CLIENT)
  # The bundled client runs from its archive directory.
  set(NX3D_DATA_DIR ".")
endif()
if(NX3D_DATA_DIR)
  set(PROJECT_ROOT "${NX3D_DATA_DIR}")
  # Spear assets live in spear/ next to the game assets.
  foreach(_property COMPILE_DEFINITIONS INTERFACE_COMPILE_DEFINITIONS)
    get_target_property(_definitions spear ${_property})
    if(_definitions)
      list(FILTER _definitions EXCLUDE REGEX "^SPEAR_ROOT=")
      list(APPEND _definitions "SPEAR_ROOT=\"${NX3D_DATA_DIR}/spear\"")
      set_target_properties(spear PROPERTIES ${_property} "${_definitions}")
    endif()
  endforeach()
else()
  set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()
target_include_directories(nx_3D_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(nx_3D_client PRIVATE PROJECT_ROOT="${PROJECT_ROOT}")
target_link_libraries(nx_3D_client PRIVATE Nexilis::nexilis spear)
if(WIN32)
  target_link_libraries(nx_3D_client PRIVATE ws2_32)
endif()
