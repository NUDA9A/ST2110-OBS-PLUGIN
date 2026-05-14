cmake_minimum_required(VERSION 3.20)
project(st2110_obs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ST2110_BUILD_OBS_PLUGIN "Build the OBS plugin target" OFF)

add_subdirectory(libs/st2110core)
add_subdirectory(apps/st2110_rx_dump)

if(ST2110_BUILD_OBS_PLUGIN)
    add_subdirectory(plugins/obs_st2110)
endif()

enable_testing()
add_subdirectory(tests)