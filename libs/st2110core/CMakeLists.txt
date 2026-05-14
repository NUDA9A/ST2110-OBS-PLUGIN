if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ST2110_HAS_MTL_BACKEND ON)
else()
    set(ST2110_HAS_MTL_BACKEND OFF)
endif()

if(ST2110_HAS_MTL_BACKEND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ST2110_MTL REQUIRED IMPORTED_TARGET mtl)
endif()

set(ST2110CORE_SOURCES
        src/stub.cpp
        src/backends/receive_local_policy.cpp
        src/backends/socket/socket_rx_single_media_backend_base.cpp
)

if(ST2110_HAS_MTL_BACKEND)
    list(APPEND ST2110CORE_SOURCES
            src/backends/mtl/mtl_rx_video_backend.cpp
    )
endif()

add_library(st2110core STATIC
        ${ST2110CORE_SOURCES}
)

target_include_directories(st2110core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(st2110core PUBLIC cxx_std_23)

target_compile_definitions(st2110core PRIVATE
        ST2110_HAS_MTL_BACKEND=$<BOOL:${ST2110_HAS_MTL_BACKEND}>
)

if(ST2110_HAS_MTL_BACKEND)
    target_link_libraries(st2110core PRIVATE
            PkgConfig::ST2110_MTL
    )
endif()