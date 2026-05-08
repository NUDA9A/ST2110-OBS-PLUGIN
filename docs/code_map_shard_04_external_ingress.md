# Code map shard 04 — External ingress

## Ответственность блока

Это boundary между внешним сырым миром и typed-моделью проекта.

Блок отвечает за:
- strict parsing raw SDP;
- strict parsing raw RTP/ST2110 packet bytes;
- raw -> typed adapter steps;
- reject malformed external input.

Блок **не отвечает** за:
- backend selection;
- runtime session assembly;
- project delivery/storage;
- Socket/MTL lifecycle.

## Подблоки

### Shared packet ingress
Общий ingress packet layer для RTP и ST 2110-20 packet parsing.

### Video ingress
Raw SDP/video attribute parsing и mapping в typed video signaling.

### Audio ingress
Raw SDP/audio attribute parsing и mapping в typed audio signaling.

## Файлы блока

### `include/st2110/ingress/shared/rtp.hpp`
**Источник:** текущий `include/st2110/rtp.hpp`

**Ответственность файла:**
- strict RTP header parsing;
- payload span extraction;
- RTP sequence helper'ы.

**Файл не должен знать:**
- video/audio packet policy;
- depacketizer;
- backend runtime.

### `include/st2110/ingress/shared/st2110_20.hpp`
**Источник:** текущий `include/st2110/st2110_20.hpp`

**Ответственность файла:**
- structural parsing ST 2110-20 payload header;
- SRD header modeling;
- ext-seq combine helper.

**Файл не должен знать:**
- scan-mode receive semantics;
- segment placement;
- runtime support policy.

### `include/st2110/ingress/shared/packet_view.hpp`
**Источник:** текущий `include/st2110/ingress/shared/packet_view.hpp`

**Ответственность файла:**
- normalized parsed packet view over RTP + ST 2110-20 payload header;
- staged parse failure model with precise failing stage;
- SRD segment view construction;
- payload/trailing-padding split.

**Файл сейчас содержит:**
- `SrdSegmentView`;
- `PacketViewParseFailure`;
- `PacketView`;
- `parse_packet_view_staged(...)`;
- `PacketView::from_udp_datagram(...)`.

**Граница файла:**
- structural parsing only;
- no depacketizer policy;
- no backend runtime logic.

### `include/st2110/ingress/shared/packet_parse.hpp`
**Источник:** текущий `include/st2110/ingress/shared/packet_parse.hpp`

**Ответственность файла:**
- packet-parse policy model for UDP datagram size admission;
- effective max-UDP derivation;
- ingress-level packet-size validation;
- final packet-parse entrypoints returning `PacketView`.

**Файл сейчас содержит:**
- standard/extended UDP datagram size constants;
- `PacketParsePolicy`;
- config-validation helper for that policy;
- overload without stats;
- overload with `PacketParseStats` recording.

**Файл не выполняет RTP/ST 2110 structural parse сам по себе:**
staged structural parse lives in `packet_view.hpp`.

### `include/st2110/ingress/video/video_sdp_media_section.hpp`
**Источник:** текущий `include/st2110/video_sdp_media_section.hpp`

**Ответственность файла:**
- raw selection/parsing of `m=video` SDP section;
- raw attribute preservation where needed.

### `include/st2110/ingress/video/video_sdp_fmtp.hpp`
**Источник:** текущий `include/st2110/video_sdp_fmtp.hpp`

**Ответственность файла:**
- raw fmtp parsing for video SDP.

### `include/st2110/ingress/video/video_sdp_rtpmap.hpp`
**Источник:** текущий `include/st2110/video_sdp_rtpmap.hpp`

**Ответственность файла:**
- raw RTPMAP parsing for video SDP.

### `include/st2110/ingress/video/video_sdp_timing_attributes.hpp`
**Источник:** текущий `include/st2110/video_sdp_timing_attributes.hpp`

**Ответственность файла:**
- raw timing/reference-clock parsing for video SDP.

### `include/st2110/ingress/video/video_sdp_signaling_adapter.hpp`
**Источник:** текущий `include/st2110/video_sdp_signaling_adapter.hpp`

**Ответственность файла:**
- map raw parsed video SDP fields into typed `VideoStreamSignaling`.

**Файл не должен строить:**
- runtime config;
- backend config;
- delivery config.

### `include/st2110/ingress/video/video_sdp_ingestion.hpp`
**Источник:** текущий `include/st2110/video_sdp_ingestion.hpp`

**Ответственность файла:**
- final video SDP ingress entrypoint;
- orchestration raw selection + timing parse + typed signaling mapping.

### `include/st2110/ingress/audio/audio_sdp_media_section.hpp`
**Источник:** текущий `include/st2110/audio_sdp_media_section.hpp`

**Ответственность файла:**
- raw audio SDP media-section parsing;
- payload-type/media-line transport admission on raw input boundary.

### `include/st2110/ingress/audio/audio_sdp_timing_attributes.hpp`
**Источник:** текущий `include/st2110/audio_sdp_timing_attributes.hpp`

**Ответственность файла:**
- raw audio timing/reference-clock parsing.

### `include/st2110/ingress/audio/audio_sdp_signaling_adapter.hpp`
**Источник:** текущий `include/st2110/audio_sdp_signaling_adapter.hpp`

**Ответственность файла:**
- map raw parsed audio SDP fields into typed `AudioStreamSignaling`.

### `include/st2110/ingress/audio/audio_sdp_ingestion.hpp`
**Источник:** текущий `include/st2110/audio_sdp_ingestion.hpp`

**Ответственность файла:**
- final audio SDP ingress entrypoint;
- orchestration raw selection + timing parse + typed signaling mapping.

### `include/st2110/ingress/shared/packet_parse_stats.hpp`
**Источник:** текущий `include/st2110/ingress/shared/packet_parse_stats.hpp`

**Ответственность файла:**
- staged packet-parse observability vocabulary;
- parse-stage taxonomy via `PacketParseStage`;
- aggregate parser counters via `ParserStats` and `PacketParseStats`;
- helper functions for recording parse outcomes.

**Файл сейчас используется как dedicated stats boundary для:**
- `packet_view.hpp`;
- `packet_parse.hpp`;
- backend-level stats aggregation.
