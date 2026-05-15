find_package(PkgConfig REQUIRED)

if(NOT TARGET PkgConfig::ST2110_MTL)
    pkg_check_modules(ST2110_MTL REQUIRED IMPORTED_TARGET mtl)
endif()

add_executable(st2110_mtl_rx_worker
        main.cpp
        mtl_runtime_context.cpp
        mtl_video_rx_session.cpp
        mtl_audio_rx_session.cpp
        mtl_receive_graph.cpp
        mtl_worker_process_state.cpp
)

target_link_libraries(st2110_mtl_rx_worker PRIVATE
        st2110core
        PkgConfig::ST2110_MTL
)

if(UNIX AND NOT APPLE)
    target_link_libraries(st2110_mtl_rx_worker PRIVATE
            dl
    )
endif()

target_compile_features(st2110_mtl_rx_worker PRIVATE cxx_std_23)