### tests/test_video_receiver_timing.cpp
- Роль:
    - проверяет receiver timing capability/requirements/config validation.

### tests/test_video_receiver_timing_signaling.cpp
- Роль:
    - проверяет consistency validation между receiver timing config и video stream signaling.

### tests/test_video_receiver_timing_bootstrap.cpp
- Роль:
    - проверяет timing-aware signaling-driven bootstrap wrapper.
- Покрывает:
    - successful timing-aware bootstrap composition;
    - missing `SSN` rejected before timing-aware runtime/bootstrap projection;
    - unsupported sender-type rejection remains localized in timing boundary;
    - unconsumed `TSDELAY` rejection remains localized in timing boundary.

### tests/video_receiver_timing_architecture_test.cpp
- Роль:
    - architecture regression test для receiver timing boundary placement;
    - фиксирует, что timing-aware layer остается overlay над generic signaling/bootstrap path.

### tests/video_playout_timing_test.cpp
- Роль:
    - focused unit test для receiver-side video playout/reconstruction timing boundary.
    - проверяет, что receiver-side playout timing остается отдельным overlay above RTP timestamp mapping and works correctly for both explicit initial-anchor modes.
- Покрывает:
    - `validate_video_receiver_playout_timing_config(...)` for default and non-zero link offset delay.
    - `video_receiver_playout_timing_decision(...)`:
        - reconstruction timestamp derived from mapped media timestamp plus link-offset delay;
        - overflow rejection.
    - separation between RTP timestamp mapping and playout timing in `ConfiguredReference` mode:
        - RTP mapper can produce a reference-based non-zero media timestamp;
        - playout timing only adds reconstruction delay on top of that mapped media timestamp.
    - separation between RTP timestamp mapping and playout timing in `FirstObservedBecomesLocalZero` mode:
        - first observed RTP timestamp maps to `0 ns`;
        - playout timing still independently applies receiver-side reconstruction delay.
    - `video_reconstructed_frame_timing(...)`:
        - attaches playout timing metadata to reconstructed frame metadata without changing frame payload semantics.
- Фиксирует:
    - receiver playout/reconstruction timing remains a separate boundary above media timestamp mapping;
    - explicit initial-anchor policy changes media timestamp origin, but does not collapse playout timing into the mapper.

### tests/video_timestamp_mapping_test.cpp
- Роль:
    - focused unit test для `video_timestamp_mapping.hpp`.
    - проверяет explicit initial-anchor policy, basic RTP-to-nanoseconds conversion behavior, wraparound handling, validation, и synthetic fps-based mapper path.
- Покрывает:
    - default/manual `VideoRtpTimestampMapperConfig{}` behavior:
        - explicit default mode maps the first observed RTP timestamp to local `0 ns`;
        - subsequent packets map by RTP delta.
    - `ConfiguredReference` mode:
        - anchor RTP timestamp maps to configured anchor nanoseconds;
        - `90000` RTP ticks map to one second relative to the configured anchor;
        - progressive 25 fps tick step remains correct when mapping from RTP-domain deltas;
        - wraparound handling remains correct;
        - backward timestamp rejected.
    - validation:
        - zero RTP clock rate rejected;
        - `FirstObservedBecomesLocalZero` with non-zero anchor fields rejected.
    - synthetic mapper path:
        - frame-index-to-nanoseconds cadence via explicit fps config;
        - invalid frame-rate config rejected.
- Фиксирует:
    - explicit initial-anchor policy is part of the public video timestamp-mapper config boundary;
    - default/manual path now uses first-observed-local-zero semantics explicitly instead of silently behaving like configured reference with `0/0`.

### tests/video_timestamp_mapping_invariants_test.cpp
- Роль:
    - invariants/regression test для video RTP timestamp mapping boundary и его interaction with reconstructed-frame playout timing.
    - фиксирует две явные политики initial anchoring:
        - `ConfiguredReference`;
        - `FirstObservedBecomesLocalZero`.
- Покрывает:
    - monotonic 90 kHz RTP-to-nanoseconds mapping in `ConfiguredReference` mode.
    - default/manual-path behavior:
        - first observed RTP timestamp maps to local `0 ns`;
        - later packets map by RTP delta from that first observation.
    - continuity across 32-bit RTP timestamp wraparound in both modes.
    - rejection behavior:
        - backward delta rejected;
        - exact half-range ambiguous delta rejected;
        - invalid RTP clock rate rejected;
        - configured-reference anchor-plus-offset overflow rejected.
    - validation of `FirstObservedBecomesLocalZero` policy:
        - non-zero anchor fields are rejected for that mode.
    - synthetic mapper invariants:
        - explicit fps-based cadence remains separate from RTP-domain mapping;
        - invalid fps config rejected.
    - reconstructed-frame timing overlay:
        - `ConfiguredReference` mapped media timestamp plus playout offset;
        - `FirstObservedBecomesLocalZero` mapped media timestamp plus playout offset.
- Фиксирует:
    - video timestamp mapping now explicitly supports both configured-reference and first-observed-local-zero semantics;
    - default/manual path no longer relies on an unexplained hidden `0/0` anchor artifact;
    - playout timing remains layered above media timestamp mapping rather than fused with it.