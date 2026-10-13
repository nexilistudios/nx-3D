cmake_minimum_required(VERSION 3.30)
project(nx3d_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../nexilis/nexilis/build")
include(../cmake/Nexilis.cmake)
find_package(Boost REQUIRED COMPONENTS json)
enable_testing()

add_executable(deathmatch_test deathmatch_test.cc
    ../server/gamemode/deathmatch_mode.cc
    ../server/gamemode/bomb_mode.cc
    ../server/gamemode/game_mode_registry.cc)
target_include_directories(deathmatch_test PRIVATE ..)
target_link_libraries(deathmatch_test PRIVATE Nexilis::nexilis Boost::json)
add_test(NAME deathmatch COMMAND deathmatch_test)

add_executable(bomb_mode_test bomb_mode_test.cc
    ../server/gamemode/bomb_mode.cc
    ../server/gamemode/deathmatch_mode.cc
    ../server/gamemode/game_mode_registry.cc)
target_include_directories(bomb_mode_test PRIVATE ..)
target_link_libraries(bomb_mode_test PRIVATE Nexilis::nexilis Boost::json)
add_test(NAME bomb_mode COMMAND bomb_mode_test)

add_executable(bomb_player_state_test bomb_player_state_test.cc)
target_include_directories(bomb_player_state_test PRIVATE ..)
add_test(NAME bomb_player_state COMMAND bomb_player_state_test)

add_executable(economy_test economy_test.cc)
target_include_directories(economy_test PRIVATE ..)
add_test(NAME economy COMMAND economy_test)

add_executable(ammunition_test ammunition_test.cc)
target_include_directories(ammunition_test PRIVATE ..)
add_test(NAME ammunition COMMAND ammunition_test)

find_package(glm REQUIRED)
add_executable(weapon_motion_test weapon_motion_test.cc)
target_include_directories(weapon_motion_test PRIVATE ..)
target_link_libraries(weapon_motion_test PRIVATE glm::glm)
add_test(NAME weapon_motion COMMAND weapon_motion_test)

add_executable(player_hit_test player_hit_test.cc)
target_include_directories(player_hit_test PRIVATE ..)
target_link_libraries(player_hit_test PRIVATE glm::glm)
add_test(NAME player_hit COMMAND player_hit_test)

add_executable(network_ports_test network_ports_test.cc)
target_include_directories(network_ports_test PRIVATE ..)
add_test(NAME network_ports COMMAND network_ports_test)

if(TARGET spear)
    add_executable(movement_controller_test movement_controller_test.cc)
    target_compile_definitions(movement_controller_test PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(movement_controller_test PRIVATE spear)
    add_test(NAME movement_controller COMMAND movement_controller_test)

    add_executable(player_model_test player_model_test.cc)
    target_compile_definitions(player_model_test PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(player_model_test PRIVATE spear)
    add_test(NAME player_model COMMAND player_model_test)

    add_executable(weapon_model_test weapon_model_test.cc)
    target_compile_definitions(weapon_model_test PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
    target_link_libraries(weapon_model_test PRIVATE spear)
    add_test(NAME weapon_model COMMAND weapon_model_test)
endif()
