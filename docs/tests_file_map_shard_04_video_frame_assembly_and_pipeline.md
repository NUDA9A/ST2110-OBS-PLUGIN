### tests/test_video_frame.cpp
- Роль:
    - проверяет базовый `VideoFrame` / `VideoFrameView` contract.

### tests/test_video_frame_mutable_access.cpp
- Роль:
    - проверяет mutable UYVY storage access:
        - stride;
        - row access;
        - plane data.

### tests/test_frame_write_coverage.cpp
- Роль:
    - проверяет byte coverage tracking for frame completeness.

### tests/test_frame_assembler_lifecycle.cpp
- Роль:
    - проверяет `FrameAssembler` lifecycle:
        - begin;
        - write segment;
        - end.

### tests/test_frame_assembler_bounds.cpp
- Роль:
    - проверяет bounds checks:
        - invalid row;
        - invalid offset;
        - segment extending beyond row.

### tests/test_frame_assembler_completeness.cpp
- Роль:
    - проверяет completeness tracking.

### tests/test_frame_assembler_partial_policy.cpp
- Роль:
    - проверяет partial frame policy:
        - drop;
        - emit-with-flag.

### tests/test_video_scan_mode.cpp
- Роль:
    - проверяет, что `VideoScanMode` modeled as an independent axis across config and depacketizer boundaries, а не как скрытое свойство pixel/storage format.
- Покрывает:
    - distinct enum-value contract for:
        - `Progressive`;
        - `Interlaced`;
        - `PsF`.
    - `RxVideoConfig` validation:
        - progressive accepted;
        - interlaced accepted as a recognized model value;
        - PsF accepted as a recognized model value;
        - unknown scan mode rejected as `InvalidValue`.
    - `DepacketizerConfig` scan-mode surface:
        - default scan mode is progressive;
        - configured non-progressive mode is observable through `Depacketizer::scan_mode()`.
    - independence from handoff/storage format:
        - the same `PixelFormat::UYVY` config remains structurally valid under progressive, interlaced, and PsF scan modes.
- Фиксирует:
    - scan mode is a first-class modeled axis separate from `PixelFormat`;
    - depacketizer configuration preserves scan-mode intent explicitly instead of deriving it from storage format.

### tests/test_video_receive_semantics.cpp
- Роль:
    - проверяет mode-aware receive semantics boundary:
        - unit kind;
        - completion policy;
        - scan-mode-specific packet acceptance;
        - architecture-level depacketizer behavior for current progressive path.
- Покрывает:
    - `VideoAssemblyUnitKind` derivation from `VideoScanMode`;
    - rejection of unknown scan mode;
    - progressive completion policy;
    - localized `Unsupported` for interlaced / PsF completion policy in current MVP;
    - progressive depacketizer marker behavior unchanged;
    - progressive depacketizer timestamp/key-change behavior unchanged;
    - non-progressive depacketizer mode remains locally rejected.
- Фиксирует:
    - assembly-unit semantics remain mode-aware by architecture;
    - cross-packet ordering enforcement is layered under the same receive-semantics boundary rather than pushed into low-level generic payload parsing.

### tests/test_video_assembly_key.cpp
- Роль:
    - проверяет `VideoAssemblyKey` grouping helper behavior.

### tests/test_video_field_id_boundary.cpp
- Роль:
    - regression coverage для переноса `field_id` / `F` handling из generic payload-header validation в mode-aware runtime boundary.

### tests/test_video_segment_constraints.cpp
- Роль:
    - проверяет format-specific segment constraints:
        - pgroup size;
        - offset alignment;
        - UYVY constraints.

### tests/test_video_segment_placement.cpp
- Роль:
    - проверяет mode/format-aware packet segment -> frame write mapping.

### tests/test_video_packet_trailing_padding.cpp
- Роль:
    - проверяет packet trailing padding policy boundary independent from generic parsing.

### tests/test_depacketizer_api.cpp
- Роль:
    - проверяет public depacketizer API shape.

### tests/test_depacketizer_grouping.cpp
- Роль:
    - проверяет current progressive packet grouping behavior.

### tests/test_depacketizer_marker.cpp
- Роль:
    - проверяет marker-driven completion through current progressive policy.

### tests/test_depacketizer_writes.cpp
- Роль:
    - проверяет depacketizer segment writes into assembled frame storage;
    - покрывает assembly-unit-local cross-packet SRD row/offset monotonicity for the current `Progressive + GPM` path;
    - теперь также фиксирует packet-atomic write behavior: invalid later segment must write none of that packet’s segments.
- Покрывает:
    - single-packet complete frame emission;
    - valid multi-packet same-row fragmentation with strictly increasing offset;
    - accepted row advance across packets within the same frame/unit;
    - rejection of a later packet with lower row number;
    - rejection of a later packet with the same row and lower/equal offset;
    - proof that rejected regressing packets do not corrupt already-assembled frame bytes and do not prevent later valid completion of the same unit;
    - valid multi-segment packet write behavior;
    - timestamp/key transition behavior for starting a new unit.
- Фиксирует:
    - cross-packet SRD monotonicity is enforced in depacketizer assembly state, not only packet-locally;
    - packet rejection happens before write mutation for the rejected packet;
    - valid progressive packet behavior remains unchanged.

### tests/test_depacketizer_stats.cpp
- Роль:
    - проверяет depacketizer stats accounting.

### tests/test_depacketizer_unit_state.cpp
- Роль:
    - проверяет current unit state / reset behavior.

### tests/test_depacketizer_segment_mapping.cpp
- Роль:
    - проверяет depacketizer integration with segment placement mapping.

### tests/test_depacketizer_trailing_padding.cpp
- Роль:
    - проверяет depacketizer-side trailing padding validation.

### tests/test_depacketizer_trailing_padding_state.cpp
- Роль:
    - regression coverage for depacketizer state integrity when packet rejection happens before frame mutation because of invalid trailing padding, invalid transition conditions, or packet-level validation failure.
- Покрывает:
    - invalid first packet with forbidden trailing padding does not open a unit;
    - invalid key-transition packet with forbidden trailing padding does not close or replace the previous in-progress unit;
    - after such rejection, the original unit can still be completed by a later valid packet with a monotonic same-row offset advance;
    - rejected packet does not count as a used packet and does not corrupt assembler/depacketizer state.
- Фиксирует:
    - packet-level rejection before assembly mutation preserves current depacketizer state;
    - this remains compatible with both the localized cross-packet row/offset ordering boundary and packet-atomic segment-write behavior.

### tests/test_video_unit_reconstructor.cpp
- Роль:
    - проверяет progressive video unit reconstructor behavior.

### tests/test_video_receive_pipeline.cpp
- Роль:
    - проверяет composition layer:
        - depacketizer;
        - video unit reconstructor;
        - reset;
        - config consistency.

