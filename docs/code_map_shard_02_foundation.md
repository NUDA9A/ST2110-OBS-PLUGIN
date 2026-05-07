# Code map shard 02 — Foundation

## Ответственность блока

Foundation — это нижний слой проекта.

Он отвечает только за:
- базовые примитивы;
- общую семантику ошибок;
- bytes/endian/timestamp;
- общие stats-типы;
- общие derived-value helper'ы без media/backend policy.

Блок **не должен** знать:
- audio/video standard axes;
- backend kind;
- socket family;
- SDP;
- project handoff format;
- MTL API.

## Файлы блока

### `include/st2110/foundation/bytes.hpp`
**Источник:** текущий `include/st2110/bytes.hpp`

**Ответственность файла:**
- единое определение byte-span aliases;
- отсутствие любой другой логики.

### `include/st2110/foundation/endian.hpp`
**Источник:** текущий `include/st2110/endian.hpp`

**Ответственность файла:**
- только endian-aware integer read helpers;
- отсутствие RTP/ST2110/media semantics.

### `include/st2110/foundation/error.hpp`
**Источник:** текущий `include/st2110/error.hpp`

**Ответственность файла:**
- единый semantic vocabulary ошибок проекта;
- включать parse/ingress ошибки и operational/runtime ошибки;
- не кодировать backend-specific private detail.

### `include/st2110/foundation/timestamp.hpp`
**Источник:** текущий `include/st2110/timestamp.hpp`

**Ответственность файла:**
- только базовый internal time type (`TimestampNs`);
- отсутствие mapping policy.

### `include/st2110/foundation/stats.hpp`
**Источник:** текущий `include/st2110/stats.hpp`

**Ответственность файла:**
- общие snapshot/counter types;
- общие helper'ы учета результатов;
- отсутствие media-specific receive semantics.

### `include/st2110/foundation/rtp_timestamp_anchor_policy.hpp`
**Источник:** текущий `include/st2110/rtp_timestamp_anchor_policy.hpp`

**Ответственность файла:**
- общий policy-type initial RTP anchoring;
- использоваться audio/video timestamp mapper'ами;
- не выполнять mapping самостоятельно.

### `include/st2110/foundation/derived_values.hpp` (`NEW`)
**Источник:** полезная часть текущего `include/st2110/config_validation.hpp`

**Ответственность файла:**
- хранить только derived-value helper'ы общего назначения;
- пример: `audio_samples_per_packet_from_rate_and_packet_time(...)`;
- checked arithmetic helpers, если они действительно generic.

**Файл не должен содержать:**
- `validate_video_scan_mode(...)`;
- `validate_rx_backend_kind(...)`;
- `validate_receive_reorder_gap_policy(...)`;
- другие enum-validator'ы ради validator'ов.

## Файлы, которые должны исчезнуть из блока после реструктуризации

### `include/st2110/config_validation.hpp`
**Целевой статус:** удалить.

**Причина:**
файл смешивает:
- полезные derived-value helper'ы;
- transport/value checks;
- enum-validation theater.

После реструктуризации:
- derived values -> `foundation/derived_values.hpp`;
- ingress checks -> ingress block;
- session/runtime checks -> contracts или backend block.
