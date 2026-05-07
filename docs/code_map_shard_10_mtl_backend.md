# Code map shard 10 — MTL backend

## Ответственность блока

Это concrete adapter поверх Media Transport Library.

Блок отвечает за:
- MTL-local support branches;
- MTL-local projection from common receive session description to MTL runtime/session config;
- MTL device/session lifecycle;
- frame acquisition/release;
- MTL-local stats.

Блок **не должен**:
- определять standard media model;
- определять project delivery model;
- содержать generic SDP parsing;
- быть второй архитектурой всего проекта.

Цель блока:
- MTL backend должен быть локальной оберткой над MTL;
- `Unsupported` должен оставаться в конкретном projection/runtime branch, а не растекаться по общим слоям.

## Файлы блока

### `include/st2110/backends/mtl/mtl_rx_backend_factory.hpp`
**Источник:** текущий `include/st2110/mtl_rx_backend_factory.hpp`

**Ответственность файла:**
- public factory declarations for MTL backend integration only.

### `src/mtl_rx_backend_factory.cpp`
**Источник:** текущий `src/mtl_rx_backend_factory.cpp`

**Ответственность файла:**
- builtin MTL factory implementation only.

**Что нужно убрать из текущей перегруженной роли:**
- дублирование unavailable/not-built/not-started semantics.

### `include/st2110/backends/mtl/mtl_video_support.hpp` (`NEW`)
**Источник:** выделить из текущих `include/st2110/mtl_rx_video_backend.hpp` и `src/mtl_rx_video_backend.cpp`

**Ответственность файла:**
- только MTL-local support branches;
- только named support boundaries;
- отсутствие lifecycle/runtime object implementation.

### `include/st2110/backends/mtl/mtl_video_projection.hpp` (`NEW`)
**Источник:** выделить из текущих `include/st2110/mtl_rx_video_backend.hpp` и `src/mtl_rx_video_backend.cpp`

**Ответственность файла:**
- common video receive session description -> MTL runtime/session config;
- MTL runtime/session config -> `st20p_rx_ops` projection.

**Именно здесь должны жить mode/format-specific `Unsupported`, если projection branch еще не реализован.**

### `include/st2110/backends/mtl/mtl_rx_video_backend.hpp`
**Источник:** текущий `include/st2110/mtl_rx_video_backend.hpp`

**Новая ответственность файла:**
- только concrete backend object declaration;
- state/lifecycle/stats/start/stop surface;
- отсутствие крупных inline support/projection деревьев.

### `src/mtl_rx_video_backend.cpp`
**Источник:** текущий `src/mtl_rx_video_backend.cpp`

**Новая ответственность файла:**
- `mtl_init` / `mtl_uninit` lifecycle;
- `st20p_rx_create` / `st20p_rx_free` lifecycle;
- `st20p_rx_get_frame` / `st20p_rx_put_frame` runtime flow;
- wake/stop handling;
- MTL-local stats.

## Файлы, которые должны быть распилены ради этого блока

### `include/st2110/mtl_rx_video_backend.hpp`
**Целевой статус:** распилить внутри блока.

### `src/mtl_rx_video_backend.cpp`
**Целевой статус:** распилить внутри блока.

**Распил:**
- support-only branches -> `mtl_video_support.hpp`;
- projection-only branches -> `mtl_video_projection.hpp`;
- backend object/runtime lifecycle -> `mtl_rx_video_backend.*`.
