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
**Источник:** текущий `include/st2110/backends/socket/socket_rx_single_media_backend_base.hpp`

**Ответственность файла:**
- shared runtime/lifecycle base for single-media socket RX backends;
- common socket port ownership and lifecycle;
- common receive-thread startup/shutdown;
- common raw datagram receive loop;
- common backend stats accounting.

**Файл сейчас реализует:**
- `IRxBackend` surface for backend name / capabilities / state / stats / stop;
- shared `start_common_runtime(...)`;
- shared `run_receive_loop(...)`;
- default receive-buffer sizing from packet-parse policy;
- RTCP-like datagram detection;
- configured payload-type match helper;
- common record helpers for datagrams, parse rejects, delivered media units.

**Файл не должен знать:**
- video depacketizer internals;
- audio assembly internals;
- backend selection.

**Расширение вниз по иерархии:**
- concrete backend supplies `process_received_datagram(...)`;
- concrete backend supplies media-runtime cleanup;
- concrete backend may augment snapshot with media-specific stats.

### `src/socket_rx_single_media_backend_base.cpp`
**Источник:** текущий `src/socket_rx_single_media_backend_base.cpp`

**Ответственность файла:**
- out-of-line implementation shared socket backend base;
- default platform port-factory selection.

### `include/st2110/backends/socket/socket_rx_video_backend.hpp`
**Источник:** текущий `include/st2110/backends/socket/socket_rx_video_backend.hpp`

**Ответственность файла:**
- concrete socket video RX backend;
- socket-local support boundary for current video receive implementation;
- socket video operational-config construction;
- runtime composition of reorder buffer + receive pipeline + RTP timestamp mapper + sink delivery.

**Файл сейчас содержит:**
- `SocketRxVideoOperationalConfig`;
- `SocketRxVideoSupportPolicy`;
- socket-local implementation-support validators:
    - packing mode;
    - scan mode;
    - topology;
    - RTP clock;
    - current receive-pipeline storage/handoff support;
- builders from `VideoReceiverBootstrapConfig` and from plain `RxVideoConfig`;
- concrete `SocketRxVideoBackend`;
- `SocketRxVideoBackendFactory`.

**Runtime responsibility inside `SocketRxVideoBackend`:**
- parse-policy gate and staged packet parse;
- non-media / RTCP-like datagram filtering;
- reorder-buffer push/drain;
- `VideoReceivePipeline` use for reconstructed frames;
- RTP timestamp -> `TimestampNs` mapping;
- final sink delivery for complete frames only;
- backend-local snapshot augmentation with reorder/depacketizer stats.

**Текущее смешение ответственностей в одном файле:**
- backend-local support policy;
- operational projection;
- concrete runtime implementation;
- factory surface.

Это нужно отражать как текущее co-location, а не как чисто runtime-only файл.

### `include/st2110/backends/socket/socket_rx_audio_backend.hpp`
**Источник:** текущий `include/st2110/socket_rx_audio_backend.hpp`

**Ответственность файла:**
- concrete socket audio receive backend;
- consume audio session config;
- compose packet/reorder/assembly/timestamp/delivery path.

**Файл не должен строить скрытые runtime defaults внутри `start_audio(...)`.**
