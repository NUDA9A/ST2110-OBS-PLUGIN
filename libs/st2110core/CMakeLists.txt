option(ST2110_WITH_MTL "Build MTL-backed receive integration" OFF)
set(ST2110_MTL_INCLUDE_DIRS "" CACHE PATH "MTL include directory when ST2110_WITH_MTL=ON")
set(ST2110_MTL_LIBRARIES "" CACHE STRING "MTL libraries when ST2110_WITH_MTL=ON")

set(ST2110CORE_SOURCES
        src/stub.cpp
        src/socket_rx_single_media_backend_base.cpp
        src/backend_factory_registry.cpp
        src/mtl_rx_backend_factory_unavailable.cpp
)

if(ST2110_WITH_MTL)
    list(REMOVE_ITEM ST2110CORE_SOURCES
            src/mtl_rx_backend_factory_unavailable.cpp
    )
    list(APPEND ST2110CORE_SOURCES
            src/mtl_rx_backend_factory.cpp
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
        ST2110_WITH_MTL=$<BOOL:${ST2110_WITH_MTL}>
)

if(ST2110_WITH_MTL)
    if(ST2110_MTL_INCLUDE_DIRS STREQUAL "")
        message(FATAL_ERROR "ST2110_WITH_MTL=ON requires ST2110_MTL_INCLUDE_DIRS")
    endif()

    if(ST2110_MTL_LIBRARIES STREQUAL "")
        message(FATAL_ERROR "ST2110_WITH_MTL=ON requires ST2110_MTL_LIBRARIES")
    endif()

    target_include_directories(st2110core PRIVATE
            ${ST2110_MTL_INCLUDE_DIRS}
    )

    target_link_libraries(st2110core PRIVATE
            ${ST2110_MTL_LIBRARIES}
    )
endif()