add_library(st2110core STATIC
        src/stub.cpp
)

target_include_directories(st2110core PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(st2110core PUBLIC cxx_std_23)