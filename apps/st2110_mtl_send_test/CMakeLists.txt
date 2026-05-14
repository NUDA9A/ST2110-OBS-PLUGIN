find_path(ST2110_NDI_INCLUDE_DIR
        NAMES Processing.NDI.Lib.h
        HINTS
        "$ENV{NDI_SDK_DIR}/include"
        "$ENV{NDI_INCLUDE_DIR}"
        "$ENV{NDILIB_INCLUDE_DIR}"
        PATH_SUFFIXES include
)

if(NOT ST2110_NDI_INCLUDE_DIR)
    message(FATAL_ERROR
            "st2110_mtl_send_test requires NDI SDK headers. "
            "Install NDI SDK or set NDI_INCLUDE_DIR/NDI_SDK_DIR.")
endif()

find_package(PkgConfig REQUIRED)

if(NOT TARGET PkgConfig::ST2110_MTL)
    pkg_check_modules(ST2110_MTL REQUIRED IMPORTED_TARGET mtl)
endif()

add_executable(st2110_mtl_send_test
        main.cpp
)

target_include_directories(st2110_mtl_send_test PRIVATE
        ${ST2110_NDI_INCLUDE_DIR}
)

target_link_libraries(st2110_mtl_send_test PRIVATE
        st2110core
        PkgConfig::ST2110_MTL
)

if(UNIX AND NOT APPLE)
    target_link_libraries(st2110_mtl_send_test PRIVATE
            dl
    )
endif()

target_compile_features(st2110_mtl_send_test PRIVATE cxx_std_23)