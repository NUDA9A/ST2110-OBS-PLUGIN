add_executable(st2110_rx_dump
        main.cpp
)

target_link_libraries(st2110_rx_dump PRIVATE st2110core)
target_compile_features(st2110_rx_dump PRIVATE cxx_std_23)