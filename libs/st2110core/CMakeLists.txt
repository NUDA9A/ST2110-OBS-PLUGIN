if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ST2110_HAS_MTL_BACKEND ON)
else()
    set(ST2110_HAS_MTL_BACKEND OFF)
endif()

set(ST2110CORE_SOURCES
        src/stub.cpp
        src/backends/receive_local_policy.cpp
        src/backends/socket/socket_rx_single_media_backend_base.cpp
)

if(ST2110_HAS_MTL_BACKEND)
    list(APPEND ST2110CORE_SOURCES
            src/backends/mtl/mtl_rx_video_backend_proxy.cpp
            src/backends/mtl/mtl_rx_audio_backend_proxy.cpp
            src/backends/mtl/mtl_worker_control_channel.cpp
            src/backends/mtl/mtl_worker_process_control_channel.cpp
            src/backends/mtl/mtl_worker_ipc_framing.cpp
            src/backends/mtl/mtl_worker_ipc_codec.cpp
            src/backends/mtl/mtl_worker_manager.cpp
            src/backends/mtl/mtl_worker_graph_client.cpp
    )
endif()

option(ST2110_MTL_DEV_KERNEL_SOCKET
        "Use MTL kernel-socket PMD for local VM/dev testing"
        OFF)

if(ST2110_MTL_DEV_KERNEL_SOCKET AND NOT ST2110_HAS_MTL_BACKEND)
    message(FATAL_ERROR "ST2110_MTL_DEV_KERNEL_SOCKET requires ST2110_HAS_MTL_BACKEND")
endif()

add_library(st2110core STATIC
        ${ST2110CORE_SOURCES}
)

set_target_properties(st2110core PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(st2110core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(st2110core PUBLIC cxx_std_23)

target_compile_definitions(st2110core PUBLIC
        ST2110_HAS_MTL_BACKEND=$<BOOL:${ST2110_HAS_MTL_BACKEND}>
        ST2110_MTL_DEV_KERNEL_SOCKET=$<BOOL:${ST2110_MTL_DEV_KERNEL_SOCKET}>
)