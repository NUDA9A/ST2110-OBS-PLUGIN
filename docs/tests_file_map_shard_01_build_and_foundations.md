### tests/CMakeLists.txt
- Роль:
    - объявляет helper `add_st2110_test(name ...)`;
    - регистрирует все unit / architecture / regression tests через CTest;
    - каждый test executable линкуется с `st2110core`;
    - каждому test target явно задается `cxx_std_23`, чтобы IDE/test-target compilation model не расходился с project/toolchain requirements;
    - регистрирует `test_backend_factory` как обычный test target, а затем задает для него per-target build/test contract через `ST2110_TEST_EXPECT_MTL_BUILT`;
    - выводит `ST2110_TEST_EXPECT_MTL_BUILT` из текущей platform/build expectation:
        - `ON` для `CMAKE_SYSTEM_NAME STREQUAL "Linux"`;
        - `OFF` для остальных платформ;
    - инжектирует `ST2110_TEST_EXPECT_MTL_BUILT=$<BOOL:${ST2110_TEST_EXPECT_MTL_BUILT}>` только в `test_backend_factory`, чтобы assertions о MTL built/unavailable boundary были привязаны к test-target compile definition, а не к ad hoc логике внутри теста.
- Сущности:
    - `add_st2110_test(...)`
    - `add_st2110_test(test_backend_factory test_backend_factory.cpp)`
    - `set(ST2110_TEST_EXPECT_MTL_BUILT ON)` under Linux
    - `set(ST2110_TEST_EXPECT_MTL_BUILT OFF)` for non-Linux
    - `target_compile_definitions(test_backend_factory PRIVATE ST2110_TEST_EXPECT_MTL_BUILT=$<BOOL:${ST2110_TEST_EXPECT_MTL_BUILT}>)`
    - targets for smoke/base tests, ODR/link regression target, RTP/ST 2110 packet parsing, packet admission, reorder, frame assembly, depacketizer, video signaling, video timing/playout/timestamp mapping, video SDP ingestion, audio signaling model tests, audio SDP ingestion/timing tests, audio receiver bootstrap tests, backend interface tests, backend factory tests, socket runtime tests, Linux socket receive-port tests, socket video/audio backend tests, socket operational architecture tests, and shared reorder-tolerance policy tests.

### tests/test_smoke.cpp
- Роль:
    - минимальный smoke test для CTest/build pipeline.

### tests/test_endian.cpp
- Роль:
    - проверяет big-endian helpers `read_be16` / `read_be32`.

### tests/test_error.cpp
- Роль:
    - проверяет общий `Error` enum / string mapping.
    - теперь покрывает не только базовые parse/validation errors, но и new backend/runtime operational error vocabulary.
- Покрывает:
    - non-empty string mapping for all known `Error` values;
    - distinct string mapping for all known `Error` values;
    - explicit backend/runtime error classification through `is_backend_runtime_error(...)`:
        - parse/validation errors are not classified as backend/runtime errors;
        - backend/runtime errors are classified correctly;
        - unknown enum values are not classified as backend/runtime errors.
    - regression for unknown enum handling:
        - unknown `Error` value no longer renders as `"OK"`.
- Фиксирует:
    - common error vocabulary now separates parse/validation failures from backend/runtime operational failures;
    - future backend lifecycle/runtime work can classify operational failures without overloading packet/signaling parse errors.

### tests/test_bytespan.cpp
- Роль:
    - проверяет базовый `ByteSpan` alias / span contract.

### tests/test_stats.cpp
- Роль:
    - проверяет базовые stats/counter structs и helper behavior.

### tests/test_header_odr_link_main.cpp
### tests/test_header_odr_link_a.cpp
### tests/test_header_odr_link_b.cpp
- Роль:
    - multi-translation-unit ODR/link regression test для public headers.
- Target:
    - `test_header_odr_link`

### tests/test_timestamp_ns.cpp
- Роль:
    - проверяет базовый internal timestamp type contract.