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
    - каждый test executable линкуется с `st2110core`.
- Сущности:
    - `add_st2110_test(...)`
    - targets for smoke/base tests, RTP/ST2110 packet parsing, reorder, frame assembly, depacketizer, video signaling, SDP ingestion, timing, playout, and audio signaling model tests.

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
    - покрывает FakeBackend -> FakeVideoSink delivery path.

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
        - minimal `SMPTE2110.` channel-order structural validation;
        - explicit invalid `Unspecified` channel-order raw value rejection;
        - forward-compatible `Other` channel-order preservation.

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