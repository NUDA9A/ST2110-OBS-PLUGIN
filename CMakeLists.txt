cmake_minimum_required(VERSION 3.20)
project(st2110_obs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ST2110_BUILD_OBS_PLUGIN "Build the OBS plugin target" OFF)
option(ST2110_BUILD_MTL_SEND_TEST_APP "Build the ST 2110 MTL send test app" OFF)
option(ST2110_BUILD_MTL_RX_WORKER "Build the ST 2110 MTL receive worker" OFF)

set(ST2110_OBS_PLUGIN_INSTALL_DIR "" CACHE PATH
        "OBS plugin package directory, for example ~/.config/obs-studio/plugins/st2110_obs")

set(ST2110_MTL_RX_WORKER_INSTALL_DIR "" CACHE PATH
        "Directory where st2110_mtl_rx_worker is installed")

if(ST2110_BUILD_OBS_PLUGIN AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ST2110_BUILD_MTL_RX_WORKER ON CACHE BOOL
            "Build the ST 2110 MTL receive worker" FORCE)
endif()

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

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET st2110_obs AND TARGET st2110_mtl_rx_worker)
        add_dependencies(st2110_obs st2110_mtl_rx_worker)
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()