add_library(st2110core STATIC
        src/stub.cpp
        src/socket_rx_single_media_backend_base.cpp
)

target_include_directories(st2110core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(st2110core PUBLIC cxx_std_23)