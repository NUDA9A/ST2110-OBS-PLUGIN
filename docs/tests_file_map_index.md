# ST2110-OBS-PLUGIN — Tests file map

> Назначение файла:
> - держать карту тестового покрытия отдельно от production code map;
> - фиксировать, какой subsystem / boundary покрывает каждый тест;
> - не дублировать полную реализацию тестов;
> - обновлять после принятия задач, если добавлены, удалены или существенно изменены тестовые файлы;
> - хранить стабильный индекс шардированной test map.
>
> Важно:
> - production headers / runtime code описываются в `code_map.md`;
> - test targets / `.cpp` тесты описываются здесь;
> - `plan.md` хранит backlog/status/deviations;
> - `code_map.md` тесты не содержит.
>
> Правила ведения:
> - один test file описываем ровно в одном шарде;
> - шарды группируем по subsystem / architectural boundary, а не по алфавиту;
> - при изменении состава шардов или порядка сборки обновляем и этот индекс, и `scripts/rebuild_tests_file_map.sh`;
> - детальные записи хранятся в `tests_file_map_shard_*.md`.

## Карта шардов

### `tests_file_map_shard_01_build_and_foundations.md`
- Область:
    - build integration;
    - smoke/common foundations;
    - общие low-level helpers;
    - ODR/link regressions.
- Содержит:
    - `tests/CMakeLists.txt`
    - `tests/test_smoke.cpp`
    - `tests/test_endian.cpp`
    - `tests/test_error.cpp`
    - `tests/test_bytespan.cpp`
    - `tests/test_stats.cpp`
    - `tests/test_config_validation.cpp`
    - `tests/test_header_odr_link_main.cpp`
    - `tests/test_header_odr_link_a.cpp`
    - `tests/test_header_odr_link_b.cpp`
    - `tests/test_timestamp_ns.cpp`

### `tests_file_map_shard_02_runtime_config_and_backend_interfaces.md`
- Область:
    - manual/runtime config surface;
    - backend interfaces;
    - backend factory;
    - shared reorder-tolerance policy.
- Содержит:
    - `tests/test_rx_config.cpp`
    - `tests/test_backend_interface.cpp`
    - `tests/test_backend_factory.cpp`
    - `tests/test_receive_reorder_tolerance_policy.cpp`

### `tests_file_map_shard_03_packet_parsing_and_reorder.md`
- Область:
    - RTP / ST 2110-20 packet parsing;
    - packet view / packet policy;
    - packet admission;
    - generic reorder abstractions and fixed reorder behavior.
- Содержит:
    - `tests/test_rtp_parser.cpp`
    - `tests/test_rtp_seq.cpp`
    - `tests/test_rtp_payload.cpp`
    - `tests/test_st2110_20_types.cpp`
    - `tests/test_st2110_20_parse.cpp`
    - `tests/test_st2110_20_validate.cpp`
    - `tests/test_extended_seq.cpp`
    - `tests/test_zero_length_srd.cpp`
    - `tests/test_st2110_20_ordering.cpp`
    - `tests/test_packet_view.cpp`
    - `tests/test_packet_view_parse.cpp`
    - `tests/test_packet_view_trailing_padding.cpp`
    - `tests/test_packet_parse_stats.cpp`
    - `tests/test_packet_parse_policy.cpp`
    - `tests/test_packet_parse_integration_stats.cpp`
    - `tests/test_reorder_buffer_interface.cpp`
    - `tests/test_fixed_reorder_buffer.cpp`
    - `tests/test_fixed_reorder_buffer_stats.cpp`
    - `tests/test_fixed_reorder_buffer_flush.cpp`
    - `tests/test_video_packet_admission.cpp`

### `tests_file_map_shard_04_video_frame_assembly_and_pipeline.md`
- Область:
    - video frame storage;
    - frame assembly;
    - video receive semantics;
    - depacketizer;
    - receive pipeline and reconstructor.
- Содержит:
    - `tests/test_video_frame.cpp`
    - `tests/test_video_frame_mutable_access.cpp`
    - `tests/test_frame_write_coverage.cpp`
    - `tests/test_frame_assembler_lifecycle.cpp`
    - `tests/test_frame_assembler_bounds.cpp`
    - `tests/test_frame_assembler_completeness.cpp`
    - `tests/test_frame_assembler_partial_policy.cpp`
    - `tests/test_video_scan_mode.cpp`
    - `tests/test_video_receive_semantics.cpp`
    - `tests/test_video_assembly_key.cpp`
    - `tests/test_video_field_id_boundary.cpp`
    - `tests/test_video_segment_constraints.cpp`
    - `tests/test_video_segment_placement.cpp`
    - `tests/test_video_packet_trailing_padding.cpp`
    - `tests/test_depacketizer_api.cpp`
    - `tests/test_depacketizer_grouping.cpp`
    - `tests/test_depacketizer_marker.cpp`
    - `tests/test_depacketizer_writes.cpp`
    - `tests/test_depacketizer_stats.cpp`
    - `tests/test_depacketizer_unit_state.cpp`
    - `tests/test_depacketizer_segment_mapping.cpp`
    - `tests/test_depacketizer_trailing_padding.cpp`
    - `tests/test_depacketizer_trailing_padding_state.cpp`
    - `tests/test_video_unit_reconstructor.cpp`
    - `tests/test_video_receive_pipeline.cpp`

### `tests_file_map_shard_05_video_signaling_and_runtime_projection.md`
- Область:
    - video signaling model;
    - runtime projection;
    - bootstrap projection;
    - modeled sender/reference/media properties.
- Содержит:
    - `tests/test_video_signaling.cpp`
    - `tests/test_video_signaling_rx_match.cpp`
    - `tests/test_video_signaling_to_rx_config.cpp`
    - `tests/test_video_signaling_to_pipeline_config.cpp`
    - `tests/test_video_receiver_bootstrap.cpp`
    - `tests/test_video_packing_mode_runtime_projection.cpp`
    - `tests/test_video_signaled_media_properties.cpp`
    - `tests/test_video_reference_clock.cpp`
    - `tests/test_video_sender_signaling.cpp`
    - `tests/test_video_timing_signaling.cpp`

### `tests_file_map_shard_06_video_receiver_timing_and_timestamp_mapping.md`
- Область:
    - video receiver timing policy;
    - timing-aware bootstrap wrapper;
    - RTP timestamp mapping;
    - playout/reconstruction timing.
- Содержит:
    - `tests/test_video_receiver_timing.cpp`
    - `tests/test_video_receiver_timing_signaling.cpp`
    - `tests/test_video_receiver_timing_bootstrap.cpp`
    - `tests/video_receiver_timing_architecture_test.cpp`
    - `tests/video_playout_timing_test.cpp`
    - `tests/video_timestamp_mapping_test.cpp`
    - `tests/video_timestamp_mapping_invariants_test.cpp`

### `tests_file_map_shard_07_video_sdp_ingestion_and_transport_boundary.md`
- Область:
    - raw video SDP media-section parsing;
    - fmtp / rtpmap / timing parsing;
    - final SDP ingestion;
    - raw transport/redundancy metadata boundaries.
- Содержит:
    - `tests/video_sdp_media_section_test.cpp`
    - `tests/video_sdp_fmtp_test.cpp`
    - `tests/video_sdp_signaling_adapter_test.cpp`
    - `tests/video_sdp_timing_attributes_test.cpp`
    - `tests/video_sdp_rtpmap_test.cpp`
    - `tests/video_sdp_ingestion_test.cpp`
    - `tests/video_sdp_fmtp_timing_parameters_test.cpp`
    - `tests/video_sdp_maxudp_parameters_test.cpp`
    - `tests/video_sdp_depth_16f_test.cpp`
    - `tests/video_sdp_media_property_enum_coverage_test.cpp`
    - `tests/video_sdp_optional_sender_timing_test.cpp`
    - `tests/video_sdp_transport_boundary_test.cpp`
    - `tests/video_sdp_media_cross_field_validation_test.cpp`
    - `tests/video_sdp_source_filter_scope_test.cpp`
    - `tests/video_sdp_redundancy_boundary_test.cpp`
    - `tests/video_sdp_fmtp_strict_parsing_test.cpp`
    - `tests/video_sdp_timing_scope_test.cpp`
    - `tests/video_sdp_connection_data_test.cpp`

### `tests_file_map_shard_08_audio_signaling_bootstrap_and_channel_order.md`
- Область:
    - audio signaling model;
    - signaling-to-runtime projection;
    - bootstrap composition;
    - channel-order boundary.
- Содержит:
    - `tests/audio_signaling_to_rx_config_test.cpp`
    - `tests/audio_signaling_model_test.cpp`
    - `tests/audio_channel_order_boundary_test.cpp`
    - `tests/audio_receiver_bootstrap_test.cpp`

### `tests_file_map_shard_09_audio_sdp_ingestion.md`
- Область:
    - raw audio SDP media-section parsing;
    - audio SDP signaling adapter;
    - final audio SDP ingestion;
    - raw audio timing/reference-clock parsing.
- Содержит:
    - `tests/audio_sdp_media_section_test.cpp`
    - `tests/audio_sdp_signaling_adapter_test.cpp`
    - `tests/audio_sdp_ingestion_test.cpp`
    - `tests/audio_sdp_timing_attributes_test.cpp`

### `tests_file_map_shard_10_audio_packet_pipeline_and_timestamping.md`
- Область:
    - audio frame storage;
    - audio packet model;
    - audio reorder and frame assembly;
    - audio receive stats;
    - audio RTP timestamp mapping and playout timing.
- Содержит:
    - `tests/test_audio_frame.cpp`
    - `tests/test_audio_packet.cpp`
    - `tests/test_audio_rtp_parser.cpp`
    - `tests/test_audio_reorder_buffer.cpp`
    - `tests/test_audio_frame_assembler.cpp`
    - `tests/test_audio_stats.cpp`
    - `tests/audio_timestamp_mapping_test.cpp`
    - `tests/audio_timestamp_mapping_invariants_test.cpp`

### `tests_file_map_shard_11_socket_runtime_and_concrete_backends.md`
- Область:
    - socket runtime abstraction;
    - Linux socket runtime;
    - socket operational architecture;
    - concrete socket video/audio backends.
- Содержит:
    - `tests/test_socket_runtime_interface.cpp`
    - `tests/test_linux_socket_rx_port.cpp`
    - `tests/test_socket_rx_video_backend.cpp`
    - `tests/test_socket_rx_audio_backend.cpp`
    - `tests/test_socket_rx_operational_architecture.cpp`

## Текущая структура тестов (file map)

> Назначение раздела:
> - держать карту тестового покрытия отдельно от production code map;
> - фиксировать, какой subsystem / boundary покрывает каждый тест;
> - не дублировать полную реализацию тестов;
> - обновлять после принятия задач, если добавлены, удалены или существенно изменены тестовые файлы.
>
> Детальные записи ниже подтягиваются из шардов в порядке, зафиксированном в `scripts/rebuild_tests_file_map.sh`.