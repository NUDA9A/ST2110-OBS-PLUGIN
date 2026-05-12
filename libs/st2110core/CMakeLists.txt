if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(ST2110_HAS_MTL_BACKEND OFF)
else()
    set(ST2110_HAS_MTL_BACKEND OFF)
endif()

if(ST2110_HAS_MTL_BACKEND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ST2110_MTL IMPORTED_TARGET mtl)

    if(NOT ST2110_MTL_FOUND)
        message(FATAL_ERROR
                "Linux st2110core builds require an installed MTL pkg-config package named 'mtl'. "
                "Install Media Transport Library and set PKG_CONFIG_PATH if it is installed outside the default pkg-config search paths.")
    endif()
endif()

set(ST2110CORE_SOURCES
        src/stub.cpp
        src/backends/socket/socket_rx_single_media_backend_base.cpp
)

if(ST2110_HAS_MTL_BACKEND)
    list(APPEND ST2110CORE_SOURCES)
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