# ST2110-OBS-PLUGIN — Code map

> Назначение файла:
> - держать краткую карту текущей production-реализации;
> - уменьшать необходимость пересылать весь код в чат;
> - помогать формулировать задачи и делать приемку в контексте реальной архитектуры;
> - хранить стабильный индекс шардированной code map.
>
> Правила ведения:
> - описываем прежде всего public headers и значимые entry points;
> - фиксируем архитектурную роль, связи и публичные сущности;
> - не дублируем полную реализацию;
> - тесты сюда не добавляем — для них есть `tests_file_map.md`;
> - после принятия задачи обновляем этот файл, если изменились публичные структуры, классы, методы, связи или архитектурная роль production-файлов;
> - детальные записи хранятся в тематических шардах `code_map_shard_*.md`;
> - один production-файл описываем ровно в одном шарде;
> - при изменении состава шардов или порядка сборки обновляем и этот индекс, и `scripts/rebuild_code_map.sh`.

## Карта шардов

### `code_map_shard_01_build_and_entrypoints.md`
- Область:
    - build glue;
    - application entry points;
    - временные compilation units.
- Содержит:
    - `libs/st2110core/CMakeLists.txt`
    - `apps/st2110_rx_dump/main.cpp`
    - `libs/st2110core/src/stub.cpp`

### `code_map_shard_02_core_interfaces_and_common_types.md`
- Область:
    - базовые public интерфейсы;
    - общие типы и ошибки;
    - runtime/config surface;
    - общие policy/value helpers.
- Содержит:
    - `libs/st2110core/include/st2110/backend.hpp`
    - `libs/st2110core/include/st2110/backend_factory.hpp`
    - `libs/st2110core/include/st2110/bytes.hpp`
    - `libs/st2110core/include/st2110/config_validation.hpp`
    - `libs/st2110core/include/st2110/endian.hpp`
    - `libs/st2110core/include/st2110/error.hpp`
    - `libs/st2110core/include/st2110/packet_admission.hpp`
    - `libs/st2110core/include/st2110/pixel_format.hpp`
    - `libs/st2110core/include/st2110/rx_config.hpp`
    - `libs/st2110core/include/st2110/timestamp.hpp`
    - `libs/st2110core/include/st2110/stats.hpp`
    - `libs/st2110core/include/st2110/receive_reorder_tolerance_policy.hpp`
    - `libs/st2110core/include/st2110/rtp_timestamp_anchor_policy.hpp`
    - `libs/st2110core/include/st2110/mtl_rx_backend_factory.hpp`

### `code_map_shard_03_packet_parsing_and_reorder.md`
- Область:
    - RTP / ST 2110 packet parsing;
    - generic packet models;
    - reorder abstraction and concrete fixed-window reorder.
- Содержит:
    - `libs/st2110core/include/st2110/packet_parse.hpp`
    - `libs/st2110core/include/st2110/packet_view.hpp`
    - `libs/st2110core/include/st2110/reorder_buffer.hpp`
    - `libs/st2110core/include/st2110/fixed_reorder_buffer.hpp`
    - `libs/st2110core/include/st2110/rtp.hpp`
    - `libs/st2110core/include/st2110/st2110_20.hpp`

### `code_map_shard_04_video_pipeline_and_frame_assembly.md`
- Область:
    - video frame storage;
    - depacketizer;
    - assembly;
    - placement/semantics;
    - reconstructed-frame pipeline.
- Содержит:
    - `libs/st2110core/include/st2110/depacketizer.hpp`
    - `libs/st2110core/include/st2110/frame_assembler.hpp`
    - `libs/st2110core/include/st2110/frame_write_coverage.hpp`
    - `libs/st2110core/include/st2110/video_frame.hpp`
    - `libs/st2110core/include/st2110/video_receive_pipeline.hpp`
    - `libs/st2110core/include/st2110/video_receive_semantics.hpp`
    - `libs/st2110core/include/st2110/video_segment_constraints.hpp`
    - `libs/st2110core/include/st2110/video_segment_placement.hpp`
    - `libs/st2110core/include/st2110/video_packet_padding.hpp`
    - `libs/st2110core/include/st2110/video_unit_reconstructor.hpp`

### `code_map_shard_05_video_signaling_bootstrap_and_timing.md`
- Область:
    - video modeled axes;
    - signaling model;
    - bootstrap projection;
    - timestamp/playout/timing;
    - reorder policy.
- Содержит:
    - `libs/st2110core/include/st2110/signaling_structs.hpp`
    - `libs/st2110core/include/st2110/video_scan_mode.hpp`
    - `libs/st2110core/include/st2110/video_packing_mode.hpp`
    - `libs/st2110core/include/st2110/video_signaling.hpp`
    - `libs/st2110core/include/st2110/video_receiver_timing.hpp`
    - `libs/st2110core/include/st2110/video_receiver_timing_signaling.hpp`
    - `libs/st2110core/include/st2110/video_timestamp_mapping.hpp`
    - `libs/st2110core/include/st2110/video_playout_timing.hpp`
    - `libs/st2110core/include/st2110/video_reorder_policy.hpp`
    - `libs/st2110core/include/st2110/video_receive_capability.hpp`

### `code_map_shard_06_video_sdp_ingestion.md`
- Область:
    - raw video SDP parsing;
    - fmtp / rtpmap / timing attribute parsing;
    - final SDP-to-signaling ingestion.
- Содержит:
    - `libs/st2110core/include/st2110/video_sdp_media_section.hpp`
    - `libs/st2110core/include/st2110/video_sdp_fmtp.hpp`
    - `libs/st2110core/include/st2110/video_sdp_signaling_adapter.hpp`
    - `libs/st2110core/include/st2110/video_sdp_timing_attributes.hpp`
    - `libs/st2110core/include/st2110/video_sdp_rtpmap.hpp`
    - `libs/st2110core/include/st2110/video_sdp_ingestion.hpp`

### `code_map_shard_07_audio_signaling_bootstrap_and_channel_order.md`
- Область:
    - audio signaling model;
    - signaling-to-runtime projection;
    - bootstrap;
    - channel-order normalization;
    - audio RTP timestamp mapping / playout timing.
- Содержит:
    - `libs/st2110core/include/st2110/audio_signaling.hpp`
    - `libs/st2110core/include/st2110/audio_signaling_rx_config.hpp`
    - `libs/st2110core/include/st2110/audio_receiver_bootstrap.hpp`
    - `libs/st2110core/include/st2110/audio_channel_order.hpp`
    - `libs/st2110core/include/st2110/audio_timestamp_mapping.hpp`

### `code_map_shard_08_audio_pipeline_and_storage.md`
- Область:
    - audio storage/view model;
    - packet model;
    - reorder;
    - block assembly;
    - audio receive stats.
- Содержит:
    - `libs/st2110core/include/st2110/audio_frame.hpp`
    - `libs/st2110core/include/st2110/audio_packet.hpp`
    - `libs/st2110core/include/st2110/audio_reorder_buffer.hpp`
    - `libs/st2110core/include/st2110/audio_frame_assembler.hpp`
    - `libs/st2110core/include/st2110/audio_stats.hpp`

### `code_map_shard_09_audio_sdp_ingestion.md`
- Область:
    - raw audio SDP parsing;
    - timing/reference-clock parsing;
    - final audio SDP ingestion.
- Содержит:
    - `libs/st2110core/include/st2110/audio_sdp_media_section.hpp`
    - `libs/st2110core/include/st2110/audio_sdp_signaling_adapter.hpp`
    - `libs/st2110core/include/st2110/audio_sdp_timing_attributes.hpp`
    - `libs/st2110core/include/st2110/audio_sdp_ingestion.hpp`

### `code_map_shard_10_socket_runtime_and_media_backends.md`
- Область:
    - socket runtime abstraction;
    - platform socket ports;
    - common socket single-media runtime base;
    - concrete socket video/audio backends.
- Содержит:
    - `libs/st2110core/include/st2110/socket_runtime.hpp`
    - `libs/st2110core/include/st2110/socket_stub_rx_port.hpp`
    - `libs/st2110core/include/st2110/linux_socket_rx_port.hpp`
    - `libs/st2110core/include/st2110/socket_rx_single_media_backend_base.hpp`
    - `libs/st2110core/include/st2110/socket_rx_video_backend.hpp`
    - `libs/st2110core/include/st2110/socket_rx_audio_backend.hpp`
    - `libs/st2110core/src/socket_rx_single_media_backend_base.cpp`
    - `libs/st2110core/src/backend_factory_registry.cpp`
    - `libs/st2110core/src/mtl_rx_backend_factory.cpp`
    - `libs/st2110core/include/st2110/mtl_rx_video_backend.hpp`
    - `libs/st2110core/src/mtl_rx_video_backend.cpp`

## Текущая структура кода (file map)

> Назначение раздела:
> - держать краткую карту текущей реализации;
> - уменьшать необходимость пересылать весь код в чат;
> - помогать ассистенту формулировать задачи и делать приемку в контексте реальной архитектуры.
>
> Правила ведения:
> - описываем прежде всего public headers и значимые entry points;
> - фиксируем архитектурную роль, связи и публичные сущности;
> - не дублируем полную реализацию;
> - после принятия задачи обновляем этот раздел вместе с кодом.
>
> Детальные записи ниже подтягиваются из шардов в порядке, зафиксированном в `scripts/rebuild_code_map.sh`.