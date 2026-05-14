find_package(libobs REQUIRED)

find_path(ST2110_NDI_INCLUDE_DIR
        NAMES Processing.NDI.Lib.h
        HINTS
        "$ENV{NDI_SDK_DIR}/include"
        "$ENV{NDI_INCLUDE_DIR}"
        "$ENV{NDILIB_INCLUDE_DIR}"
        PATH_SUFFIXES include
)

if(ST2110_NDI_INCLUDE_DIR)
    set(ST2110_HAS_NDI_DISCOVERY ON)
else()
    set(ST2110_HAS_NDI_DISCOVERY OFF)
endif()

add_library(st2110_obs MODULE)

target_sources(st2110_obs PRIVATE
        src/plugin-main.cpp
        src/st2110-source.cpp
        src/obs-synchronized-frame-sink.cpp
        src/source_runtime.cpp
        src/discovery_provider.cpp
        src/sdp_media_selection.cpp
        src/sdp_parser_dispatch.cpp
)

target_include_directories(st2110_obs PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(ST2110_HAS_NDI_DISCOVERY)
    target_include_directories(st2110_obs PRIVATE
            ${ST2110_NDI_INCLUDE_DIR}
    )
endif()

target_link_libraries(st2110_obs PRIVATE
        st2110core
        OBS::libobs
)

if(ST2110_HAS_NDI_DISCOVERY AND UNIX AND NOT APPLE)
    target_link_libraries(st2110_obs PRIVATE
            dl
    )
endif()

target_compile_features(st2110_obs PRIVATE cxx_std_23)

target_compile_definitions(st2110_obs PRIVATE
        ST2110_HAS_NDI_DISCOVERY=$<BOOL:${ST2110_HAS_NDI_DISCOVERY}>
)

set_target_properties(st2110_obs PROPERTIES
        PREFIX ""
        OUTPUT_NAME "st2110_obs"
)