include_guard(GLOBAL)

# Compile the dependency and its consumers from the same checkout. Importing
# an old .so alongside newer public headers corrupts ClientAPI's event queues.
option(NX3D_USE_SYSTEM_NEXILIS "Use an externally built Nexilis package" OFF)
if(NX3D_USE_SYSTEM_NEXILIS)
    find_package(Nexilis REQUIRED CONFIG)
else()
    set(_nx3d_nexilis_source "${CMAKE_CURRENT_LIST_DIR}/../nexilis/nexilis")
    if(NOT EXISTS "${_nx3d_nexilis_source}/CMakeLists.txt")
        message(FATAL_ERROR "Initialize the Nexilis submodule or set NX3D_USE_SYSTEM_NEXILIS=ON and Nexilis_DIR.")
    endif()
    add_subdirectory("${_nx3d_nexilis_source}" "${CMAKE_BINARY_DIR}/nexilis" EXCLUDE_FROM_ALL)
    add_library(Nexilis::nexilis ALIAS nexilis)
endif()
