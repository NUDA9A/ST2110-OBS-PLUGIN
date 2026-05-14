find_package(libobs REQUIRED)

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

target_link_libraries(st2110_obs PRIVATE
        st2110core
        OBS::libobs
)

target_compile_features(st2110_obs PRIVATE cxx_std_23)

set_target_properties(st2110_obs PROPERTIES
        PREFIX ""
        OUTPUT_NAME "st2110_obs"
)