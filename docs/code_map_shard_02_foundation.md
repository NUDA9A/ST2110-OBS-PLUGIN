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
**Источник:** текущий `include/st2110/foundation/error.hpp`

**Ответственность файла:**
- common project-wide error vocabulary;
- co-locate ingress/parse errors and backend/runtime operational errors in one enum `Error`;
- provide basic stringification and backend-runtime classification helpers.

**Файл сейчас содержит:**
- `Error` enum for parse, validation, backend-state, socket-bind/multicast/receive failures;
- `to_string(...)`;
- `is_backend_runtime_error(...)`.

### `include/st2110/foundation/timestamp.hpp`
**Источник:** текущий `include/st2110/timestamp.hpp`

**Ответственность файла:**
- только базовый internal time type (`TimestampNs`);
- отсутствие mapping policy.

### `include/st2110/foundation/stats.hpp`
**Источник:** текущий `include/st2110/foundation/stats.hpp`

**Ответственность файла:**
- common stats/counter vocabulary used across receive backends;
- backend-wide snapshot aggregation via `BackendStats`;
- helper functions for recording packet-parse outcomes.

**Файл сейчас дополнительно co-locates:**
- `ReorderBufferStats`;
- `PacketParseStage`;
- `ParserStats`;
- `PacketParseStats`;
- `DepacketizerStats`;
- `record_parse_result(...)`;
- `record_packet_parse_result(...)`.

**Это нужно отразить как текущее состояние:**
backend-wide snapshot и subsystem-specific stats/types пока еще частично живут в одном foundation header-е, несмотря на появление более узких dedicated stats headers.

### `include/st2110/foundation/rtp_timestamp_anchor_policy.hpp`
**Источник:** текущий `include/st2110/foundation/rtp_timestamp_anchor_policy.hpp`

**Ответственность файла:**
- shared initial-anchor policy type for RTP timestamp mapping;
- common enum `RtpTimestampInitialAnchorMode`;

**Файл используется как foundation-level policy header для:**
- video RTP timestamp mapping;
- audio RTP timestamp mapping.

**Файл не выполняет mapping самостоятельно.**

### `include/st2110/foundation/derived_values.hpp`
**Источник:** текущий `include/st2110/foundation/derived_values.hpp`

**Ответственность файла:**
- reserved foundation location for generic derived-value helpers;
- generic checked arithmetic / value-derivation helpers when they are independent of media/backend policy.

**Текущее содержимое файла:**
- header placeholder only;
- public helpers are not implemented yet.

**Это важно отразить в карте:**
файл уже существует как отдельная точка расширения, но пока еще не несет фактической helper-логики.

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
