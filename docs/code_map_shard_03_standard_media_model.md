# Code map shard 03 — Standard media model

## Ответственность блока

Блок описывает **typed-модель стандарта**.

Он отвечает за:
- полное выражение осей, заданных ST 2110 и связанными signaling conventions;
- video/audio media model;
- video/audio signaling model;
- channel-order model.

Блок **не отвечает** за:
- parsing raw SDP или пакетов;
- backend support limits;
- project delivery/storage limits;
- socket/MTL runtime details.

Если стандартную ось можно выразить типом, enum'ом или structured model — она должна жить **здесь**.

## Подблоки

### Video standard model
Содержит все standard-facing video axes и signaling types.

### Audio standard model
Содержит all standard-facing audio axes, stream signaling model и channel-order model.

## Файлы блока

### `include/st2110/model/video/video_scan_mode.hpp`
**Источник:** текущий `include/st2110/video_scan_mode.hpp`

**Ответственность файла:**
- только modeled scan-mode axis:
  - Progressive;
  - Interlaced;
  - PsF.

**Файл не должен содержать:**
- support policy;
- helper'ы runtime narrowing;
- validator'ы enum value coverage.

### `include/st2110/model/video/video_packing_mode.hpp`
**Источник:** текущий `include/st2110/video_packing_mode.hpp`

**Ответственность файла:**
- только modeled packing-mode axis:
  - Gpm;
  - Bpm;
  - GpmSingleLine.

**Что нужно убрать из текущей роли файла:**
- runtime support helpers вроде `validate_runtime_video_packing_mode_support(...)`.

### `include/st2110/model/video/video_media_types.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/video_receive_capability.hpp`

**Ответственность файла:**
- standard-facing video media types;
- token-backed modeled axes и structured media description.

**В файл должны попасть:**
- `VideoSampling`;
- `VideoBitDepth`;
- `VideoColorimetry`;
- `VideoTransferCharacteristicSystem`;
- `VideoSignalStandard`;
- `VideoRange`;
- `VideoMediaDescription`;
- `VideoTransportPayloadFormat`.

**В файл не должны попадать:**
- project handoff format;
- current `PixelFormat`;
- project delivery compatibility helpers;
- backend support logic.

### `include/st2110/model/video/video_signaling_types.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/signaling_structs.hpp`

**Ответственность файла:**
- только typed signaling model video stream.

**В файл должны попасть:**
- `MediaClockMode`;
- `TimestampMode`;
- `ReferenceClockKind`;
- `PtpReferenceClock`;
- `LocalMacReferenceClock`;
- `ReferenceClock`;
- `VideoSenderType`;
- `VideoStreamSignaling`.

**В файл не должен попадать:**
- receiver bootstrap aggregate;
- runtime config;
- backend-specific projection.

### `include/st2110/model/audio/audio_signaling.hpp`
**Источник:** текущий `include/st2110/audio_signaling.hpp`

**Ответственность файла:**
- standard-facing audio signaling model;
- conformance-level axes;
- PCM encoding/bit-depth axes;
- `AudioMediaDescription`;
- `AudioStreamSignaling`.

**После очистки файл не должен играть роль runtime support boundary.**
Он должен оставаться model-first файлом.

### `include/st2110/model/audio/audio_channel_order.hpp`
**Источник:** текущий `include/st2110/audio_channel_order.hpp`

**Ответственность файла:**
- normalized typed audio channel-order model;
- `AudioChannelGroupKind`;
- `AudioChannelOrderGroup`;
- `ParsedAudioChannelOrder`.

**Почему файл здесь:**
это still standard/signaling abstraction, а не backend/runtime logic.

## Файлы, которые должны быть распилены ради этого блока

### `include/st2110/signaling_structs.hpp`
**Целевой статус:** удалить.

**Распил:**
- signaling types -> `model/video/video_signaling_types.hpp`;
- bootstrap aggregate -> contracts block.

### `include/st2110/video_receive_capability.hpp`
**Целевой статус:** удалить/распилить.

**Распил для этого блока:**
- standard media axes -> `model/video/video_media_types.hpp`.
