cmake_minimum_required(VERSION 3.20)
project(st2110_obs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ST2110_BUILD_OBS_PLUGIN "Build the OBS plugin target" OFF)
option(ST2110_BUILD_MTL_SEND_TEST_APP "Build the ST 2110 MTL send test app" OFF)
option(ST2110_BUILD_MTL_RX_WORKER "Build the ST 2110 MTL receive worker" OFF)

add_subdirectory(libs/st2110core)
add_subdirectory(apps/st2110_rx_dump)

if(ST2110_BUILD_MTL_SEND_TEST_APP)
    add_subdirectory(apps/st2110_mtl_send_test)
endif()

if(ST2110_BUILD_MTL_RX_WORKER)
    add_subdirectory(apps/st2110_mtl_rx_worker)
endif()

if(ST2110_BUILD_OBS_PLUGIN)
    add_subdirectory(plugins/obs_st2110)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()