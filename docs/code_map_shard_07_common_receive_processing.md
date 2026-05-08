# Code map shard 07 — Common receive processing

## Ответственность блока

Это backend-independent receive runtime logic.

Блок отвечает за:
- packet admission после generic parsing;
- reorder abstraction и current reorder implementations;
- video depacketize/reconstruct path;
- audio packet/block assembly path;
- RTP timestamp mapping;
- playout timing helpers.

Блок **не отвечает** за:
- raw SDP parsing;
- backend selection;
- Linux/Winsock syscalls;
- MTL API projection.

`Unsupported` в этом блоке допустим **только в конкретном mode/format/placement/reconstruction branch**, где должна жить нереализованная логика.

## Подблоки

### Shared receive processing
Общие packet admission и reorder abstractions.

### Video receive processing
Backend-independent ST 2110-20 packet -> unit -> frame processing.

### Audio receive processing
Backend-independent ST 2110-30 RTP packet -> audio block processing.

## Файлы блока

### `include/st2110/receive/shared/packet_admission.hpp`
**Источник:** текущий `include/st2110/packet_admission.hpp`

**Ответственность файла:**
- post-parse packet admission against configured session expectations.

**Что нужно упростить:**
- убрать пустые proxy wrappers, которые только переименовывают generic helper без новой ответственности.

### `include/st2110/receive/shared/reorder_buffer.hpp`
**Источник:** текущий `include/st2110/receive/shared/reorder_buffer.hpp`

**Ответственность файла:**
- generic reorder-buffer interface;
- owning stored-packet representation independent from backend transport lifetime;
- reconstruction of `PacketView` from stored packet data.

**Файл сейчас содержит:**
- `StoredPacket` with copied RTP header, extended sequence, SRD headers, and payload bytes;
- `StoredPacket::view()` for rebuilding a transient `PacketView`;
- `IReorderBuffer` interface:
  - `push(...)`;
  - `pop_next()`;
  - `flush_missing_once()`;
  - `reset()`;
  - `stats()`.

**Файл не задает concrete reorder-window policy.**

### `include/st2110/receive/shared/fixed_reorder_buffer.hpp`
**Источник:** текущий `include/st2110/fixed_reorder_buffer.hpp`

**Ответственность файла:**
- current fixed-window reorder implementation.

### `include/st2110/receive/shared/receive_reorder_tolerance_policy.hpp`
**Источник:** текущий `include/st2110/receive_reorder_tolerance_policy.hpp`

**Ответственность файла:**
- modeled gap-tolerance axis for reorder drain behavior.

**Что нужно убрать:**
- enum-validation theater.

### `include/st2110/receive/video/video_receive_description.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/video_receive_capability.hpp`

**Ответственность файла:**
- backend-agnostic video receive description:
  - media description;
  - scan mode;
  - packing mode;
  - transport format;
  - topology;
  - RTP clock.

**Файл не должен знать:**
- project handoff format;
- current `PixelFormat`.

### `include/st2110/receive/video/video_timestamp_mapping.hpp`
**Источник:** текущий `include/st2110/receive/video/video_timestamp_mapping.hpp`

**Ответственность файла:**
- stateful video RTP timestamp -> `TimestampNs` mapping;
- anchor-policy application for video RTP timestamps;
- checked arithmetic helpers used by video timestamp conversion;
- synthetic video timestamp mapping from unit index and frame rate.

**Файл сейчас содержит:**
- `VideoRtpTimestampMapperConfig`;
- `VideoRtpTimestampMapper`;
- checked add/mul helpers for timestamp arithmetic;
- forward RTP delta helper with ambiguous-half-range rejection;
- RTP ticks -> nanoseconds conversion;
- `SyntheticVideoTimestampMapperConfig`;
- `SyntheticVideoTimestampMapper`.

**Файл объединяет два соседних helper-domain:**
- real RTP-based timestamp mapping;
- synthetic frame-index-based timestamp generation.

### `include/st2110/receive/video/video_playout_timing.hpp`
**Источник:** текущий `include/st2110/video_playout_timing.hpp`

**Ответственность файла:**
- receiver-side playout/reconstruction timing helpers.

### `include/st2110/receive/video/video_reorder_policy.hpp`
**Источник:** текущий `include/st2110/video_reorder_policy.hpp`

**Ответственность файла:**
- named video reorder config/policy;
- no actual packet storage implementation.

### `include/st2110/receive/video/frame_write_coverage.hpp`
**Источник:** текущий `include/st2110/frame_write_coverage.hpp`

**Ответственность файла:**
- frame byte-coverage tracking.

### `include/st2110/receive/video/frame_assembler.hpp`
**Источник:** текущий `include/st2110/frame_assembler.hpp`

**Ответственность файла:**
- byte-oriented assembly of one video unit into owning frame storage.

### `include/st2110/receive/video/video_receive_semantics.hpp`
**Источник:** текущий `include/st2110/video_receive_semantics.hpp`

**Ответственность файла:**
- mode-aware receive semantics:
  - assembly unit kind;
  - completion policy;
  - packet grouping key;
  - cross-packet ordering cursor.

### `include/st2110/receive/video/video_segment_constraints.hpp`
**Источник:** текущий `include/st2110/video_segment_constraints.hpp`

**Ответственность файла:**
- format-aware one-segment constraints.

### `include/st2110/receive/video/video_segment_placement.hpp`
**Источник:** текущий `include/st2110/video_segment_placement.hpp`

**Ответственность файла:**
- segment -> frame write mapping.

**Именно здесь допустим `Unsupported`, если конкретный packing/scan/format branch еще не реализован.**

### `include/st2110/receive/video/video_packet_padding.hpp`
**Источник:** текущий `include/st2110/video_packet_padding.hpp`

**Ответственность файла:**
- trailing padding policy per packing/scan-mode branch.

### `include/st2110/receive/video/depacketizer.hpp`
**Источник:** текущий `include/st2110/receive/video/depacketizer.hpp`

**Ответственность файла:**
- packet-to-video-unit assembly over parsed ST 2110-20 packets;
- coordinate current assembly key / cursor / completion policy;
- integrate segment placement, padding validation, and frame-assembler writes;
- emit completed or partial assembled video units.

**Файл сейчас содержит:**
- `DepacketizerConfig`;
- `DepacketizerAssemblyState`;
- `Depacketizer`;
- per-packet assembly-key and completion-policy handling;
- key-transition termination path;
- write-op derivation from segment placement;
- cursor validation for cross-packet ordering;
- stats accounting via `DepacketizerStats`.

**Ошибочная и нереализованная логика локализована через исключения:**
- `std::invalid_argument` for invalid packet/placement/order input;
- `std::logic_error` for unsupported-yet branches in current mode/placement semantics.

### `include/st2110/receive/video/video_unit_reconstructor.hpp`
**Источник:** текущий `include/st2110/video_unit_reconstructor.hpp`

**Ответственность файла:**
- final unit-to-frame reconstruction boundary.

### `include/st2110/receive/video/video_receive_pipeline.hpp`
**Источник:** текущий `include/st2110/video_receive_pipeline.hpp`

**Ответственность файла:**
- compose depacketizer + reconstructor;
- absence of backend/runtime transport concerns.

### `include/st2110/receive/audio/audio_packet.hpp`
**Источник:** текущий `include/st2110/receive/audio/audio_packet.hpp`

**Ответственность файла:**
- normalized typed audio RTP packet model;
- wire-format axis for current PCM payload handling;
- packet policy derived from `RxAudioConfig`;
- exact payload-size admission for audio RTP packets.

**Файл сейчас содержит:**
- `AudioPcmWireFormat`;
- `AudioRtpPacketPolicy`;
- `AudioRtpPacketView`;
- wire-sample-size helper;
- projection from `AudioPcmBitDepth` / `RxAudioConfig` into packet policy;
- payload-size calculation;
- packet-view construction and RTP parse entrypoint.

**Файл не отвечает за:**
- reorder/jitter buffering;
- block assembly into owning audio storage;
- playout timing.

### `include/st2110/receive/audio/audio_reorder_buffer.hpp`
**Источник:** текущий `include/st2110/audio_reorder_buffer.hpp`

**Ответственность файла:**
- audio reorder/jitter buffer boundary.

### `include/st2110/receive/audio/audio_frame_assembler.hpp`
**Источник:** текущий `include/st2110/audio_frame_assembler.hpp`

**Ответственность файла:**
- packet -> owning audio block assembly;
- PCM wire decode into internal audio storage.

### `include/st2110/receive/audio/audio_timestamp_mapping.hpp`
**Источник:** текущий `include/st2110/receive/audio/audio_timestamp_mapping.hpp`

**Ответственность файла:**
- stateful audio RTP timestamp -> `TimestampNs` mapping;
- audio anchor-policy application;
- audio playout timing helpers above mapped media timestamps.

**Файл сейчас содержит:**
- `AudioRtpTimestampMapperConfig`;
- `AudioRtpTimestampMapper`;
- forward-delta helper with ambiguous-half-range rejection;
- RTP-ticks -> nanoseconds conversion;
- playout timing config/decision helpers;
- `AudioBlockTiming` construction.

**Файл объединяет:**
- RTP timestamp mapping;
- simple audio playout timing / block-timing helpers.

### `include/st2110/receive/audio/audio_stats.hpp`
**Источник:** текущий `include/st2110/audio_stats.hpp`

**Ответственность файла:**
- audio-specific observability vocabulary.

### `include/st2110/receive/shared/reorder_stats.hpp`
**Источник:** текущий `include/st2110/receive/shared/reorder_stats.hpp`

**Ответственность файла:**
- dedicated stats vocabulary for reorder-buffer behavior;
- counters for push/store/pop, duplicates, out-of-window, late packets, and flushed missing sequence numbers.

**Файл содержит только typed counters и не содержит reorder logic.**

### `include/st2110/receive/video/depacketizer_stats.hpp`
**Источник:** текущий `include/st2110/receive/video/depacketizer_stats.hpp`

**Ответственность файла:**
- dedicated counter vocabulary for depacketizer throughput and unit outcomes;
- counters for packets in/use and for complete/partial/dropped assembled units.

**Файл содержит только stats-type и не содержит depacketizer logic.**