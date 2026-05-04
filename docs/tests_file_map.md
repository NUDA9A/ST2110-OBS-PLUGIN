# ST2110-OBS-PLUGIN — Tests file map

> Назначение файла:
> - держать карту тестового покрытия отдельно от production code map;
> - фиксировать, какой subsystem / boundary покрывает каждый тест;
> - не дублировать полную реализацию тестов;
> - обновлять после принятия задач, если добавлены, удалены или существенно изменены тестовые файлы.
>
> Важно:
> - production headers / runtime code описываются в `code_map.md`;
> - test targets / `.cpp` тесты описываются здесь;
> - `plan.md` хранит backlog/status/deviations;
> - `code_map.md` тесты не содержит.

## Build integration

## Build integration

### tests/CMakeLists.txt
- Роль:
    - объявляет helper `add_st2110_test(name ...)`;
    - регистрирует все unit / architecture / regression tests через CTest;
    - каждый test executable линкуется с `st2110core`;
    - каждому test target явно задается `cxx_std_23`, чтобы IDE/test-target compilation model не расходился с project/toolchain requirements.
- Сущности:
    - `add_st2110_test(...)`
    - targets for smoke/base tests, RTP/ST2110 packet parsing, packet admission, reorder, frame assembly, depacketizer, video signaling, SDP ingestion, timing, playout, audio signaling model tests, audio SDP ingestion tests, audio receiver bootstrap tests, backend interface tests, backend factory tests, audio frame storage tests, audio packet model tests, socket runtime tests, Linux socket receive-port tests, socket video backend tests, and socket audio backend tests.

## Smoke / common foundations

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

## Manual config / backend interface

### tests/test_rx_config.cpp
- Роль:
    - проверяет базовую validation модели `RxVideoConfig`.
    - проверяет, что manual/backend-facing video runtime config carries already-modeled runtime axes:
        - `VideoScanMode`;
        - `VideoPackingMode`.
    - проверяет current video packing-mode runtime support boundary:
        - `VideoPackingMode::Gpm` accepted;
        - `VideoPackingMode::Bpm` represented but rejected as `Unsupported` by current MVP runtime support;
        - invalid packing-mode enum rejected as `InvalidValue`.
    - проверяет `RxAudioConfig` runtime validation:
        - Level A-oriented default runtime support;
        - channel count bounds;
        - sample rate / packet time validation through runtime support policy;
        - derived `samples_per_packet` consistency;
        - UDP port;
        - dynamic RTP payload type;
        - destination IP requirement;
        - local IP allowed to be empty;
        - unsupported audio sample format rejection;
        - explicit `pcm_bit_depth` validation for `Bits16` / `Bits24` and rejection of invalid bit-depth enum values.
    - проверяет architecture property for audio runtime support:
        - `validate_rx_audio_config(...)` is a thin default-policy wrapper;
        - `validate_rx_audio_config_against_runtime_support(...)` can validate a custom support policy without rewriting default validation;
        - non-default packet time support can be accepted by explicit support policy while remaining rejected by current default policy.

### tests/test_backend_interface.cpp
- Роль:
    - проверяет backend/sink interface contracts;
    - покрывает FakeVideoBackend -> FakeVideoSink delivery path;
    - покрывает FakeAudioBackend -> FakeAudioSink delivery path;
    - покрывает combined video+audio backend shape over one common `IRxBackend` base;
    - фиксирует explicit backend lifecycle result/state boundary;
    - теперь также фиксирует generic backend stats snapshot boundary.
- Покрывает:
    - abstract/interface shape:
        - `IRxBackend`;
        - `IRxVideoBackend`;
        - `IRxAudioBackend`;
        - `IVideoFrameSink`;
        - `IAudioFrameSink`.
    - backend capability model:
        - `RxMediaKind`;
        - `RxBackendCapabilities`;
        - `supports_media(...)`;
        - unknown media enum value returns `false`.
    - lifecycle state model:
        - `RxBackendState`;
        - `RxBackendLifecycleResult`;
        - `backend_is_stopped(...)`;
        - `backend_media_active(...)`.
    - common backend API:
        - `backend_name()`;
        - `stop() -> RxBackendLifecycleResult`;
        - `state() -> RxBackendState`;
        - `stats() -> BackendStats`;
        - `capabilities()`.
    - media-specific start API:
        - `IRxVideoBackend::start_video(...) -> RxBackendLifecycleResult`;
        - `IRxAudioBackend::start_audio(...) -> RxBackendLifecycleResult`.
    - lifecycle policy:
        - stop before successful start is accepted and leaves backend stopped;
        - repeated stop is accepted and remains stopped;
        - repeated start on already-active media returns `InvalidBackendState`;
        - failed start can leave backend stopped and retryable;
        - combined backend can hold video and audio active simultaneously.
    - stats behavior in fake backends:
        - zero snapshot before delivery;
        - video frame delivery increments `frames_delivered` / `media_units_delivered`;
        - audio delivery increments `media_units_delivered`;
        - failed-start path can expose backend-local stats changes without using sinks.
- Фиксирует:
    - backend lifecycle/runtime failures remain inside backend boundary and are not routed through sinks;
    - backend observability is available through `stats()` and remains separate from sink code.

### tests/test_backend_factory.cpp
- Роль:
    - проверяет backend factory / selector boundary.
- Покрывает:
    - modeled backend-kind axis:
        - `RxBackendKind`;
        - `validate_rx_backend_kind(...)`;
        - `rx_backend_kind_name(...)`;
        - `parse_rx_backend_kind(...)`.
    - descriptor / selection validation:
        - `RxBackendDescriptor`;
        - `validate_rx_backend_descriptor(...)`;
        - `RxBackendSelection`;
        - `validate_rx_backend_selection(...)`.
    - abstract factory shape:
        - `IRxBackendFactory`;
        - `descriptor()`;
        - `create_backend()`.
    - common backend API from created backends:
        - `backend_name()`;
        - `capabilities()`;
        - `state()`;
        - `stats()`;
        - `stop()`.
    - factory selection:
        - `select_rx_backend_factory(...)`;
        - selection by requested backend kind;
        - selection constrained by requested media capability;
        - unavailable backend rejection;
        - missing backend-kind rejection;
        - null factory entry rejection.
    - backend creation:
        - `create_rx_backend(...)`;
        - uses the selected factory only;
        - rejects null backend results from a factory;
        - created backend starts in a stopped `RxBackendState`;
        - created backend `stats()` starts from a zero snapshot;
        - created backend `stop()` returns a lifecycle result.
- Фиксирует:
    - backend selection/creation remains separate from backend lifecycle semantics;
    - factory-created backends expose both lifecycle/state and stats snapshot boundaries immediately after construction.

## RTP / ST 2110-20 packet parsing

### tests/test_rtp_parser.cpp
- Роль:
    - проверяет RTP header parsing:
        - version;
        - minimum header size;
        - payload type;
        - marker;
        - sequence number;
        - timestamp;
        - SSRC;
        - payload offset / payload length.

### tests/test_rtp_seq.cpp
- Роль:
    - проверяет RTP sequence wrap comparison/distance helpers.

### tests/test_rtp_payload.cpp
- Роль:
    - проверяет RTP payload span extraction;
    - покрывает CSRC/header-offset behavior.

### tests/test_st2110_20_types.cpp
- Роль:
    - проверяет базовые ST 2110-20 payload header structs/types.

### tests/test_st2110_20_parse.cpp
- Роль:
    - проверяет parsing ST 2110-20 payload header:
        - extended sequence high bits;
        - SRD headers;
        - segment count / header size.

### tests/test_st2110_20_validate.cpp
- Роль:
    - проверяет structural validation ST 2110-20 payload header.

### tests/test_extended_seq.cpp
- Роль:
    - проверяет `combine_extended_seq(...)`.

### tests/test_zero_length_srd.cpp
- Роль:
    - regression coverage для стандартного zero-length SRD special-case.

### tests/test_st2110_20_ordering.cpp
- Роль:
    - проверяет packet-local SRD ordering rules:
        - row number ordering;
        - offset ordering within row.

### tests/test_packet_view.cpp
- Роль:
    - проверяет normalized `PacketView` contract.

### tests/test_packet_view_parse.cpp
- Роль:
    - проверяет full UDP datagram -> RTP/ST2110-20 `PacketView` parsing path.

### tests/test_packet_view_trailing_padding.cpp
- Роль:
    - проверяет generic extraction of trailing payload bytes after SRD-covered data.

### tests/test_packet_parse_stats.cpp
- Роль:
    - проверяет packet parse stats structs/counters.

### tests/test_packet_parse_policy.cpp
- Роль:
    - проверяет packet-size policy boundary:
        - UDP datagram size semantics;
        - absent `MAXUDP` defaulting to Standard UDP Size Limit;
        - acceptance of explicit Standard / Extended limits only;
        - oversize rejection before wire parse.
- Покрывает:
    - `udp_datagram_size_bytes(...)` adds UDP header bytes;
    - absent policy override => effective Standard UDP Size Limit;
    - explicit Standard UDP Size Limit accepted and enforced;
    - explicit Extended UDP Size Limit accepted and enforced;
    - non-boundary numeric policy values rejected by config validation;
    - oversized packet rejected at packet-policy stage before wire parsing;
    - invalid policy config rejected before packet checks.

### tests/test_packet_parse_integration_stats.cpp
- Роль:
    - проверяет integrated packet parse path with stage-specific stats recording.

## Reorder buffer

### tests/test_reorder_buffer_interface.cpp
- Роль:
    - проверяет `IReorderBuffer` abstraction, `StoredPacket` ownership/view restoration, explicit gap-flush hook, and reorder stats snapshot boundary.
- Покрывает:
    - abstract/interface shape:
        - `push(...)`;
        - `pop_next()`;
        - `flush_missing_once()`;
        - `stats() -> ReorderBufferStats`;
        - `reset()`.
    - `StoredPacket::view()`:
        - RTP header field restoration;
        - extended sequence restoration;
        - SRD header restoration;
        - payload segment view reconstruction.
    - fake reorder-buffer behavior:
        - push/pop lifecycle;
        - blocked-head / missing-once accounting;
        - single-step gap flush unblocks the head once;
        - reset behavior;
        - zero-after-reset stats behavior.
- Фиксирует:
    - generic reorder interface now exposes both stats and an explicit one-step gap-flush boundary;
    - backend/runtime code can stay on the abstraction boundary without depending on concrete reorder-buffer types.

### tests/test_fixed_reorder_buffer.cpp
- Роль:
    - проверяет fixed-window reorder behavior by extended sequence number.

### tests/test_fixed_reorder_buffer_stats.cpp
- Роль:
    - проверяет reorder stats:
        - duplicates;
        - out-of-window;
        - late/missing accounting.

### tests/test_fixed_reorder_buffer_flush.cpp
- Роль:
    - проверяет missing sequence flush behavior.

## Video frame storage / frame assembly

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

## Video scan mode / receive semantics / placement

### tests/test_video_scan_mode.cpp
- Роль:
    - проверяет, что `VideoScanMode` modeled separately from `PixelFormat`.

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

## Depacketizer / video receive pipeline

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

## Video signaling / runtime projection

### tests/test_video_signaling.cpp
- Роль:
    - проверяет modeled `VideoStreamSignaling` validation.
- Покрывает:
    - valid progressive `GPM` signaling;
    - valid `BPM` signaling at the structural signaling layer;
    - standards-clean `SSN` requirement:
        - missing `SSN` rejected by `validate_video_stream_signaling(...)`;
        - existing valid signaling with explicit `SSN` remains accepted.
    - signaled dimension limits in the signaling/media-description boundary:
        - width/height `1` accepted structurally;
        - width/height `32767` accepted structurally;
        - width/height `0` rejected structurally;
        - width/height `32768` rejected structurally.
    - invalid frame rate;
    - signaling-valid but runtime-invalid odd-width projection case;
    - finalized `MAXUDP` signaling policy:
        - Standard UDP Size Limit accepted;
        - Extended UDP Size Limit accepted;
        - non-boundary numeric values rejected;
        - values above Extended UDP Size Limit rejected;
        - packet-parse-policy derivation from signaling for Standard / Extended values;
        - absent `MAXUDP` keeps empty policy override and therefore Standard-by-default behavior.
    - signaling-valid but runtime-unsupported sampling projection case.
    - tightened ST 2110-20 `SSN` cross-field validation:
        - `BT709 + SDR + SSN=ST2110-20:2017` accepted;
        - `BT709 + SDR + SSN=ST2110-20:2022` rejected;
        - `ALPHA`/KEY signaling requires `SSN=ST2110-20:2022`;
        - `TCS=ST2115LOGS3` requires `SSN=ST2110-20:2022`.
    - localized runtime support boundary:
        - structurally valid odd-width signaling still fails only through runtime UYVY projection/config validation, not in signaling dimension-limit validation;
        - structurally valid `ALPHA`/KEY signaling remains runtime-unsupported only through `pixel_format_from_video_stream_signaling(...)`.

### tests/test_video_signaling_rx_match.cpp
- Роль:
    - проверяет consistency между signaling model и manual `RxVideoConfig`.
- Покрывает:
    - matching signaling and `RxVideoConfig` accepted;
    - width / height / frame-rate / scan-mode mismatch rejected;
    - missing `SSN` in signaling rejected before RX-match consistency logic.

### tests/test_video_signaling_to_rx_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to `RxVideoConfig`.
- Покрывает:
    - valid standards-clean signaling projects successfully;
    - invalid signaling rejected before projection;
    - missing `SSN` rejected before projection;
    - invalid transport arguments rejected after signaling projection;
    - scan mode is preserved structurally;
    - invalid payload type rejected;
    - structurally valid but unsupported runtime pixel-format mapping rejected as `Unsupported`.

### tests/test_video_signaling_to_pipeline_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to runtime video receive pipeline config.
- Покрывает:
    - depacketizer config projection;
    - reconstructor config projection;
    - composed pipeline config projection;
    - structural preservation of interlaced scan mode in projection helpers;
    - invalid signaling rejected before runtime projection;
    - missing `SSN` rejected before runtime projection;
    - structurally valid but runtime-unmappable media rejected as `Unsupported`.

### tests/test_video_receiver_bootstrap.cpp
- Роль:
    - проверяет generic signaling-driven receiver bootstrap composition.
- Покрывает:
    - successful composition of packet-parse policy, RX config, and receive-pipeline config from valid signaling;
    - absent `MAXUDP` override preserved through bootstrap policy projection;
    - invalid signaling rejected before downstream bootstrap projection;
    - missing `SSN` rejected before downstream bootstrap projection;
    - invalid transport inputs rejected after signaling-derived projection;
    - structurally valid but runtime-unmappable signaling rejected as `Unsupported`.

### tests/test_video_packing_mode_runtime_projection.cpp
- Роль:
    - проверяет runtime projection/support boundary для `VideoPackingMode`.
- Покрывает:
    - `GPM` projection into depacketizer / receive-pipeline / bootstrap configs;
    - `BPM` remains structurally valid in signaling model;
    - `BPM` remains rejected by current runtime projection/support boundaries as `Unsupported`;
    - missing `SSN` is rejected before packing-mode runtime projection is attempted.

### tests/test_video_signaled_media_properties.cpp
- Роль:
    - проверяет modeled video SDP/media properties separate from runtime `PixelFormat`.
- Покрывает:
    - validation of token-backed signaling/media enums:
        - `sampling`;
        - `colorimetry`;
        - `TCS`;
        - `SSN`;
        - `RANGE`.
    - structural `VideoBitDepth` validation including `16f`;
    - structural `VideoPixelAspectRatio` validation:
        - `1:1` accepted;
        - non-square ratios such as `12:11` accepted;
        - zero parts rejected.
    - signaling/media-description dimension validation:
        - explicit helper-level acceptance of `1` and `32767`;
        - rejection of `0` and `32768`;
        - stream-level acceptance of signaled min/max dimensions;
        - stream-level rejection of out-of-range dimensions.
    - standards-clean media-description behavior:
        - valid BT709/SDR media-description with explicit `SSN=ST2110-20:2017` accepted;
        - missing `SSN` rejected;
        - optional `TCS` and `RANGE` may still be absent where currently allowed.
    - rejection of invalid structural media fields;
    - tightened `SSN` cross-field validation;
    - tightened `RANGE` modeling and cross-field validation.
    - PAR/pixel-aspect-ratio as a signaling/media-description property:
        - non-square PAR accepted structurally;
        - invalid PAR rejected structurally;
        - runtime projection remains independent from PAR.
    - runtime projection remains separate:
        - valid `YCbCr422 + 8-bit` projects to `UYVY`;
        - structurally valid but unsupported media, including KEY/ALPHA, remain rejected only by runtime projection.
- Фиксирует:
    - `SSN` is no longer treated as an optional signaling/media-description field in standards-clean validation;
    - current UYVY-specific even-width runtime constraint still remains localized below the signaling boundary.

### tests/test_video_reference_clock.cpp
- Роль:
    - проверяет modeled `ReferenceClock` validation on the signaling boundary.
- Покрывает:
    - valid PTP reference clock with non-zero clock identity;
    - valid traceable PTP form with zero clock identity allowed only when `traceable=true`;
    - rejection of invalid modeled PTP shapes:
        - missing payload;
        - mixed `Ptp` + `LocalMac`;
        - unexpected raw token;
        - all-zero non-traceable clock identity.
    - valid localmac reference clock with non-zero MAC;
    - rejection of invalid modeled localmac shapes:
        - missing payload;
        - all-zero MAC;
        - mixed `LocalMac` + `Ptp`.
    - explicit `Other + raw_token` support for unknown/open-ended forms;
    - propagation of reference-clock validation through `validate_video_stream_signaling(...)`;
    - missing `SSN` rejection remains above/reference-clock-using signaling validation paths.
- Фиксирует:
    - strict modeled validation of known reference-clock forms remains localized on the signaling boundary;
    - standards-clean signaling still requires explicit `SSN` in addition to a valid reference clock.

### tests/test_video_sender_signaling.cpp
- Роль:
    - проверяет sender timing signaling fields:
        - sender type;
        - `TROFF`;
        - `CMAX`;
        - structural validation.
- Покрывает:
    - direct sender-field validation for `Narrow`, `NarrowLinear`, and `Wide`;
    - corrected ST 2110-21 optional-parameter semantics:
        - `TROFF` is allowed for `Narrow`, `NarrowLinear`, and `Wide` when present and positive;
        - `CMAX` is allowed for `Narrow`, `NarrowLinear`, and `Wide` when present and valid for the local modeled policy;
        - absent optional sender parameters remain accepted;
        - `TROFF=0` rejected;
        - `CMAX=0` rejected.
    - stream-level signaling validation for valid and invalid sender-timing cases;
    - missing `SSN` rejection remains explicit and happens before stream-level sender-timing acceptance is treated as standards-clean signaling.
- Фиксирует:
    - generic signaling validation no longer hardcodes a stricter sender-class policy than ST 2110-21 optional-parameter semantics;
    - stricter receiver/conformance checks must remain outside this generic signaling-validation boundary.

### tests/test_video_timing_signaling.cpp
- Роль:
    - проверяет timing-related signaling:
        - media clock mode;
        - timestamp mode;
        - timing validation.

## Video receiver timing / playout timing

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

## Video RTP timestamp mapping

### tests/test_timestamp_ns.cpp
- Роль:
    - проверяет базовый internal timestamp type contract.

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

## Video SDP raw media-section parsing / SDP ingestion

### tests/video_sdp_media_section_test.cpp
- Роль:
    - проверяет raw SDP video media-section selection boundary in `video_sdp_media_section.hpp`;
    - проверяет payload-bound `rtpmap` / `fmtp`;
    - проверяет preservation of raw media-line and unknown attributes;
    - проверяет tightened raw `m=video` validation for ST 2110 video SDP.
- Покрывает:
    - selection of the correct `m=video` section by expected payload type;
    - dynamic RTP payload-type requirement for selected video SDP;
    - media-line protocol validation;
    - preservation of raw `rtpmap`, `fmtp`, and unknown media attributes without prematurely collapsing them into final signaling;
    - preservation of scoped standalone timing/reference-clock attributes separately from fmtp media parameters.
- Фиксирует:
    - raw media-section parsing remains a non-destructive SDP boundary;
    - final requirements such as mandatory `TP` for standards-clean video SDP ingestion remain above this raw selection layer.

### tests/video_sdp_fmtp_test.cpp
- Роль:
    - проверяет strict raw parser for video `a=fmtp`.
- Покрывает:
    - parsing of required and optional known fmtp parameters;
    - preservation of unknown syntactically valid parameters;
    - canonical `exactframerate` parsing:
        - integer `25` accepted;
        - non-canonical integer rational `25/1` rejected;
        - canonical rational `30000/1001` accepted;
        - reducible rational such as `60000/2002` rejected;
        - zero numerator/denominator rejected;
        - malformed `exactframerate` rejected;
        - existing valid SDP examples using `30000/1001` remain accepted.
    - `PAR` parsing:
        - valid `PAR=1:1`;
        - valid non-square `PAR=12:11`;
        - canonical/minimal reduction where practical (for example `2:2 -> 1:1`);
        - malformed `PAR` rejection;
        - duplicate `PAR` rejection.
    - attribute-level parsing for matching / non-matching payload types;
    - malformed numeric and duplicate known-parameter rejection.
- Фиксирует:
    - raw fmtp parser keeps absent `PAR` as absent raw property;
    - signaling-level defaulting to `1:1` is left to later mapping/model layers;
    - `exactframerate` canonical form is enforced locally in the raw fmtp parser rather than in runtime cadence/timestamp logic.

### tests/video_sdp_signaling_adapter_test.cpp
- Роль:
    - проверяет adapter from raw SDP/fmtp media-description fields to `VideoStreamSignaling`.
- Покрывает:
    - mapping of known progressive video fmtp fields into explicit signaling enums/values;
    - default signaling-model `PAR=1:1` when raw `PAR` is absent;
    - mapping of explicit square `PAR=1:1`;
    - mapping of non-square `PAR=12:11`;
    - mapping of `FULLPROTECT` into `VideoRange::Known::FullProtect`;
    - interlaced / PsF scan-mode derivation;
    - packing-mode mapping;
    - rejection of malformed scan-mode / packing-mode combinations;
    - preservation of unknown future tokens through `Other + raw_token`, including unknown `RANGE` tokens;
    - rejection of malformed depth outside the signaling model.
- Фиксирует:
    - SDP-to-signaling adapter preserves PAR as a media-description property;
    - runtime/storage behavior remains outside this mapping boundary.

### tests/video_sdp_timing_attributes_test.cpp
- Роль:
    - проверяет raw parsing of SDP timing/reference/sender attributes.
- Покрывает:
    - parsing of known `ts-refclk` forms:
        - `ptp=IEEE1588-2008:<gmid>[:domain]`
        - `ptp=IEEE1588-2008:traceable`
        - `localmac=<EUI-48 MAC>`
    - preservation of unknown/open-ended reference-clock forms through `Other`;
    - rejection of malformed known `ptp` / `localmac` forms:
        - malformed PTP GMID;
        - malformed PTP domain;
        - malformed localmac.
    - parsing of known `mediaclk`, `TSMODE`, `TSDELAY`, `TP`, `TROFF`, `CMAX`;
    - session/media scoped timing resolution and media-over-session precedence;
    - helper-level detection of presence of resolved reference clock and media-level `mediaclk`.
- Фиксирует:
    - strict known reference-clock parsing is localized in the raw timing parser;
    - malformed known forms do not silently fall through as unknown/open-ended forms.

### tests/video_sdp_rtpmap_test.cpp
- Роль:
    - проверяет raw SDP `a=rtpmap` parsing/binding for selected payload type.

### tests/video_sdp_ingestion_test.cpp
- Роль:
    - проверяет final SDP-to-`VideoStreamSignaling` ingestion entry point and composition with raw media-section parsing.
- Покрывает:
    - full valid video SDP ingestion into signaling;
    - composition equivalence between raw-media-section path and final SDP entry point;
    - signaling-level default `PAR=1:1` when absent in SDP;
    - explicit square `PAR=1:1`;
    - non-square `PAR=12:11` surviving final SDP-to-signaling mapping;
    - malformed `PAR` rejection through final ingestion;
    - propagation of invalid RTP/timing/media errors from final ingestion;
    - final-ingestion requirement for:
        - `ts-refclk`
        - media-level `mediaclk`
    - acceptance of valid reference-clock forms:
        - PTP GMID form
        - PTP traceable form
        - localmac form
    - rejection of missing or malformed reference-clock signaling.
- Фиксирует:
    - final video SDP ingestion is standards-clean only when both required timing-clock attributes are present in the accepted form;
    - runtime video projection/bootstrapping behavior remains separate from SDP timing/reference parsing.

### tests/video_sdp_fmtp_timing_parameters_test.cpp
- Роль:
    - проверяет known ST 2110 timing/sender parameters inside `a=fmtp`:
        - TP;
        - TROFF;
        - CMAX;
        - TSMODE;
        - TSDELAY.

### tests/video_sdp_maxudp_parameters_test.cpp
- Роль:
    - проверяет parsing/mapping of `MAXUDP` from SDP `a=fmtp`;
    - проверяет propagation into signaling and packet parse policy;
    - проверяет финальную policy semantics for absent / Standard / Extended values.
- Покрывает:
    - raw fmtp parser extracts `MAXUDP` as known parameter rather than leaving it in unknown parameters;
    - duplicate `MAXUDP` rejected;
    - malformed numeric `MAXUDP` rejected in raw fmtp parsing;
    - final SDP ingestion maps Standard UDP Size Limit `MAXUDP` into signaling;
    - final SDP ingestion maps Extended UDP Size Limit `MAXUDP` into signaling;
    - packet parse policy receives the final Standard / Extended effective limit from signaling;
    - absent `MAXUDP` preserves empty signaling/policy override and therefore Standard-by-default behavior;
    - final SDP ingestion rejects non-boundary numeric `MAXUDP` values via signaling validation;
    - final SDP ingestion rejects values above Extended UDP Size Limit via signaling validation.

### tests/video_sdp_depth_16f_test.cpp
- Роль:
    - проверяет SDP `depth=16f` parsing and signaling representation.

### tests/video_sdp_media_property_enum_coverage_test.cpp
- Роль:
    - проверяет explicit enum coverage for known ST 2110-20 SDP media-property tokens;
    - проверяет `Other + raw_token` для unknown future tokens.
- Покрывает:
    - explicit enum mapping for known `sampling` tokens;
    - explicit enum mapping for known `colorimetry` tokens;
    - explicit enum mapping for known `TCS` tokens;
    - explicit enum mapping for known `RANGE` tokens:
        - `NARROW`;
        - `FULLPROTECT`;
        - `FULL`;
    - preservation of unknown future `sampling` / `colorimetry` / `TCS` / `RANGE` tokens through `Other + raw_token`;
    - separation between explicit signaling-model enum coverage and runtime `PixelFormat` projection support.
- Фиксирует:
    - `FULLPROTECT` is now a first-class known `VideoRange` value rather than a fallback `Other` token;
    - unsupported runtime projection still remains localized outside the enum-mapping boundary.

### tests/video_sdp_optional_sender_timing_test.cpp
- Роль:
    - проверяет relaxed receiver-side optional sender timing validation for SDP ingestion.

### tests/video_sdp_transport_boundary_test.cpp
- Роль:
    - проверяет preservation boundary for raw SDP transport/redundancy metadata around the selected video media section.
- Покрывает:
    - preservation of session-level and media-level `c=` data in the selected raw media section;
    - preservation of `mid`, `a=source-filter`, and `a=group:DUP`;
    - separate preservation of unknown session/media attributes;
    - proof that preserved raw transport metadata does not break final video signaling ingestion;
    - rejection of duplicate media/session connection-data lines;
    - rejection of duplicate `mid`;
    - proof that transport metadata from other media sections is not leaked into the selected video section.
- Фиксирует:
    - raw transport/redundancy SDP metadata remains separate from final `VideoStreamSignaling`;
    - detailed `c=` structural validation itself is covered separately by `video_sdp_connection_data_test.cpp`.

### tests/video_sdp_media_cross_field_validation_test.cpp
- Роль:
    - проверяет ST 2110-20 media-description cross-field validation:
        - progressive-only `4:2:0` variants;
        - `KEY + ALPHA`;
        - rejection of invalid KEY/TCS/colorimetry combinations;
        - tightened `SSN` cross-field rule;
        - tightened `RANGE` cross-field rule.
- Покрывает:
    - `4:2:0` accepted only with progressive scan signaling;
    - `KEY` requires `colorimetry=ALPHA` and forbids `TCS`;
    - `BT709 + SDR + SSN=ST2110-20:2017` accepted;
    - `BT709 + SDR + SSN=ST2110-20:2022` rejected;
    - `ALPHA` requiring `SSN=ST2110-20:2022`;
    - `TCS=ST2115LOGS3` requiring `SSN=ST2110-20:2022`;
    - `BT2100 + RANGE=FULL` accepted;
    - `BT2100 + RANGE=FULLPROTECT` rejected;
    - non-BT2100 `RANGE=FULLPROTECT` accepted;
    - absent `RANGE` remains accepted at signaling level;
    - unknown future `RANGE` token preserved through `Other + raw_token`.
    - the same cross-field rules through final SDP ingestion, not only manually constructed signaling objects;
    - unsupported runtime projection remaining localized after structurally valid KEY/ALPHA acceptance.

### tests/video_sdp_source_filter_scope_test.cpp
- Роль:
    - проверяет raw SDP `a=source-filter` grammar boundary in `video_sdp_media_section.hpp`;
    - проверяет preservation of parsed source-filter fields together with explicit session/media scope;
    - проверяет, что source-filter remains transport metadata and does not change final signaling/runtime behavior.
- Покрывает:
    - valid session-level source-filter parsing and preservation:
        - raw value;
        - scope;
        - filter mode;
        - nettype / addrtype;
        - destination address;
        - source address list.
    - valid media-level source-filter parsing and preservation.
    - simultaneous session-level + media-level source-filter parsing with preserved scope distinction.
    - invalid filter-mode rejection.
    - rejection of missing destination/source fields.
    - rejection of malformed packed source-list forms:
        - comma-separated packed addresses;
        - semicolon-separated packed addresses;
        - trailing packed separators inside one token.
    - final SDP ingestion/runtime boundary remains untouched:
        - valid SDP with source-filter still ingests into `VideoStreamSignaling`;
        - source-filter does not alter video media/signaling projection behavior.
- Фиксирует:
    - tightened source-filter grammar validation remains localized in the raw SDP media-section parser;
    - source-filter continues to live outside `VideoStreamSignaling` and outside backend/socket behavior in the current task.

### tests/video_sdp_redundancy_boundary_test.cpp
- Роль:
    - проверяет raw redundancy boundary:
        - `a=group:DUP`;
        - `a=mid`;
        - duplicate video media-section candidates.

### tests/video_sdp_fmtp_strict_parsing_test.cpp
- Роль:
    - проверяет strict SDP `a=fmtp` media-parameter parsing:
        - required separator whitespace;
        - whitespace around `=`;
        - doubled/trailing separators;
        - unknown syntactically valid parameters.

### tests/video_sdp_timing_scope_test.cpp
- Роль:
    - проверяет session/media scope resolution for standalone SDP timing/reference-clock attributes and their interaction with fmtp timing/sender media parameters.
- Покрывает:
    - preservation of session-level `ts-refclk` / `mediaclk` in the raw SDP model;
    - media-level override of session-level `ts-refclk` / `mediaclk`;
    - rejection of duplicate standalone timing attributes within the same scope;
    - explicit separation between:
        - standalone timing parsing in `video_sdp_timing_attributes.hpp`;
        - fmtp timing/sender parsing in `video_sdp_fmtp.hpp`;
        - final merge/conflict handling in `video_sdp_ingestion.hpp`.
    - `TSMODE` from fmtp overriding session-level standalone `tsmode`;
    - conflict rejection when fmtp timing field duplicates a media-level standalone timing field;
    - standards-clean final-ingestion requirement that `mediaclk` must be media-level;
    - existing media-level-only SDP timing behavior for `ts-refclk`, `mediaclk`, `tsmode`, `tsdelay`, `TROFF`, `CMAX`, and `TP=2110TPW`.
- Фиксирует:
    - `parse_video_sdp_timing_attributes(...)` parses standalone SDP timing attributes only;
    - `TP` from `a=fmtp` is not expected to appear inside the standalone raw timing model and is merged only at final SDP ingestion;
    - final signaling still carries the correct `sender_type` after fmtp merge.

### tests/video_sdp_connection_data_test.cpp
- Роль:
    - focused regression/acceptance coverage for raw SDP `c=` connection-data structural validation in `video_sdp_media_section.hpp`.
- Покрывает:
    - valid unicast connection-data parsing:
        - `c=IN IP4 192.0.2.10`;
    - valid IPv4 multicast parsing:
        - `c=IN IP4 239.1.1.1/32`;
        - `c=IN IP4 239.1.1.1/32/4`;
    - rejection of malformed raw connection-data structure:
        - empty base address;
        - empty TTL;
        - non-numeric TTL;
        - out-of-range TTL;
        - empty address count;
        - non-numeric address count;
        - zero address count;
        - too many slash parameters.
    - preservation of existing session/media `c=` behavior through `select_raw_video_sdp_media_section(...)`;
    - rejection of malformed session-level and media-level `c=` lines during media-section parsing.
- Фиксирует:
    - raw SDP `c=` remains transport metadata outside `VideoStreamSignaling`;
    - structural validation is now tighter, but session/media preservation behavior remains unchanged.

## Audio frame storage

### tests/test_audio_frame.cpp
- Роль:
    - проверяет initial `AudioBuffer` / `AudioFrameView` contract.
    - покрывает:
        - construction from explicit audio dimensions;
        - construction from `RxAudioConfig`;
        - current MVP storage layout `InterleavedS32`;
        - interleaved sample indexing by `(sample_index, channel)`;
        - mutable and const sample access;
        - total sample count;
        - sample-frame stride;
        - byte size;
        - timestamp propagation into `AudioFrameView`;
        - out-of-range sample/channel access rejection.
    - фиксирует separation between:
        - audio storage layout;
        - runtime `RxAudioConfig` validation;
        - channel-order / channel-mapping semantics;
        - future audio RTP packet assembly and backend behavior.

## Audio signaling model

### tests/audio_signaling_to_rx_config_test.cpp
- Роль:
    - проверяет projection from `AudioStreamSignaling` to `RxAudioConfig`.
    - фиксирует, что runtime projection keeps the current receiver-support boundary separate from structural signaling validity.
- Покрывает:
    - public projection helper shape:
        - `rx_audio_config_from_audio_stream_signaling(...) -> std::expected<RxAudioConfig, Error>`.
    - successful projection of current Level A-oriented signaling:
        - `sampling_rate_hz`;
        - `packet_time_us`;
        - derived `samples_per_packet`;
        - `channel_count`;
        - `udp_port`;
        - `payload_type`;
        - `local_ip`;
        - `dest_ip`;
        - `format`;
        - `pcm_bit_depth`.
    - Level A min/max channel-count projection:
        - `1` channel accepted;
        - `8` channels accepted;
        - empty local IP preserved as valid.
    - projection with valid channel-order signaling:
        - SMPTE 2110 channel-order signaling accepted without altering runtime channel-count projection.
    - bit-depth projection:
        - `L24` path preserved as `Bits24`;
        - `L16` path preserved as `Bits16`.
    - explicit separation between structural signaling validity and runtime support:
        - structurally valid `96000 Hz` signaling rejected by runtime projection as `Unsupported`;
        - structurally valid `125 us` packet-time signaling rejected by runtime projection as `Unsupported`;
        - structurally valid `9-channel` signaling rejected by runtime projection as `Unsupported`.
    - runtime transport/config validation after signaling projection:
        - UDP port `0` rejected as `InvalidValue`;
        - non-dynamic RTP payload type rejected as `InvalidValue`;
        - empty destination IP rejected as `InvalidValue`.
    - runtime sample-format support boundary:
        - unsupported runtime sample format rejected as `Unsupported`.
- Фиксирует:
    - `AudioStreamSignaling` may remain structurally valid outside the current Level A-oriented receiver baseline.
    - rejection of unsupported sample rate / packet time / channel count now happens at `RxAudioConfig` runtime projection/support validation, not at the structural signaling boundary.
    - `samples_per_packet` remains derived from signaling rate and packet time instead of being hardcoded.

### tests/audio_signaling_model_test.cpp
- Роль:
    - проверяет modeled `AudioStreamSignaling` validation.
    - фиксирует разделение между:
        - structural audio signaling validation;
        - explicit receiver/conformance support validation against supported conformance ranges.
- Покрывает:
    - базовые modeled audio enums / helper signatures:
        - `AudioConformanceLevel`;
        - `AudioPcmEncoding`;
        - `AudioChannelOrderConvention`;
        - `validate_audio_conformance_range(...)`;
        - `audio_media_description_matches_conformance_range(...)`;
        - `validate_audio_media_description_against_conformance_range(...)`;
        - `validate_audio_stream_signaling(...)`;
        - `validate_audio_stream_signaling_against_conformance_ranges(...)`.
    - `audio_level_a_receiver_baseline()`:
        - `48 kHz`;
        - `1000 us`;
        - `1..8 channels`.
    - structural validation of audio conformance ranges:
        - zero sampling rate rejected;
        - zero packet time rejected;
        - zero min channel count rejected;
        - reversed channel range rejected;
        - invalid conformance level enum rejected.
    - structural signaling acceptance:
        - valid Level A stereo accepted;
        - valid min/max Level A channel counts accepted;
        - zero channel count rejected;
        - invalid PCM encoding rejected.
    - explicit separation of structural signaling from current receiver-support boundary:
        - `96000 Hz` signaling remains structurally valid;
        - `125 us` packet-time signaling remains structurally valid;
        - `9-channel` signaling remains structurally valid;
        - the same cases fail only through conformance-range validation as `Unsupported`.
    - `validate_audio_media_description_against_conformance_range(...)` behavior:
        - matching Level A media accepted;
        - structurally valid but non-matching media rejected as `Unsupported`.
    - `validate_audio_stream_signaling_against_conformance_ranges(...)` behavior:
        - matching Level A signaling accepted;
        - empty supported-range set rejected as `Unsupported`;
        - structurally invalid signaling still rejected as `InvalidValue`.
    - audio channel-order signaling validation:
        - valid SMPTE 2110 channel-order accepted;
        - declared SMPTE 2110 channel count must not exceed actual channel count;
        - empty or malformed SMPTE 2110 value rejected;
        - `Unspecified` with empty raw value accepted;
        - `Unspecified` with non-empty raw value rejected;
        - `Other` with non-empty raw value accepted;
        - `Other` with empty raw value rejected;
        - invalid channel-order convention enum rejected.
- Фиксирует:
    - generic `validate_audio_stream_signaling(...)` is structural-only and no longer hardcodes the current Level A receiver-support boundary.
    - current support limits remain explicit through `validate_audio_stream_signaling_against_conformance_ranges(...)` instead of being collapsed into the signaling/model boundary.

### tests/audio_channel_order_boundary_test.cpp
- Роль:
    - проверяет modeled ST 2110-30 audio channel-order boundary.
    - покрывает:
        - parsing of `SMPTE2110.(...)` channel-order values;
        - known grouping symbols:
            - `M`;
            - `DM`;
            - `ST`;
            - `LtRt`;
            - `51`;
            - `71`;
            - `222`;
            - `SGRP`;
            - `U01`..`U64`;
        - rejection of malformed `Uxx` groups and unsupported symbols;
        - declared channel-count accumulation;
        - rejection when declared SMPTE2110 channel count exceeds stream channel count;
        - absent / unspecified channel-order becoming an effective Undefined group;
        - under-declared SMPTE2110 channel-order appending an Undefined remainder group;
        - separation of channel-order parsing/effective grouping from runtime audio buffer layout and channel reordering.

### tests/audio_receiver_bootstrap_test.cpp
- Роль:
    - проверяет audio receiver bootstrap composition boundary.
    - покрывает:
        - projection from `AudioStreamSignaling` to runtime `RxAudioConfig`;
        - effective channel-order derivation in the same bootstrap result;
        - absent channel-order becoming one Undefined group;
        - exact SMPTE2110 channel-order preservation;
        - under-declared SMPTE2110 channel-order appending an Undefined remainder group;
        - `Other` channel-order convention preservation as an unknown/future convention;
        - rejection of over-declared SMPTE2110 channel-order;
        - rejection of invalid runtime transport fields:
            - UDP port;
            - RTP payload type;
            - destination IP;
        - rejection of unsupported runtime audio sample format.
    - фиксирует, что bootstrap layer composes existing signaling/runtime/channel-order boundaries and does not implement audio buffer layout, channel reordering, packet pipeline, or backend behavior.

## Audio SDP parsing / signaling adapter / ingestion

### tests/audio_sdp_media_section_test.cpp
- Роль:
    - проверяет raw audio SDP media-section selection boundary для `select_raw_audio_sdp_media_section(...)`.
    - фиксирует current raw parsing/selection behavior для:
        - `m=audio` media-line selection по expected payload type;
        - `a=rtpmap`;
        - `a=ptime`;
        - `a=fmtp`;
        - preservation of unknown session/media attributes.
- Покрывает:
    - compile-time contract:
        - `select_raw_audio_sdp_media_section(...)` returns `std::expected<RawAudioSdpMediaSection, Error>`.
    - positive raw media-section selection:
        - selected section preserves `media_line`, `payload_type`, and `media_payload_types`;
        - `a=rtpmap` is parsed into `encoding_name`, `sampling_rate_hz`, and optional `channel_count`;
        - `a=ptime:1` maps to `packet_time_us == 1000`;
        - `a=ptime:0.125` maps to `packet_time_us == 125`;
        - `a=fmtp` `channel-order=...` is parsed into `parsed_fmtp.channel_order` and mirrored to `channel_order`;
        - unknown fmtp parameters are preserved in `parsed_fmtp.unknown_parameters`;
        - unknown session-level attributes are preserved in `unknown_session_attributes`;
        - unknown media-level attributes are preserved in `unknown_attributes`;
        - raw `c=` line inside the selected media section is preserved in `unknown_attributes`.
    - payload-type-driven media selection:
        - correct audio media section is selected when SDP contains both video and audio sections;
        - selected audio section may contain multiple payload types, and the expected one must be present in `media_payload_types`.
    - raw rtpmap tolerance currently modeled at this boundary:
        - `L24/48000` without explicit channel count is accepted at raw-media-section level and leaves `parsed_rtpmap.channel_count` absent.
    - raw channel-order boundary behavior:
        - `channel-order` inside `a=fmtp` is parsed;
        - standalone `a=channel-order:...` is not treated as the modeled fmtp field and is preserved as an unknown media attribute.
    - negative cases:
        - no matching selected payload type in `m=audio` is rejected;
        - missing required payload-bound `a=rtpmap` is rejected;
        - duplicate payload-bound `a=rtpmap` is rejected;
        - duplicate `a=ptime` is rejected;
        - duplicate payload-bound `a=fmtp` is rejected;
        - duplicate `channel-order` inside one fmtp payload is rejected;
        - malformed `rtpmap` sampling rate is rejected;
        - zero audio channel count in `rtpmap` is rejected;
        - extra slash components in `rtpmap` are rejected;
        - invalid `ptime` values such as `0` and over-precise `0.0001` are rejected.
- Фиксирует:
    - raw audio SDP media-section parsing remains strict and fail-fast on malformed selected-section attributes;
    - unknown/non-modeled attributes are preserved rather than silently coerced into modeled fields;
    - raw media-section selection stays separated from later signaling/runtime mapping layers.

### tests/audio_sdp_signaling_adapter_test.cpp
- Роль:
    - проверяет final raw SDP audio media-section ingestion into `AudioStreamSignaling`.
    - фиксирует, что raw SDP -> signaling adapter now builds structurally valid audio signaling independently from the current Level A receiver-support boundary.
- Покрывает:
    - public adapter shape:
        - `audio_stream_signaling_from_raw_audio_sdp_media_section(...) -> std::expected<AudioStreamSignaling, Error>`.
    - successful SDP ingestion for valid PCM audio SDP:
        - `a=rtpmap` parsing for `L24` and `L16`;
        - `a=ptime` parsing into microseconds;
        - channel-count extraction from `rtpmap`;
        - mapping to `AudioPcmEncoding::LinearPcm`;
        - mapping to `AudioPcmBitDepth::Bits24` / `Bits16`.
    - successful channel-order ingestion:
        - `a=fmtp ... channel-order=SMPTE2110.(...)` mapped to `AudioChannelOrderConvention::Smpte2110`;
        - vendor/other `channel-order` mapped to `AudioChannelOrderConvention::Other`.
    - structural signaling validation after SDP ingestion:
        - standard Level A SDP accepted structurally;
        - explicit validation against supported Level A conformance ranges accepted for matching cases.
    - corrected raw SDP -> signaling behavior for structurally valid but currently unsupported audio streams:
        - `L24/96000/2` with `ptime:1` now ingests successfully as structurally valid signaling;
        - `L24/48000/2` with `ptime:0.125` now ingests successfully as structurally valid signaling;
        - both cases are rejected only by explicit conformance-range validation as `Unsupported`.
    - unsupported non-PCM encoding handling:
        - `OPUS` rejected as `Unsupported`.
    - strict malformed-SDP rejection:
        - missing `ptime` rejected as `InvalidValue`;
        - missing channel count in `rtpmap` rejected as `InvalidValue`.
    - raw SDP attribute-boundary behavior:
        - absent `channel-order` remains explicitly representable;
        - standalone unknown `a=channel-order:...` attribute is preserved in `unknown_attributes` and does not populate the parsed `channel_order` field.
- Фиксирует:
    - SDP ingestion no longer collapses structural audio signaling validity into the current Level A-oriented receiver-support boundary.
    - explicit support rejection remains available only through `validate_audio_stream_signaling_against_conformance_ranges(...)`, not through the generic SDP-to-signaling adapter.
    - malformed SDP is still rejected strictly and independently from support-policy decisions.

### tests/audio_sdp_ingestion_test.cpp
- Роль:
    - проверяет final audio SDP ingestion entry point `parse_audio_stream_signaling_from_sdp(...)`.
    - фиксирует, что standards-aware final audio ingestion больше не опирается на attribute-name-only presence для clock signaling, а использует dedicated parsed timing boundary for `ts-refclk` and media-level `mediaclk`.
- Покрывает:
    - successful final ingestion of valid Level A raw audio SDP into `AudioStreamSignaling`:
        - `L24/48000/2`;
        - `ptime:1`;
        - valid `ts-refclk:ptp=...`;
        - media-level `mediaclk:direct=0`;
        - optional `channel-order=SMPTE2110.(ST)`.
    - final mapped signaling values:
        - `AudioPcmEncoding::LinearPcm`;
        - `sampling_rate_hz == 48000`;
        - `packet_time_us == 1000`;
        - expected channel count;
        - optional SMPTE 2110 channel-order mapping.
    - existing final-ingestion rejection paths independent from timing parsing:
        - selected payload-type mismatch rejected;
        - missing required payload-bound `a=rtpmap` rejected;
        - invalid `ptime` rejected;
        - unsupported audio encoding such as `AM824` rejected as `Unsupported`.
    - required ST 2110 clock-signaling presence at final ingestion:
        - missing `ts-refclk` rejected;
        - missing media-level `mediaclk` rejected;
        - session-level-only `mediaclk` remains insufficient and rejected.
    - new strict timing parsing boundary behavior:
        - malformed known `ts-refclk` form is rejected even when the attribute name is present;
        - malformed media-level `mediaclk` known form is rejected even when the attribute name is present.
    - open-ended/future timing signaling tolerance:
        - unknown non-empty `ts-refclk` form is accepted through the parsed open-ended reference-clock path;
        - unknown non-empty media-level `mediaclk` form is accepted through the parsed open-ended media-clock path.
    - raw unknown-attribute preservation remains separate from final signaling mapping:
        - unknown session/media attributes are preserved by raw media-section selection;
        - they do not interfere with successful final mapping when required known signaling is valid.
- Фиксирует:
    - final audio SDP ingestion now depends on parsed/validated timing objects rather than raw attribute-name presence.
    - malformed known ST 2110 clock-signaling forms fail at the audio SDP timing boundary instead of passing accidentally into final ingestion.
    - standards-clean requirement for media-level `mediaclk` remains explicit and localized at the final audio SDP ingestion boundary.

## Audio packet model

### tests/test_audio_packet.cpp
- Роль:
    - проверяет audio RTP packet policy/view boundary.
    - покрывает:
        - explicit wire-sample byte sizes for `AudioPcmWireFormat::{L16, L24}`;
        - derivation of `AudioPcmWireFormat` from `RxAudioConfig::pcm_bit_depth`;
        - preservation of runtime axes in `AudioRtpPacketPolicy`;
        - payload-size calculation for both `L24` and `L16`;
        - rejection of inconsistent `samples_per_packet`;
        - packet-view construction with matching RTP payload type and payload size;
        - rejection of payload-type mismatch;
        - rejection of payload-size mismatch.
    - фиксирует:
        - packet policy no longer takes a backend-supplied temporary wire-format override;
        - runtime bit depth now drives RTP wire-format policy explicitly.

### tests/test_audio_rtp_parser.cpp
- Роль:
    - проверяет full audio RTP parser path on top of `AudioRtpPacketPolicy`.
    - покрывает:
        - valid `L24` audio RTP packet parsing;
        - valid `L16` audio RTP packet parsing;
        - preservation of parsed `wire_format` in `AudioRtpPacketView`;
        - RTP payload extraction with CSRC + header extension present;
        - payload-type mismatch rejection;
        - payload-size mismatch rejection;
        - short RTP packet rejection;
        - bad RTP version rejection before audio-payload policy acceptance.

## Audio reorder / jitter MVP

### tests/test_audio_reorder_buffer.cpp
- Роль:
    - проверяет `AudioFixedWindowReorderBuffer` как concrete audio reorder implementation поверх `AudioRtpPacketView`.
- Покрывает:
    - in-order push/pop behavior;
    - out-of-order acceptance within reorder window and ordered pop;
    - duplicate rejection without overwriting originally stored payload;
    - single-gap advance through `flush_missing_once()`;
    - late-packet rejection after progress;
    - out-of-window rejection for configured reorder window;
    - `reset()` clearing pending packets and sequence state;
    - 16-bit RTP sequence wraparound handling;
    - stats accounting:
        - `packets_pushed`;
        - `packets_popped`;
        - `duplicates`;
        - `late_packets`;
        - `out_of_window`;
        - `missing_packets_flushed`.
- Фиксирует:
    - audio reorder behavior remains aligned with the modeled fixed-window receive policy for audio RTP packets;
    - stored audio payload/view integrity survives reordering decisions and later pop.

## Audio frame/block assembly MVP

### tests/test_audio_frame_assembler.cpp
- Роль:
    - проверяет audio packet-to-block assembly into current MVP storage layout `InterleavedS32`.
    - покрывает:
        - `L24` wire-sample decode into signed 32-bit interleaved storage;
        - `L16` wire-sample decode into signed 32-bit interleaved storage;
        - non-stereo / non-48-sample block handling without hardcoded assumptions;
        - payload-size mismatch rejection;
        - invalid packet-shape rejection;
        - stats accounting and `reset()` reusability.
    - фиксирует:
        - current storage layout remains separate from RTP wire-format width;
        - explicit `AudioPcmWireFormat` survives far enough to drive actual sample decoding.

### tests/test_audio_stats.cpp
- Роль:
    - проверяет shared audio receive stats boundary introduced for task `094`.
- Покрывает:
    - default `AudioReceiveStats` counters are initialized to zero;
    - packet helper functions increment the correct counters:
        - `record_audio_packet_ok(...)`;
        - `record_audio_packet_lost(...)`;
        - `record_audio_packet_rejected(...)`.
    - `record_audio_block_result(...)` increments the correct block counters for:
        - `AudioBlockCompletionStatus::Complete`;
        - `AudioBlockCompletionStatus::Partial`;
        - `AudioBlockCompletionStatus::Dropped`.
    - invalid block-completion enum values are rejected and do not mutate stats;
    - `reset_audio_receive_stats(...)` clears all counters.
- Фиксирует:
    - audio stats accounting remains a standalone boundary;
    - stats helpers do not embed RTP parsing, reorder/jitter, audio assembly, timestamp mapping, playout policy, channel-order mapping, socket backend, or MTL backend behavior.

### tests/audio_timestamp_mapping_test.cpp
- Роль:
    - focused unit test для `audio_timestamp_mapping.hpp`.
    - проверяет config validation, RTP-tick-to-nanoseconds conversion, mapper behavior in both initial-anchor modes, reset semantics, и audio playout-timing helpers.
- Покрывает:
    - `validate_audio_rtp_timestamp_mapper_config(...)`:
        - valid `ConfiguredReference` config accepted;
        - zero RTP clock rate rejected;
        - `FirstObservedBecomesLocalZero` with non-zero anchor fields rejected.
    - `audio_rtp_ticks_to_timestamp_ns(...)`:
        - exact conversion for `48 kHz` and `96 kHz`;
        - zero clock rate rejection;
        - overflow rejection.
    - `forward_audio_rtp_timestamp_delta(...)`:
        - normal forward delta;
        - wraparound forward delta;
        - half-range ambiguous delta rejection;
        - backward delta rejection.
    - `AudioRtpTimestampMapper` in `ConfiguredReference` mode:
        - anchor RTP timestamp maps to configured anchor nanoseconds;
        - subsequent packets map by RTP delta.
    - `AudioRtpTimestampMapper` in `FirstObservedBecomesLocalZero` mode:
        - first observed RTP timestamp maps to `0 ns`;
        - subsequent packets map by delta from the first observed packet.
    - wraparound mapping in both modes.
    - invalid mapping behavior:
        - backward packet rejected;
        - invalid clock-rate config rejected;
        - anchor-plus-offset overflow rejected.
    - `reset(...)` semantics in both modes:
        - explicit configured reanchor;
        - explicit first-observed-local-zero reanchor.
    - audio playout timing helpers:
        - `audio_receiver_playout_timing_decision(...)`;
        - overflow rejection for playout timing;
        - `audio_block_timing(...)` remains independent from assembler/runtime code.
- Фиксирует:
    - audio timestamp mapper API now explicitly models initial anchoring policy;
    - default/manual path no longer relies on an unexplained hidden `0/0` anchor artifact;
    - playout timing remains layered above media timestamp mapping rather than fused with it.

### tests/audio_timestamp_mapping_invariants_test.cpp
- Роль:
    - invariants/regression test для audio RTP timestamp mapping boundary и его взаимодействия с receiver-side playout timing.
    - фиксирует две явные политики initial anchoring:
        - `ConfiguredReference`;
        - `FirstObservedBecomesLocalZero`.
- Покрывает:
    - monotonic 48 kHz / 1 ms cadence in `ConfiguredReference` mode:
        - exact 1 ms step;
        - strict monotonic growth over many packets.
    - monotonic 48 kHz / 1 ms cadence in `FirstObservedBecomesLocalZero` mode:
        - first observed RTP timestamp maps to local `0 ns`;
        - subsequent packets preserve exact 1 ms cadence.
    - explicit non-default RTP clock-rate support:
        - `96 kHz` cadence uses the configured clock rate rather than hidden `48 kHz` assumptions.
    - wraparound behavior in both modes:
        - continuity across 32-bit RTP timestamp wraparound;
        - no cadence break after wraparound.
    - long-running stream behavior beyond one 32-bit RTP epoch in both modes:
        - accumulated nanoseconds continue from RTP-domain deltas;
        - mapping remains monotonic and larger than one full 32-bit epoch worth of ticks.
    - rejection behavior:
        - backward delta rejected;
        - exact half-range ambiguous delta rejected;
        - failed packet does not advance mapper state.
    - reset semantics in both modes:
        - `ConfiguredReference` reset reanchors to the new explicit configured anchor;
        - `FirstObservedBecomesLocalZero` reset discards old cadence state and makes the next observed RTP timestamp the new local zero.
    - playout timing overlay:
        - `audio_block_timing(...)` preserves media cadence and applies constant playout delay independently from the RTP timestamp mapper mode.
- Фиксирует:
    - audio timestamp mapping no longer has a single hidden `0/0` anchor convention;
    - both anchoring policies are explicit and stable;
    - long-running / wraparound continuity remains correct after the architecture change.


### tests/test_socket_rx_video_backend.cpp
- Роль:
    - end-to-end unit/integration-style coverage для `SocketRxVideoBackend` over fake socket port/factory and fake video sink.
- Покрывает:
    - compile-time/backend contract:
        - backend is `final`;
        - concrete backend implements `ISocketRxVideoBackend` and `IRxBackend`;
        - concrete backend does not expose manual `IRxVideoBackend` start directly;
        - operational `start_video(const SocketRxVideoOperationalConfig&, ...)` stays the concrete start boundary;
        - `SocketRxVideoBackendFactory` type shape.
    - operational-config validation:
        - fully consistent operational config is accepted;
        - mismatched projected open config vs `RxVideoConfig` is rejected;
        - mismatched receive-pipeline config vs `RxVideoConfig` is rejected;
        - invalid reorder tolerance policy is rejected.
    - lifecycle/runtime behavior:
        - stop-before-start is accepted;
        - repeated stop remains accepted;
        - successful operational start opens the socket port with projected open config;
        - repeated start while active returns `InvalidBackendState`;
        - null created port is rejected without opening.
    - receive/delivery path:
        - one valid ST 2110-20 datagram produces one delivered `VideoFrameView`;
        - payload bytes are copied correctly into frame storage;
        - first-observed RTP timestamp can map to local zero;
        - configured-reference timestamp mode can produce a non-zero delivered timestamp.
    - reorder-tolerance behavior on the socket receive path:
        - `WaitForMissing` blocks delivery across a gap and records missing-gap state without flushing;
        - `FlushGapOnce` allows one single-gap advance so the next unit can continue through the receive path.
    - backend stats surface:
        - datagrams received;
        - parsed/rejected packets;
        - delivered frames/media units;
        - reorder pushed/popped/missing counters.
- Фиксирует:
    - concrete socket video backend consumes only the operational-config boundary;
    - receive-loop behavior, timestamp mapping, and reorder tolerance remain observable through backend stats rather than sink-side side channels.

### tests/test_socket_runtime_interface.cpp
- Роль:
    - проверяет OS-neutral socket runtime boundary shape, validation, and config projection behavior.
- Покрывает:
    - `SocketAddressFamily` helpers;
    - endpoint validation;
    - multicast membership validation:
        - valid IPv4/IPv6 multicast groups;
        - optional empty interface address;
        - valid explicit interface address in the same family;
        - invalid interface-address syntax;
        - family-mismatched interface address;
        - rejection of unicast group addresses.
    - `SocketRxOpenConfig` validation and `socket_rx_uses_multicast(...)`;
    - `socket_rx_open_config_from_video_config(...)`:
        - IPv4/IPv6 unicast projection;
        - IPv4/IPv6 multicast projection;
        - multicast wildcard bind behavior;
        - propagation of explicit `RxVideoConfig.local_ip` into `multicast_membership.interface_address`.
    - abstract `ISocketRxPort` / `ISocketRxPortFactory` shape through fake implementations.
- Фиксирует:
    - multicast interface selection is modeled explicitly inside the runtime boundary;
    - backend-facing `RxVideoConfig.local_ip` now maps to multicast membership interface selection rather than overriding multicast wildcard bind semantics.

### tests/test_linux_socket_rx_port.cpp
- Роль:
    - проверяет concrete Linux socket receive-port lifecycle and failure mapping against the existing socket runtime boundary.
- Покрывает:
    - IPv4 unicast open/close/repeated-open behavior;
    - IPv6 unicast open/close when loopback IPv6 is available;
    - IPv4 multicast open/close/repeated-close behavior;
    - invalid open requests;
    - localized rejection of IPv6 multicast as `Unsupported`;
    - bind failure mapping to `BindFailed`;
    - IPv4 multicast join failure mapping to `MulticastJoinFailed`;
    - cleanup after failed multicast join so the bound port can be reopened successfully;
    - basic raw UDP receive contract and factory shape.
- Фиксирует:
    - multicast join/leave is implemented in the Linux socket runtime layer, not in backend code;
    - failed multicast join remains transactional and does not leak an already-bound native socket into later opens;
    - unsupported family branches stay localized in Linux runtime behavior.

### tests/test_socket_rx_audio_backend.cpp
- Роль:
    - end-to-end unit/integration-style coverage для `SocketRxAudioBackend` over fake socket port/factory and capturing audio sink.
- Покрывает:
    - compile-time/backend contract:
        - backend is `final`;
        - concrete backend implements `ISocketRxAudioBackend` and `IRxBackend`;
        - concrete backend does not expose manual `IRxAudioBackend` start directly;
        - operational `start_audio(const SocketRxAudioOperationalConfig&, ...)` stays the concrete start boundary;
        - `SocketRxAudioBackendFactory` type shape.
    - operational-config validation:
        - fully consistent operational config is accepted;
        - mismatched audio packet policy vs `RxAudioConfig` is rejected;
        - invalid reorder-buffer config is rejected;
        - invalid reorder tolerance policy is rejected;
        - mismatched timestamp-mapper config vs `RxAudioConfig` is rejected.
    - lifecycle/runtime behavior:
        - stop-before-start is accepted;
        - repeated stop remains accepted;
        - successful operational start opens the socket port with projected open config;
        - repeated start while active returns `InvalidBackendState`;
        - null created port is rejected without opening.
    - receive/delivery path:
        - one valid audio RTP datagram produces one delivered audio block;
        - L24 payload is unpacked into delivered samples correctly;
        - first-observed RTP timestamp can map to local zero;
        - configured-reference timestamp mode can produce a non-zero delivered timestamp.
    - reorder-tolerance behavior on the socket receive path:
        - `WaitForMissing` stalls delivery on the first gap and records missing-gap state;
        - `FlushGapOnce` allows a single-gap advance for subsequent delivery.
    - backend stats surface:
        - datagrams received;
        - parsed/rejected packets;
        - delivered media units;
        - reorder pushed/popped/missing counters.
- Фиксирует:
    - concrete socket audio backend consumes only the operational-config boundary;
    - audio receive behavior keeps packet policy, timestamp mapping, and reorder tolerance explicit and observable at backend level rather than through sink-side side effects.

### tests/test_video_packet_admission.cpp
- Роль:
    - regression tests для explicit RTP payload-type admission boundary in the video receive path.
    - проверяет, что payload-type admission remains separate from generic RTP/ST 2110-20 parsing and that wrong-PT packets are ignored before reorder/depacketizer use.
- Покрывает:
    - helper-level admission boundary:
        - matching dynamic RTP payload type accepted by `validate_rtp_payload_type_admission(...)`;
        - matching video packet accepted by `validate_video_packet_payload_type_admission(...)`;
        - mismatching payload type rejected as `InvalidValue`.
    - separation of concerns:
        - generic `PacketView` parsing still succeeds for structurally valid RTP/ST 2110-20 packets regardless of whether the payload type matches the configured stream;
        - payload-type admission remains a separate stream-specific boundary above generic packet parsing.
    - runtime backend behavior with the new operational start boundary:
        - `SocketRxVideoBackend` is started from `SocketRxVideoOperationalConfig`, not manual `RxVideoConfig`;
        - wrong-payload-type packet is treated as non-media datagram and dropped locally;
        - wrong-PT packet does not enter reorder/depacketizer state;
        - no frame is delivered to the sink;
        - backend stats record:
            - one received datagram;
            - one ignored non-media datagram;
            - zero parsed-ok media packets;
            - zero rejected media packets;
            - zero depacketizer/reorder activity.
- Фиксирует:
    - payload-type admission is an explicit receive-path boundary distinct from generic RTP parsing;
    - wrong-PT packets are ignored before reorder/depacketizer mutation even after the backend’s move to the operational-only socket start API.

### tests/test_socket_rx_operational_architecture.cpp
- Роль:
    - architecture regression test для socket operational boundary over `SocketRxSingleMediaBackendBase`, signaling/bootstrap-to-operational adapters, and manual-to-operational adapters for both video and audio.
- Покрывает:
    - `SocketRxSingleMediaBackendBase` remains a media-agnostic common socket runtime base over `IRxBackend` and does not become a concrete video/audio operational interface.
    - receive-buffer sizing derived from `PacketParsePolicy`:
        - Standard UDP datagram size limit path;
        - Extended UDP datagram size limit path.
    - video bootstrap -> operational adapter preserves modeled axes and derived runtime inputs:
        - packet-parse policy;
        - `RxVideoConfig`;
        - `VideoScanMode`;
        - `VideoPackingMode`;
        - partial-frame policy;
        - timestamp-mapper config;
        - reorder window and reorder gap policy;
        - projected socket open config.
    - video manual -> operational adapter preserves explicit runtime inputs, including non-default reorder gap policy.
    - audio bootstrap -> operational adapter preserves modeled axes and derived runtime inputs:
        - packet-parse policy;
        - audio packet policy;
        - frame-assembler storage format;
        - reorder window and reorder gap policy;
        - timestamp-mapper config;
        - parsed channel order;
        - projected socket open config.
    - audio manual -> operational adapter preserves explicit runtime inputs, including non-default reorder gap policy.
- Фиксирует:
    - socket runtime architecture keeps one common media-agnostic operational base layer;
    - manual config path and signaling/bootstrap path converge into one operational-config boundary without dropping modeled axes such as reorder tolerance policy.

### tests/audio_sdp_timing_attributes_test.cpp
- Роль:
    - focused unit test for the dedicated raw audio SDP timing/reference-clock parsing boundary in `audio_sdp_timing_attributes.hpp`.
    - проверяет structural parsing, scope handling, and strict rejection rules for audio `ts-refclk` and `mediaclk`.
- Покрывает:
    - raw `ts-refclk` parsing:
        - known PTP form with explicit domain;
        - known PTP form without domain;
        - known `localmac=...` form;
        - preservation of unknown non-empty forms as `RawAudioSdpReferenceClock::Kind::Other`.
    - raw `ts-refclk` rejection of malformed known forms:
        - missing PTP GMID;
        - missing PTP version payload;
        - invalid PTP domain value.
    - raw `mediaclk` parsing:
        - known `direct=<u64>` form;
        - known `sender` form;
        - preservation of unknown non-empty forms as `RawAudioSdpMediaClock::Kind::Other`.
    - raw `mediaclk` rejection of malformed known forms:
        - malformed `direct=` numeric payload.
    - aggregate timing extraction from `RawAudioSdpMediaSection` preserved unknown attributes:
        - session-level `ts-refclk` parsing;
        - media-level `mediaclk` parsing;
        - `raw_audio_sdp_has_reference_clock(...)`;
        - `raw_audio_sdp_has_media_level_mediaclk(...)`.
    - scope-aware resolution policy:
        - media-level `mediaclk` overrides session-level `mediaclk`;
        - session-level-only `mediaclk` is parsed but is not treated as media-level presence.
    - strict duplicate handling:
        - duplicate session-level `ts-refclk` rejected;
        - duplicate media-level `mediaclk` rejected.
    - empty aggregate behavior:
        - absent timing attributes produce an empty parsed timing snapshot without synthetic defaults.
- Фиксирует:
    - audio timing/reference-clock parsing is now a dedicated strict boundary rather than an implicit name-only check.
    - malformed known timing forms are distinguished from unknown future/open-ended forms explicitly.
    - session/media scope remains explicit in the parsed audio timing model, so final ingestion can enforce media-level `mediaclk` correctly.

### tests/test_receive_reorder_tolerance_policy.cpp
- Роль:
    - focused coverage для общей модели reorder-tolerance policy, reused by both video and audio reorder-buffer configs.
- Покрывает:
    - modeled enum axis:
        - `ReceiveReorderGapPolicy::WaitForMissing`;
        - `ReceiveReorderGapPolicy::FlushGapOnce`.
    - `validate_receive_reorder_gap_policy(...)` for known and unknown enum values;
    - explicit default behavior of `ReceiveReorderTolerancePolicy`:
        - default-constructed policy stays `WaitForMissing`;
        - default policy validates successfully.
    - helper behavior:
        - `receive_reorder_policy_allows_gap_flush_once(...)` distinguishes named policies correctly.
    - `validate_receive_reorder_tolerance_policy(...)` rejection of invalid/unknown gap policy.
    - `VideoReorderBufferConfig` validation:
        - both named policies are accepted;
        - zero window is rejected;
        - invalid policy is rejected.
    - `AudioReorderBufferConfig` validation:
        - both named policies are accepted;
        - zero window is rejected;
        - invalid policy is rejected.
- Фиксирует:
    - reorder gap-tolerance policy is modeled once and reused consistently by video and audio reorder-buffer config boundaries;
    - default behavior stays explicit and does not rely on hidden fallback logic.