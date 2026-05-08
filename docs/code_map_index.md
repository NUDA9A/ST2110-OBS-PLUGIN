# ST2110-OBS-PLUGIN — code map index (target block layout)

Этот code map описывает **целевое разбиение репозитория на блоки ответственности**.

Ключевые принципы карты:
- карта отражает **архитектурные блоки**, а не только текущий плоский список файлов;
- каждый production-файл относится **ровно к одному блоку ответственности**;
- если текущий файл смешивает несколько ответственностей, в карте он **распилен** на несколько целевых файлов;
- `NEW` означает, что файл предлагается создать при реструктуризации;
- шарды описывают **весь блок целиком**, а внутри шарда при необходимости описываются подблоки.

---

## Блоки

### 01. Support / build / entrypoints
**Ответственность блока:**
- сборка;
- wiring внешних зависимостей;
- entrypoints приложений/плагина;
- отсутствие media/runtime/business-logic.

**Shard:** `docs/code_map_shard_01_support_build_and_entrypoints.md`

**Файлы блока:**
- `libs/st2110core/CMakeLists.txt`
- `apps/st2110_rx_dump/main.cpp`
- `libs/st2110core/src/stub.cpp`
- `plugins/obs_st2110/*` (после появления plugin layer)

---

### 02. Foundation
**Ответственность блока:**
- базовые примитивы проекта;
- общий vocabulary ошибок;
- timestamp/bytes/endian;
- общие stats-типы;
- общие derived-value helper'ы без media/backend policy.

**Shard:** `docs/code_map_shard_02_foundation.md`

**Файлы блока:**
- `include/st2110/foundation/bytes.hpp`
- `include/st2110/foundation/endian.hpp`
- `include/st2110/foundation/error.hpp`
- `include/st2110/foundation/timestamp.hpp`
- `include/st2110/foundation/stats.hpp`
- `include/st2110/foundation/rtp_timestamp_anchor_policy.hpp`
- `include/st2110/foundation/derived_values.hpp` (`NEW`)

---

### 03. Standard media model
**Ответственность блока:**
- полное typed-описание осей, заданных стандартом;
- video/audio media model;
- signaling model;
- channel-order model;
- отсутствие project delivery limits, backend support limits и platform/runtime detail.

**Shard:** `docs/code_map_shard_03_standard_media_model.md`

**Подблоки:**
- video standard model;
- audio standard model.

**Файлы блока:**
- `include/st2110/model/video/video_scan_mode.hpp`
- `include/st2110/model/video/video_packing_mode.hpp`
- `include/st2110/model/video/video_media_types.hpp` (`NEW`)
- `include/st2110/model/video/video_signaling_types.hpp` (`NEW`)
- `include/st2110/model/audio/audio_signaling.hpp`
- `include/st2110/model/audio/audio_channel_order.hpp`

---

### 04. External ingress
**Ответственность блока:**
- strict parsing внешних данных;
- raw SDP ingestion;
- raw RTP/ST2110 packet parsing;
- raw -> typed adapter boundary.

**Shard:** `docs/code_map_shard_04_external_ingress.md`

**Подблоки:**
- shared packet ingress;
- video ingress;
- audio ingress.

**Файлы блока:**
- `include/st2110/ingress/shared/rtp.hpp`
- `include/st2110/ingress/shared/st2110_20.hpp`
- `include/st2110/ingress/shared/packet_view.hpp`
- `include/st2110/ingress/shared/packet_parse.hpp`
- `include/st2110/ingress/video/video_sdp_media_section.hpp`
- `include/st2110/ingress/video/video_sdp_fmtp.hpp`
- `include/st2110/ingress/video/video_sdp_rtpmap.hpp`
- `include/st2110/ingress/video/video_sdp_timing_attributes.hpp`
- `include/st2110/ingress/video/video_sdp_signaling_adapter.hpp`
- `include/st2110/ingress/video/video_sdp_ingestion.hpp`
- `include/st2110/ingress/audio/audio_sdp_media_section.hpp`
- `include/st2110/ingress/audio/audio_sdp_timing_attributes.hpp`
- `include/st2110/ingress/audio/audio_sdp_signaling_adapter.hpp`
- `include/st2110/ingress/audio/audio_sdp_ingestion.hpp`

---

### 05. Receive session contracts
**Ответственность блока:**
- backend API и backend selection contracts;
- backend-independent receive session config;
- signaling -> session projection;
- receiver bootstrap aggregates;
- отсутствие packet parsing, OS runtime detail и delivery/storage implementation.

**Shard:** `docs/code_map_shard_05_receive_session_contracts.md`

**Подблоки:**
- backend contracts;
- video session contracts;
- audio session contracts.

**Файлы блока:**
- `include/st2110/contracts/backend/backend.hpp`
- `include/st2110/contracts/backend/backend_factory.hpp`
- `include/st2110/contracts/backend/backend_factory_registry.hpp` (`NEW`)
- `src/backend_factory_registry.cpp`
- `include/st2110/contracts/video/rx_video_session_config.hpp` (`NEW`)
- `include/st2110/contracts/video/video_receiver_bootstrap.hpp` (`NEW`)
- `include/st2110/contracts/video/video_backend_selection.hpp` (`NEW`)
- `include/st2110/contracts/audio/rx_audio_session_config.hpp` (`NEW`)
- `include/st2110/contracts/audio/audio_signaling_rx_config.hpp`
- `include/st2110/contracts/audio/audio_receiver_bootstrap.hpp`

---

### 06. Delivery and conversion
**Ответственность блока:**
- project-local storage и handoff contracts;
- video/audio frame view types;
- conversion boundaries;
- separation between receive capability and project delivery capability.

**Shard:** `docs/code_map_shard_06_delivery_and_conversion.md`

**Подблоки:**
- video delivery;
- audio delivery.

**Файлы блока:**
- `include/st2110/delivery/video/pixel_format.hpp`
- `include/st2110/delivery/video/video_handoff_format.hpp` (`NEW`)
- `include/st2110/delivery/video/video_frame.hpp`
- `include/st2110/delivery/video/video_frame_conversion.hpp` (`NEW`)
- `include/st2110/delivery/audio/audio_frame.hpp`
- `include/st2110/delivery/audio/audio_frame_conversion.hpp` (`NEW`)

---

### 07. Common receive processing
**Ответственность блока:**
- backend-independent media receive logic;
- packet admission/reorder;
- video depacketize/reconstruct path;
- audio packet/block assembly path;
- RTP timestamp mapping и playout timing helpers.

**Shard:** `docs/code_map_shard_07_common_receive_processing.md`

**Подблоки:**
- shared receive processing;
- video receive processing;
- audio receive processing.

**Файлы блока:**
- `include/st2110/receive/shared/packet_admission.hpp`
- `include/st2110/receive/shared/reorder_buffer.hpp`
- `include/st2110/receive/shared/fixed_reorder_buffer.hpp`
- `include/st2110/receive/shared/receive_reorder_tolerance_policy.hpp`
- `include/st2110/receive/video/video_receive_description.hpp` (`NEW`)
- `include/st2110/receive/video/video_timestamp_mapping.hpp`
- `include/st2110/receive/video/video_playout_timing.hpp`
- `include/st2110/receive/video/video_reorder_policy.hpp`
- `include/st2110/receive/video/frame_write_coverage.hpp`
- `include/st2110/receive/video/frame_assembler.hpp`
- `include/st2110/receive/video/video_receive_semantics.hpp`
- `include/st2110/receive/video/video_segment_constraints.hpp`
- `include/st2110/receive/video/video_segment_placement.hpp`
- `include/st2110/receive/video/video_packet_padding.hpp`
- `include/st2110/receive/video/depacketizer.hpp`
- `include/st2110/receive/video/video_unit_reconstructor.hpp`
- `include/st2110/receive/video/video_receive_pipeline.hpp`
- `include/st2110/receive/audio/audio_packet.hpp`
- `include/st2110/receive/audio/audio_reorder_buffer.hpp`
- `include/st2110/receive/audio/audio_frame_assembler.hpp`
- `include/st2110/receive/audio/audio_timestamp_mapping.hpp`
- `include/st2110/receive/audio/audio_stats.hpp`

---

### 08. Socket platform adapters
**Ответственность блока:**
- platform-neutral socket transport contract;
- Linux/stub/Windows platform adapters;
- отсутствие media logic.

**Shard:** `docs/code_map_shard_08_socket_platform_adapters.md`

**Файлы блока:**
- `include/st2110/backends/socket/platform/socket_runtime.hpp`
- `include/st2110/backends/socket/platform/socket_stub_rx_port.hpp`
- `include/st2110/backends/socket/platform/linux_socket_rx_port.hpp`
- `include/st2110/backends/socket/platform/windows_socket_rx_port.hpp` (`FUTURE`)

---

### 09. Socket backend
**Ответственность блока:**
- concrete socket receive backend composition;
- backend-local runtime assembly over session contracts + common receive processing;
- отсутствие platform syscall detail в media code и отсутствие redefinition of standard model.

**Shard:** `docs/code_map_shard_09_socket_backend.md`

**Файлы блока:**
- `include/st2110/backends/socket/socket_rx_single_media_backend_base.hpp`
- `src/socket_rx_single_media_backend_base.cpp`
- `include/st2110/backends/socket/socket_rx_video_backend.hpp`
- `include/st2110/backends/socket/socket_rx_audio_backend.hpp`

---

### 10. MTL backend
**Ответственность блока:**
- concrete adapter над MTL runtime/API;
- backend-local support/projection/runtime lifecycle;
- отсутствие generic signaling/parser/backend-selection responsibility.

**Shard:** `docs/code_map_shard_10_mtl_backend.md`

**Файлы блока:**
- `include/st2110/backends/mtl/mtl_rx_backend_factory.hpp`
- `src/mtl_rx_backend_factory.cpp`
- `include/st2110/backends/mtl/mtl_video_support.hpp` (`NEW`)
- `include/st2110/backends/mtl/mtl_video_projection.hpp` (`NEW`)
- `include/st2110/backends/mtl/mtl_rx_video_backend.hpp`
- `src/mtl_rx_video_backend.cpp`

---

### 11. OBS plugin / composition
**Ответственность блока:**
- OBS-facing source/plugin layer;
- UI/settings/runtime wiring;
- backend selection orchestration;
- OBS handoff adapters;
- отсутствие низкоуровневой receive logic.

**Shard:** `docs/code_map_shard_11_obs_plugin_composition.md`

**Файлы блока:**
- `plugins/obs_st2110/include/obs_st2110/plugin_api.hpp` (`NEW`)
- `plugins/obs_st2110/include/obs_st2110/source_config.hpp` (`NEW`)
- `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp` (`NEW`)
- `plugins/obs_st2110/src/plugin_entry.cpp` (`NEW`)
- `plugins/obs_st2110/src/source_registration.cpp` (`NEW`)
- `plugins/obs_st2110/src/source_settings_ui.cpp` (`NEW`)
- `plugins/obs_st2110/src/source_runtime.cpp` (`NEW`)
- `plugins/obs_st2110/src/backend_wiring.cpp` (`NEW`)
- `plugins/obs_st2110/src/obs_video_handoff.cpp` (`NEW`)
- `plugins/obs_st2110/src/obs_audio_handoff.cpp` (`NEW`)

---

## Правила чтения карты

1. Сначала читать `code_map_index.md`, чтобы понять блоки и связи между ними.
2. Затем читать shard нужного блока.
3. Если файл отмечен как `NEW`, это означает **целевой файл после распила смешанной ответственности**.
4. Если текущий файл отсутствует в новой карте как самостоятельный файл, значит он:
   - либо переехал в другой блок;
   - либо должен быть распилен;
   - либо должен быть удален как лишний слой.

---

## Ключевые распилы текущих смешанных файлов

### `include/st2110/rx_config.hpp`
Целевой статус: удалить.
- video session config -> `contracts/video/rx_video_session_config.hpp`;
- audio session config -> `contracts/audio/rx_audio_session_config.hpp`.

### `include/st2110/signaling_structs.hpp`
Целевой статус: удалить.
- typed signaling model -> `model/video/video_signaling_types.hpp`;
- bootstrap aggregate -> `contracts/video/video_receiver_bootstrap.hpp`.

### `include/st2110/video_receive_capability.hpp`
Целевой статус: удалить/распилить.
- standard media axes -> `model/video/video_media_types.hpp`;
- receive description -> `receive/video/video_receive_description.hpp`;
- project handoff axis -> `delivery/video/video_handoff_format.hpp`.

### `include/st2110/video_signaling.hpp`
Целевой статус: распилить.
- typed signaling types -> model block;
- signaling -> session/bootstrap projection -> contracts block;
- raw SDP mapping stays in ingress block.

### `include/st2110/mtl_rx_video_backend.hpp` и `src/mtl_rx_video_backend.cpp`
Целевой статус: распилить внутри MTL block.
- support-only branches -> `mtl_video_support.hpp`;
- projection-only branches -> `mtl_video_projection.hpp`;
- lifecycle/runtime object -> `mtl_rx_video_backend.*`.
