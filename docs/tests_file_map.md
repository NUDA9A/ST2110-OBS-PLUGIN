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

### tests/CMakeLists.txt
- Роль:
    - объявляет helper `add_st2110_test(name ...)`;
    - регистрирует все unit / architecture / regression tests через CTest;
    - каждый test executable линкуется с `st2110core`;
    - каждому test target явно задается `cxx_std_23`, чтобы IDE/test-target compilation model не расходился с project/toolchain requirements.
- Сущности:
    - `add_st2110_test(...)`
    - targets for smoke/base tests, RTP/ST2110 packet parsing, reorder, frame assembly, depacketizer, video signaling, SDP ingestion, timing, playout, audio signaling model tests, audio SDP ingestion tests, audio receiver bootstrap tests, backend interface tests, backend factory tests, audio frame storage tests, and audio packet model tests.

## Smoke / common foundations

### tests/test_smoke.cpp
- Роль:
    - минимальный smoke test для CTest/build pipeline.

### tests/test_endian.cpp
- Роль:
    - проверяет big-endian helpers `read_be16` / `read_be32`.

### tests/test_error.cpp
- Роль:
    - проверяет общий error enum / string mapping.

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
    - проверяет initial `RxAudioConfig` runtime validation:
        - Level A-oriented default runtime support;
        - channel count bounds;
        - sample rate / packet time validation through runtime support policy;
        - derived `samples_per_packet` consistency;
        - UDP port;
        - dynamic RTP payload type;
        - destination IP requirement;
        - local IP allowed to be empty;
        - unsupported audio sample format rejection.
    - проверяет architecture property for audio runtime support:
        - `validate_rx_audio_config(...)` is a thin default-policy wrapper;
        - `validate_rx_audio_config_against_runtime_support(...)` can validate a custom support policy without rewriting default validation;
        - non-default packet time support can be accepted by explicit support policy while remaining rejected by current default policy.

### tests/test_backend_interface.cpp
- Роль:
    - проверяет backend/sink interface contracts;
    - покрывает FakeVideoBackend -> FakeVideoSink delivery path;
    - покрывает FakeAudioBackend -> FakeAudioSink delivery path;
    - покрывает combined video+audio backend shape over one common `IRxBackend` base.
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
    - inheritance:
        - `IRxVideoBackend` derives from `IRxBackend`;
        - `IRxAudioBackend` derives from `IRxBackend`;
        - video/audio backend interfaces use virtual inheritance so a combined backend has a single common `IRxBackend` base.
    - common backend API:
        - `backend_name()`;
        - `stop()`;
        - `capabilities()`.
    - media-specific start API:
        - `IRxVideoBackend::start_video(const RxVideoConfig&, IVideoFrameSink&)`;
        - `IRxAudioBackend::start_audio(const RxAudioConfig&, IAudioFrameSink&)`.
    - video delivery:
        - fake video backend starts with `RxVideoConfig`;
        - emits a `VideoFrameView`;
        - sink receives width/height/timestamp/data/stride.
    - audio delivery:
        - fake audio backend starts with `RxAudioConfig`;
        - emits an `AudioFrameView`;
        - sink receives sampling rate, channel count, samples per channel, timestamp, sample pointer, stride, total sample count, and byte size.
    - combined backend:
        - one fake backend implements both `IRxVideoBackend` and `IRxAudioBackend`;
        - can be used through `IRxBackend&`, `IRxVideoBackend&`, and `IRxAudioBackend&`;
        - reports both video and audio capabilities;
        - delivers both video and audio through separate media-specific start methods.
- Фиксирует:
    - socket/MTL backend implementations can later expose video-only, audio-only, or combined capabilities without duplicating common lifecycle/base API;
    - backend interfaces consume existing runtime/storage boundaries and do not embed SDP parsing, channel-order interpretation, RTP packet parsing, jitter/reorder, playout policy, socket behavior, or MTL behavior.

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
        - rejects null backend results from a factory.
- Фиксирует:
    - backend kind is modeled separately from media kind;
    - backend selection remains separate from concrete socket/MTL implementation details and separate from video/audio runtime config and packet pipeline code;
    - future backend expansion should plug in through descriptor/factory registration rather than ad hoc creation branches in apps/plugins.

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
        - UDP datagram size policy;
        - default/effective max size;
        - oversize rejection.

### tests/test_packet_parse_integration_stats.cpp
- Роль:
    - проверяет integrated packet parse path with stage-specific stats recording.

## Reorder buffer

### tests/test_reorder_buffer_interface.cpp
- Роль:
    - проверяет `IReorderBuffer` abstraction и `StoredPacket` ownership/view restoration.

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

### tests/test_video_reference_clock.cpp
- Роль:
    - проверяет reference clock model and validation.

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
    - проверяет raw SDP video media-section selection by payload type;
    - проверяет payload-bound `rtpmap` / `fmtp`;
    - проверяет unknown attribute preservation and duplicate rejection.

### tests/video_sdp_fmtp_test.cpp
- Роль:
    - проверяет raw SDP `a=fmtp` parsing for video media-description fields.

### tests/video_sdp_signaling_adapter_test.cpp
- Роль:
    - проверяет adapter from raw SDP/fmtp media-description fields to `VideoStreamSignaling`.

### tests/video_sdp_timing_attributes_test.cpp
- Роль:
    - проверяет raw parsing of SDP timing/reference/sender attributes.

### tests/video_sdp_rtpmap_test.cpp
- Роль:
    - проверяет raw SDP `a=rtpmap` parsing/binding for selected payload type.

### tests/video_sdp_ingestion_test.cpp
- Роль:
    - проверяет final SDP-to-`VideoStreamSignaling` ingestion composition.

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
    - проверяет propagation into signaling and packet parse policy.

### tests/video_sdp_depth_16f_test.cpp
- Роль:
    - проверяет SDP `depth=16f` parsing and signaling representation.

### tests/video_sdp_media_property_enum_coverage_test.cpp
- Роль:
    - проверяет explicit enum coverage for known ST 2110-20 SDP media-property tokens;
    - проверяет `Other + raw_token` для unknown future tokens.

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

### tests/video_sdp_media_cross_field_validation_test.cpp
- Роль:
    - проверяет ST 2110-20 media-description cross-field validation:
        - progressive-only 4:2:0 variants;
        - `KEY + ALPHA`;
        - rejection of invalid KEY/TCS/colorimetry combinations.

### tests/video_sdp_source_filter_scope_test.cpp
- Роль:
    - проверяет scope-aware raw SDP `a=source-filter` parsing:
        - session scope;
        - media scope;
        - parsed fields;
        - raw value preservation.

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
    - проверяет session/media scope preservation and resolution for timing/reference/sender attributes.

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
        - invalid signaling rejection;
        - bad UDP port;
        - bad RTP payload type;
        - empty destination IP;
        - unsupported runtime audio sample format.
    - фиксирует, что `samples_per_packet` выводится из `sampling_rate_hz` + `packet_time_us`, а не задается как hardcoded runtime constant.

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
        - valid Level A stereo raw SDP mapping;
        - valid Level A min/max channel counts;
        - mapping of `L24` / `L16` RTP encoding names to `AudioPcmEncoding::LinearPcm`;
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
    - проверяет first audio RTP packet model boundary introduced for MVP audio packet/depacketize path.
- Покрывает:
    - `AudioPcmWireFormat` wire-format axis:
        - `L16`;
        - `L24`;
        - invalid enum rejection.
    - `audio_rtp_packet_policy_from_rx_audio_config(...)`:
        - derives packet policy from validated `RxAudioConfig`;
        - preserves sampling rate, channel count, samples per packet, payload type, and explicit wire format.
    - `audio_rtp_packet_payload_size_bytes(...)`:
        - derives payload byte size from `samples_per_packet * channel_count * wire_sample_bytes`;
        - proves payload sizing is not hardcoded to `48`, stereo, or L24-only behavior.
    - `make_audio_rtp_packet_view(...)`:
        - accepts matching RTP payload type and exact payload size;
        - preserves RTP timestamp/marker metadata without interpreting marker as an audio block boundary;
        - returns a non-owning payload view;
        - rejects payload type mismatch;
        - rejects payload size mismatch.
- Фиксирует:
    - audio RTP packet model remains separate from:
        - internal `AudioBuffer` / `AudioFrameView` storage;
        - `InterleavedS32` layout;
        - channel-order parsing / reordering;
        - SDP ingestion;
        - RTP timestamp mapping;
        - jitter/reorder;
        - socket / MTL backend behavior.

### tests/test_audio_rtp_parser.cpp
- Роль:
    - проверяет minimal audio RTP parser integration entry point.
- Покрывает:
    - `parse_audio_rtp_packet_view(...)` composition over existing RTP helpers:
        - `parse_rtp_header(...)`;
        - `rtp_payload_span(...)`;
        - `make_audio_rtp_packet_view(...)`.
    - valid L24 audio RTP packet parsing;
    - valid L16 audio RTP packet parsing;
    - RTP metadata preservation:
        - marker;
        - payload type;
        - sequence number;
        - timestamp;
        - SSRC;
        - payload offset.
    - CSRC and RTP header-extension tolerance through the existing RTP parsing boundary;
    - payload type mismatch rejection;
    - payload size mismatch rejection;
    - short RTP packet rejection;
    - bad RTP version rejection.
- Фиксирует:
    - audio parser integration does not duplicate RTP parsing;
    - payload type admission remains stream-specific policy, separate from generic RTP parsing;
    - RTP marker and timestamp are preserved but not interpreted;
    - parser integration remains separate from:
        - jitter/reorder;
        - audio block assembly;
        - timestamp mapping to `TimestampNs`;
        - `AudioBuffer` creation;
        - channel-order mapping / reordering;
        - socket / MTL backend behavior.

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
    - проверяет MVP audio frame/block assembly boundary over already-validated `AudioRtpPacketView`.
- Покрывает:
    - `AudioFrameAssembler` valid L16 packet assembly into `AudioBuffer`;
    - `AudioFrameAssembler` valid L24 packet assembly into `AudioBuffer`;
    - signed big-endian PCM decoding behavior:
        - positive values;
        - zero;
        - negative L16 sign extension;
        - negative L24 sign extension.
    - interleaved sample placement by `(sample_index, channel)`;
    - assembled block metadata preservation:
        - RTP timestamp;
        - RTP sequence number;
        - RTP marker bit.
    - exact payload-size validation;
    - invalid packet dimensions rejection;
    - invalid assembler storage-format rejection;
    - invalid wire format rejection;
    - assembler stats:
        - packets used;
        - packets rejected;
        - blocks emitted;
        - reset clears stats.
- Фиксирует:
    - audio frame/block assembly consumes validated audio RTP packet views rather than parsing RTP itself;
    - payload sizing is derived from packet dimensions and wire-format bytes per sample, not from hardcoded `48`, stereo-only, or L24-only assumptions;
    - RTP marker is preserved as metadata but not interpreted as an audio block boundary;
    - assembly remains separate from:
        - RTP parsing;
        - reorder/jitter;
        - RTP timestamp mapping;
        - playout/release policy;
        - channel-order mapping / reordering;
        - socket / MTL backend behavior.

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
    - smoke/regression test for the first concrete socket video backend skeleton.
- Покрывает:
    - `SocketRxVideoBackend` direct backend behavior:
        - `backend_name() == "socket"`;
        - reports video-only capabilities;
        - does not report audio capability;
        - can be viewed through `IRxBackend&` / `IRxVideoBackend&`;
        - video-only backend is not exposed as `IRxAudioBackend`.
    - skeleton lifecycle behavior:
        - `start_video(...)` is callable with valid `RxVideoConfig` and sink;
        - current skeleton start path is a no-op and does not emit frames;
        - `stop()` is callable and remains a no-op placeholder.
    - `SocketRxVideoBackendFactory`:
        - descriptor shape;
        - `RxBackendKind::Socket`;
        - `"socket"` name;
        - video-only capability advertisement;
        - `available = true`.
    - backend creation:
        - factory returns a non-null `IRxBackend`;
        - created backend can be dynamically viewed as `IRxVideoBackend`;
        - created backend remains video-only.
- Фиксирует:
    - the first concrete socket video backend plugs into the existing backend/factory architecture instead of bypassing it;
    - socket video backend skeleton remains separate from UDP socket operations, RTP parsing, depacketizer/pipeline logic, timing/playout, and audio backend behavior.