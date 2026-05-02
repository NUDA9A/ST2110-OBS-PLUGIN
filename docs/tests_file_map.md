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
    - проверяет `IReorderBuffer` abstraction, `StoredPacket` ownership/view restoration, и reorder stats snapshot boundary.
- Покрывает:
    - abstract/interface shape:
        - `push(...)`;
        - `pop_next()`;
        - `stats() -> ReorderBufferStats`;
        - `reset()`.
    - `StoredPacket::view()`:
        - RTP header field restoration;
        - extended sequence restoration;
        - SRD header restoration;
        - payload segment view reconstruction.
    - fake reorder buffer behavior:
        - push/pop lifecycle;
        - reset behavior;
        - basic `ReorderBufferStats` accounting and zero-after-reset behavior.
- Фиксирует:
    - reorder stats are available through the interface boundary and do not require concrete-type access from backend code.

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
        - scan-mode-specific packet acceptance.

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
    - проверяет SRD segment writes into assembled frame storage.

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
    - проверяет, что rejected trailing padding не corrupt’ит assembly state.

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

### tests/test_video_signaling_to_rx_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to `RxVideoConfig`.

### tests/test_video_signaling_to_pipeline_config.cpp
- Роль:
    - проверяет projection from `VideoStreamSignaling` to runtime video receive pipeline config.

### tests/test_video_receiver_bootstrap.cpp
- Роль:
    - проверяет generic signaling-driven receiver bootstrap composition.

### tests/test_video_packing_mode_runtime_projection.cpp
- Роль:
    - проверяет runtime projection/support boundary для `VideoPackingMode`.

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
    - acceptance of a valid BT709/SDR media-description shape with `SSN=ST2110-20:2017`;
    - acceptance of absent optional signaling/media fields where currently allowed;
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
    - ST 2110-20 signaled dimension limits are now enforced in the signaling/media-description boundary;
    - current UYVY-specific even-width runtime constraint still remains localized below that boundary.

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
    - propagation of reference-clock validation through `validate_video_stream_signaling(...)`.
- Фиксирует:
    - strict modeled validation of known reference-clock forms remains localized on the signaling boundary;
    - unknown/open-ended forms still remain preservable only through explicit `Other`.

### tests/test_video_sender_signaling.cpp
- Роль:
    - проверяет sender timing signaling fields:
        - sender type;
        - TROFF;
        - CMAX;
        - structural validation.

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

### tests/video_receiver_timing_architecture_test.cpp
- Роль:
    - architecture regression test для receiver timing boundary placement;
    - фиксирует, что timing-aware layer остается overlay над generic signaling/bootstrap path.

### tests/video_playout_timing_test.cpp
- Роль:
    - проверяет receiver-side playout/reconstruction timing boundary:
        - offset/delay decision;
        - overflow rejection;
        - adapter from reconstructed frame metadata to timing metadata.

## Video RTP timestamp mapping

### tests/test_timestamp_ns.cpp
- Роль:
    - проверяет базовый internal timestamp type contract.

### tests/video_timestamp_mapping_test.cpp
- Роль:
    - проверяет standards-aware RTP timestamp -> internal nanoseconds mapping;
    - проверяет synthetic/manual timestamp mapper path.

### tests/video_timestamp_mapping_invariants_test.cpp
- Роль:
    - проверяет timestamp mapping invariants:
        - monotonic internal timestamps;
        - 32-bit RTP timestamp wraparound;
        - long-running continuity;
        - backward/ambiguous delta rejection;
        - separation from synthetic/manual timing and playout timing.

## Video SDP raw media-section parsing / SDP ingestion

### tests/video_sdp_media_section_test.cpp
- Роль:
    - проверяет raw SDP video media-section selection boundary in `video_sdp_media_section.hpp`;
    - проверяет payload-bound `rtpmap` / `fmtp`;
    - проверяет preservation of raw media-line and unknown attributes;
    - проверяет tightened raw `m=video` validation for ST 2110 video SDP.
- Покрывает:
    - selection of the matching `m=video` section by expected payload type;
    - preservation of the selected raw `media_line`;
    - parsing/preservation of:
        - `rtpmap`;
        - `fmtp`;
        - timing/sender attributes where applicable;
        - unknown media attributes.
    - rejection of payload-type mismatch between selected media section and expected payload type.
    - rejection of missing required `rtpmap` association for the selected payload type.
    - rejection of duplicate relevant media attributes.
    - tightened raw `m=video` validation:
        - valid `m=video 50000 RTP/AVP 112` accepted;
        - non-dynamic RTP payload type rejected for ST 2110 raw video;
        - malformed media-line port rejected;
        - unsupported media-line protocol rejected.
- Фиксирует:
    - raw SDP media-section parsing now performs explicit ST 2110 video `m=video` validation without moving this policy into socket/runtime code;
    - raw media-line text remains preserved for future transport/bootstrap use.

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
    - проверяет raw SDP transport/redundancy metadata preservation:
        - `c=`;
        - `source-filter`;
        - `mid`;
        - `group:DUP`;
        - unknown attributes.
- Покрывает:
    - preservation of session and media `c=` connection data separately;
    - preservation of `mid`, `source-filter`, and session-level `group:DUP`;
    - separate preservation of unknown session-level and media-level attributes;
    - duplicate rejection for:
        - media connection;
        - session connection;
        - `mid`.
    - transport metadata isolation across media sections.
    - compatibility with final video SDP ingestion when the selected media section is otherwise standards-clean, including media-level `mediaclk`.
- Фиксирует:
    - raw transport/redundancy metadata stays outside `VideoStreamSignaling`;
    - transport metadata does not break final video signaling ingestion when the selected media section includes the required media-level `mediaclk`.

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
    - проверяет session/media scope preservation and final-ingestion policy for SDP timing attributes.
- Покрывает:
    - session-level `ts-refclk` / `mediaclk` preservation in the raw selected media-section model;
    - session-level-only `mediaclk` remaining preserved in raw timing parsing but being rejected by final video SDP ingestion;
    - media-level `ts-refclk` / `mediaclk` overriding session-level values in the resolved raw timing model;
    - duplicate rejection within the same scope for timing attributes;
    - conflict policy between fmtp timing fields and standalone timing attributes:
        - fmtp timing field may override session-level standalone attribute;
        - fmtp timing field conflicts with same-semantic media-level standalone attribute.
    - existing media-level-only SDP timing behavior remaining unchanged.
- Фиксирует:
    - raw timing parsing remains scope-aware and non-destructive;
    - final ST 2110 video SDP ingestion now requires both a resolved `ts-refclk` and a media-level `mediaclk`;
    - media-level timing values still override session-level values where applicable.

### tests/video_sdp_connection_data_test.cpp
- Роль:
    - проверяет raw SDP `c=` connection data parsing:
        - original connection address preservation;
        - parsed base address;
        - optional TTL;
        - optional address count.

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
    - покрывает:
        - valid Level A stereo projection;
        - min/max Level A channel counts;
        - channel-order presence not leaking into runtime buffer layout yet;
        - projection of signaled `pcm_bit_depth` into `RxAudioConfig::pcm_bit_depth`;
        - invalid signaling rejection;
        - bad UDP port;
        - bad RTP payload type;
        - empty destination IP;
        - unsupported runtime audio sample format rejection.
    - фиксирует, что:
        - `samples_per_packet` выводится из `sampling_rate_hz` + `packet_time_us`, а не задается как hardcoded runtime constant;
        - runtime audio bit depth is now carried explicitly from signaling instead of being inferred later inside the backend.

### tests/audio_signaling_model_test.cpp
- Роль:
    - проверяет initial ST 2110-30 audio signaling model shape;
    - фиксирует Level A-oriented receiver baseline:
        - 48 kHz;
        - 1 ms packet time;
        - 1..8 channels;
    - проверяет separation между signaled audio/media properties и future runtime audio buffer layout;
    - проверяет initial structural validation boundary:
        - conformance range validation;
        - media-description-to-baseline validation;
        - valid / invalid Level A channel counts;
        - valid / invalid sampling rate and packet time;
        - optional absent channel-order behavior;
        - `SMPTE2110.(...)` channel-order structural validation;
        - declared channel-order count rejection when greater than stream channel count;
        - explicit invalid `Unspecified` channel-order raw value rejection;
        - forward-compatible `Other` channel-order preservation.

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
    - проверяет raw SDP audio media-section parsing boundary.
    - покрывает:
        - selection of matching `m=audio` section by payload type;
        - preservation of media payload types;
        - payload-bound `a=rtpmap` parsing into encoding name, sampling rate, and optional channel count;
        - `a=ptime` parsing from milliseconds to integer microseconds;
        - payload-bound `a=fmtp:<pt> channel-order=...` parsing;
        - preservation of unknown fmtp parameters;
        - preservation of unknown session/media attributes;
        - standalone `a=channel-order:` remaining unknown rather than being treated as standards-facing channel-order signaling;
        - rejection of missing selected `rtpmap`, duplicate selected `rtpmap`, duplicate selected `fmtp`, duplicate `ptime`, duplicate `channel-order`, malformed RTP clock/channel count, extra rtpmap slash, zero ptime, and non-integral-microsecond ptime.

### tests/audio_sdp_signaling_adapter_test.cpp
- Роль:
    - проверяет adapter from raw parsed audio SDP media section to `AudioStreamSignaling`.
    - покрывает:
        - valid Level A raw SDP mapping;
        - valid Level A min/max channel counts;
        - mapping of `L24` / `L16` RTP encoding names to `AudioPcmEncoding::LinearPcm`;
        - explicit mapping of `L24` / `L16` RTP encoding names to `AudioPcmBitDepth::{Bits24, Bits16}`;
        - rejection of unsupported encoding names through `Unsupported`;
        - rejection of missing `ptime`;
        - rejection of missing explicit channel count in selected `rtpmap`;
        - rejection of invalid baseline values through `validate_audio_stream_signaling(...)`;
        - mapping of payload-bound `fmtp channel-order=...` into `AudioChannelOrderSignaling`;
        - preservation of adapter boundary separation from `RxAudioConfig`, socket/backend transport fields, and audio buffer layout.

### tests/audio_sdp_ingestion_test.cpp
- Роль:
    - проверяет final audio SDP-to-`AudioStreamSignaling` ingestion composition.
    - покрывает:
        - valid Level A SDP ingestion through `parse_audio_stream_signaling_from_sdp(...)`;
        - composition of raw media-section selection with raw-to-signaling adapter;
        - payload type mismatch rejection;
        - missing required selected `rtpmap` rejection;
        - invalid `ptime` rejection;
        - unsupported runtime-independent signaling values rejected through signaling validation / adapter boundaries;
        - required clock-signaling presence checks:
            - `ts-refclk` at session or selected media scope;
            - media-level `mediaclk`;
        - unknown SDP attributes preserved at raw layer but ignored by final signaling mapping unless explicitly modeled;
        - separation from runtime `RxAudioConfig`, socket/backend transport fields, and audio buffer/channel layout.

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
    - проверяет MVP audio reorder/jitter boundary over validated `AudioRtpPacketView`.
- Покрывает:
    - `AudioFixedWindowReorderBuffer` in-order packet push/pop behavior;
    - out-of-order packet arrival and ordered emission by RTP sequence number;
    - RTP sequence-number wraparound behavior;
    - duplicate packet rejection and stats accounting;
    - late packet rejection after expected sequence has advanced;
    - out-of-window packet rejection;
    - explicit `flush_missing_once()` behavior for missing packets;
    - `reset()` clearing pending state and stats;
    - `StoredAudioRtpPacket::view()` restoring a valid non-owning `AudioRtpPacketView` over owned payload bytes.
- Фиксирует:
    - audio reorder remains separate from RTP parsing and payload validation;
    - RTP marker and timestamp are preserved but not interpreted;
    - reorder/jitter MVP does not create `AudioBuffer`, does not apply channel-order mapping, does not perform timestamp mapping, and does not implement backend behavior.

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
    - проверяет audio RTP timestamp mapping and receiver-side playout timing boundary introduced for task `095`.
- Покрывает:
    - `AudioRtpTimestampMapperConfig` validation:
        - valid RTP clock rate;
        - zero RTP clock rate rejected.
    - RTP tick to nanosecond conversion:
        - exact second conversion;
        - fractional tick conversion;
        - invalid clock rejection;
        - overflow rejection where applicable.
    - `AudioRtpTimestampMapper` behavior:
        - mapping from explicit RTP/timestamp anchor;
        - monotonic forward timestamp mapping;
        - 32-bit RTP timestamp wraparound continuity;
        - backward / ambiguous timestamp movement rejection;
        - reset behavior.
    - receiver-side playout timing:
        - zero-delay behavior;
        - positive playout delay addition;
        - overflow rejection.
    - `AudioBlockTiming` adapter behavior:
        - preserves RTP timestamp metadata;
        - carries mapped media timestamp;
        - carries computed playout timestamp.
- Фиксирует:
    - audio timestamp mapping remains separate from RTP parsing, audio packet validation, reorder/jitter buffering, audio frame/block assembly, channel-order mapping, socket backend, MTL backend, and OBS handoff behavior.

### tests/audio_timestamp_mapping_invariants_test.cpp
- Роль:
    - проверяет audio timestamp mapping invariants and cadence behavior for task `096`.
- Покрывает:
    - exact and monotonic 48 kHz / 1 ms RTP timestamp cadence mapping;
    - explicit non-default audio RTP clock-rate handling;
    - 32-bit RTP timestamp wraparound continuity;
    - long-running continuity beyond one 32-bit RTP timestamp epoch;
    - rejection of backward / ambiguous timestamp deltas;
    - failed timestamp mapping not advancing mapper state;
    - reset/reanchor behavior;
    - receiver-side playout timing preserving media cadence with a constant delay.
- Фиксирует:
    - audio RTP timestamp mapping uses explicit RTP clock rate and RTP tick deltas rather than hardcoded packet cadence;
    - RTP-domain mapping remains separate from receiver-side playout timing;
    - timestamp invariants remain separate from RTP parsing, packet admission, reorder/jitter buffering, audio block assembly, channel-order mapping, socket backend, MTL backend, and OBS handoff behavior.


### tests/test_socket_rx_video_backend.cpp
- Роль:
    - проверяет lifecycle, config projection, datagram receive composition, periodic stats snapshot behavior, graceful stop/cleanup behavior, and default-factory behavior of socket video backend through injected and real Linux socket-port paths.
- Покрывает:
    - injected-factory path:
        - stop before successful start;
        - repeated stop after clean shutdown;
        - IPv4 multicast projection;
        - IPv4 multicast projection with explicit interface/local address;
        - IPv6 multicast projection;
        - projection failure;
        - port-open failure and retry;
        - null created port rejection;
        - close-failure behavior during `stop()`;
        - restart after successful stop.
    - backend receive-path composition:
        - reordered packet delivery into the existing video receive pipeline;
        - explicit RTCP-like/control datagram ignore behavior;
        - explicit RTP payload-type admission;
        - malformed media datagram reject behavior;
        - RTP timestamp mapping before sink delivery.
    - backend stats snapshot behavior:
        - zero snapshot on idle/stopped backend;
        - received datagram / byte counters;
        - ignored control / non-media counters;
        - parsed/rejected packet counters;
        - delivered frame counters;
        - nested packet-parse / reorder / depacketizer stats;
        - reset of per-run stats after stop and restart.
    - graceful stop / cleanup guarantees:
        - `stop()` before successful `start_video()` succeeds and keeps backend stopped;
        - repeated `stop()` after successful shutdown remains a no-op/success without extra close calls;
        - close failure does not falsely clear active backend state;
        - retry after failed start remains valid.
    - factory/default path:
        - descriptor and backend-creation shape;
        - Linux default backend bind-failure path on real Linux socket port;
        - Linux default backend recovery after multicast join failure;
        - Linux default backend successful IPv4 unicast frame receive path with stats snapshot.
- Фиксирует:
    - `SocketRxVideoBackend` keeps graceful stop and runtime cleanup localized inside backend/runtime boundaries;
    - socket close is performed through `ISocketRxPort`, not by leaking native-socket handling into backend callers;
    - datagram classification, receive/runtime cleanup, and sink delivery remain separate concerns.

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
    - regression / architecture test для `SocketRxAudioBackend` поверх общего `SocketRxSingleMediaBackendBase`.
- Покрывает:
    - interface / type shape:
        - `SocketRxAudioBackend` final;
        - `IRxAudioBackend` / `IRxBackend` convertibility;
        - constructor shape with default and injected socket port factory;
        - `SocketRxAudioBackendFactory` descriptor/creation shape.
    - lifecycle/runtime policy through the common socket single-media base:
        - stop before successful start;
        - repeated stop;
        - repeated start on already-active audio backend;
        - failed start remains stopped and retryable;
        - stop propagates close failure without losing active state;
        - successful stop allows restart.
    - socket-open projection for audio configs:
        - IPv4 multicast;
        - IPv4 multicast with interface address;
        - IPv4 unicast;
        - IPv6 multicast.
    - explicit failure mapping:
        - projection failure;
        - null created port;
        - port open failure.
    - audio receive-path integration:
        - reordered RTP audio packets are delivered to the sink in sequence order;
        - audio RTP timestamps are mapped to `TimestampNs`;
        - RTCP-like datagrams are ignored before media delivery;
        - wrong RTP payload type is ignored before media delivery;
        - malformed audio media packet is rejected without stopping later delivery.
    - backend stats behavior for the audio path:
        - `packets_parsed_ok`;
        - `packets_rejected`;
        - `control_datagrams_ignored`;
        - `nonmedia_datagrams_ignored`;
        - `datagrams_dropped`;
        - `media_units_delivered`;
        - `frames_delivered` remains zero for audio delivery.
    - default runtime path:
        - Linux build opens/closes one real IPv4 unicast socket port;
        - unsupported build uses the stub factory path.
- Фиксирует:
    - `SocketRxAudioBackend` now reuses the shared socket lifecycle/runtime boundary while also connecting the existing audio packet/reorder/assembler/timestamp helpers into one backend receive path;
    - remaining follow-up work is not basic audio socket runtime integration anymore, but explicit wire-format modeling and future observability/policy extensions.

### tests/test_video_packet_admission.cpp
- Роль:
    - проверяет explicit video RTP payload-type admission boundary separate from generic RTP / `PacketView` parsing.
    - проверяет, что wrong-PT packets are dropped locally before reorder/depacketizer use in the socket video receive path.
- Покрывает:
    - matching dynamic payload type accepted through:
        - `validate_rtp_payload_type_admission(...)`;
        - `validate_video_packet_payload_type_admission(...)`.
    - mismatching payload type rejected by the admission helper.
    - payload-type admission remains separate from generic RTP parsing:
        - structurally valid RTP/ST2110 packet with a different PT still parses successfully into `PacketView`;
        - stream-specific admission then rejects it.
    - socket video backend runtime path:
        - wrong-PT datagram is counted as ignored/dropped locally;
        - reorder buffer is not entered;
        - depacketizer is not entered;
        - no media units / frames are delivered.
- Фиксирует:
    - `PacketView::rtp.payload_type` is parsed by the generic RTP/packet parser;
    - stream membership by PT is decided later by the explicit admission boundary.