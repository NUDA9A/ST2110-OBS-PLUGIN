cmake_minimum_required(VERSION 3.20)
project(st2110_obs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/st2110core)
add_subdirectory(apps/st2110_rx_dump)

enable_testing()
add_subdirectory(tests)