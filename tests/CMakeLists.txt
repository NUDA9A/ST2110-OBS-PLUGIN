add_executable(st2110_tests
        test_smoke.cpp
)

target_link_libraries(st2110_tests PRIVATE st2110core)
target_compile_features(st2110_tests PRIVATE cxx_std_23)

add_test(NAME st2110_tests COMMAND st2110_tests)