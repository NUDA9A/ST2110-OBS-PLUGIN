# Code map shard 09 — Socket backend

## Ответственность блока

Это concrete receive backend поверх собственного socket runtime проекта.

Блок отвечает за:
- backend-local composition поверх session contracts;
- backend-local use of common receive processing;
- lifecycle/start/stop/receive loop integration;
- socket-media runtime glue.

Блок **не должен**:
- переопределять standard media model;
- содержать platform syscall detail;
- содержать raw SDP parsing.

## Файлы блока

### `include/st2110/backends/socket/socket_rx_single_media_backend_base.hpp`
**Источник:** текущий `include/st2110/socket_rx_single_media_backend_base.hpp`

**Ответственность файла:**
- shared runtime/lifecycle base for single-media socket backends;
- common receive thread;
- socket port lifecycle;
- common stats accounting;
- common raw datagram helpers.

**Файл не должен знать:**
- video depacketizer internals;
- audio block assembly internals;
- backend selection.

### `src/socket_rx_single_media_backend_base.cpp`
**Источник:** текущий `src/socket_rx_single_media_backend_base.cpp`

**Ответственность файла:**
- out-of-line implementation shared socket backend base;
- default platform port-factory selection.

### `include/st2110/backends/socket/socket_rx_video_backend.hpp`
**Источник:** текущий `include/st2110/socket_rx_video_backend.hpp`

**Ответственность файла:**
- concrete socket video receive backend;
- consume video session config;
- compose parse/admission/reorder/pipeline/timestamp/delivery path.

**Именно в этом файле должны жить socket-local implementation limits**, а не в standard model.

### `include/st2110/backends/socket/socket_rx_audio_backend.hpp`
**Источник:** текущий `include/st2110/socket_rx_audio_backend.hpp`

**Ответственность файла:**
- concrete socket audio receive backend;
- consume audio session config;
- compose packet/reorder/assembly/timestamp/delivery path.

**Файл не должен строить скрытые runtime defaults внутри `start_audio(...)`.**
