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
**Источник:** текущий `include/st2110/reorder_buffer.hpp`

**Ответственность файла:**
- generic reorder interface;
- owning stored-packet representation;
- no concrete window policy.

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
**Источник:** текущий `include/st2110/video_timestamp_mapping.hpp`

**Ответственность файла:**
- stateful RTP timestamp -> `TimestampNs` mapping for video receive path.

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
**Источник:** текущий `include/st2110/depacketizer.hpp`

**Ответственность файла:**
- packet-to-video-unit assembly over parsed packet views.

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
**Источник:** текущий `include/st2110/audio_packet.hpp`

**Ответственность файла:**
- normalized typed audio RTP packet model;
- packet policy derived from session config;
- exact payload-size admission.

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
**Источник:** текущий `include/st2110/audio_timestamp_mapping.hpp`

**Ответственность файла:**
- audio RTP timestamp mapping;
- audio playout timing helpers.

### `include/st2110/receive/audio/audio_stats.hpp`
**Источник:** текущий `include/st2110/audio_stats.hpp`

**Ответственность файла:**
- audio-specific observability vocabulary.
