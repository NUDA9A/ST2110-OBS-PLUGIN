### tests/CMakeLists.txt
- Роль:
    - объявляет helper `add_st2110_test(name ...)`;
    - регистрирует все unit / architecture / regression tests через CTest;
    - каждый test executable линкуется с `st2110core`;
    - каждому test target явно задается `cxx_std_23`, чтобы IDE/test-target compilation model не расходился с project/toolchain requirements.
- Сущности:
    - `add_st2110_test(...)`
    - targets for smoke/base tests, RTP/ST2110 packet parsing, packet admission, reorder, frame assembly, depacketizer, video signaling, SDP ingestion, timing, playout, audio signaling model tests, audio SDP ingestion tests, audio receiver bootstrap tests, backend interface tests, backend factory tests, audio frame storage tests, audio packet model tests, socket runtime tests, Linux socket receive-port tests, socket video backend tests, and socket audio backend tests.

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

### tests/test_config_validation.cpp
- Роль:
    - проверяет общие config validation helpers.
    - дополнительно покрывает derived audio helper behavior where applicable:
        - deriving `samples_per_packet` from sampling rate and packet time;
        - invalid zero values;
        - non-integral sample counts.

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