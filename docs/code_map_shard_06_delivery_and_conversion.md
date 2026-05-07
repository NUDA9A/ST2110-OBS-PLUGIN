# Code map shard 06 — Delivery and conversion

## Ответственность блока

Блок отвечает за **project-local representation and handoff**, а не за standard receive capability.

Он отвечает за:
- current storage formats;
- frame/audio view contracts;
- project handoff formats;
- conversion boundaries.

Блок **не отвечает** за:
- стандартные media axes;
- raw SDP/packet parsing;
- backend selection;
- Linux/MTL runtime.

Главный смысл блока:
- receive capability и delivery capability существуют раздельно;
- backend может уметь принять больше, чем проект сейчас умеет выдать наружу.

## Подблоки

### Video delivery
Project-local video storage/handoff contracts.

### Audio delivery
Project-local audio storage/handoff contracts.

## Файлы блока

### `include/st2110/delivery/video/pixel_format.hpp`
**Источник:** текущий `include/st2110/pixel_format.hpp`

**Ответственность файла:**
- current project-owned pixel/storage format axis.

**Важно:**
это **не standard media axis**, а только project storage axis.

### `include/st2110/delivery/video/video_handoff_format.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/video_receive_capability.hpp`

**Ответственность файла:**
- current/future project-local video handoff format axis;
- mapping boundary between receive-side result and project-visible handoff contract.

### `include/st2110/delivery/video/video_frame.hpp`
**Источник:** текущий `include/st2110/video_frame.hpp`

**Ответственность файла:**
- owning video frame storage;
- `VideoFrameView` as non-owning delivery contract.

### `include/st2110/delivery/video/video_frame_conversion.hpp` (`NEW`)
**Ответственность файла:**
- explicit conversion boundary:
  - direct handoff;
  - converted handoff;
  - unsupported handoff branch.

**Файл нужен как named extension point**, даже если initially он будет mostly placeholder.

### `include/st2110/delivery/audio/audio_frame.hpp`
**Источник:** текущий `include/st2110/audio_frame.hpp`

**Ответственность файла:**
- owning audio block/buffer storage;
- `AudioFrameView` as non-owning delivery contract.

### `include/st2110/delivery/audio/audio_frame_conversion.hpp` (`NEW`)
**Ответственность файла:**
- explicit conversion/handoff boundary for audio delivery;
- future bridge to OBS audio contracts.

## Файлы, которые должны быть распилены ради этого блока

### `include/st2110/video_receive_capability.hpp`
**Целевой статус:** удалить/распилить.

**Распил для этого блока:**
- project handoff types и handoff-related helpers -> `delivery/video/video_handoff_format.hpp`.
