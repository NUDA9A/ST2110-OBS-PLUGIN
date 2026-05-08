# Code map shard 08 — Socket platform adapters

## Ответственность блока

Блок отвечает за platform/socket transport layer.

Он содержит:
- OS-neutral socket runtime contract;
- concrete Linux adapter;
- concrete stub adapter;
- future Windows adapter.

Блок **не должен** содержать:
- media model;
- packet parsing;
- depacketizer/assembler logic;
- backend selection.

Это именно тот блок, который делает future Windows support локальным расширением transport layer.

## Файлы блока

### `include/st2110/backends/socket/platform/socket_runtime.hpp`
**Источник:** текущий `include/st2110/backends/socket/platform/socket_runtime.hpp`

**Ответственность файла:**
- platform-neutral socket RX transport contract;
- typed address-family / endpoint / multicast-membership / open-config model;
- `ISocketRxPort` и `ISocketRxPortFactory` interfaces;
- socket-open operational common config shared by socket backends.

**Файл сейчас дополнительно содержит:**
- text-level IPv4 / IPv6 address validation helpers;
- multicast-address classification helpers;
- config equality helpers for socket-open structures;
- `build_socket_rx_open_config(...)`;
- projection helpers from `RxVideoConfig` / `RxAudioConfig` into `SocketRxOpenConfig`.

**Граница файла:**
- знает packet-parse policy и socket-open projection from session config;
- не знает media parsing internals;
- не знает depacketizer / audio assembly;
- не содержит concrete Linux syscall implementation.

### `include/st2110/backends/socket/platform/socket_stub_rx_port.hpp`
**Источник:** текущий `include/st2110/socket_stub_rx_port.hpp`

**Ответственность файла:**
- non-networking fallback implementation of socket port contract;
- build/runtime fallback when real platform transport is absent.

### `include/st2110/backends/socket/platform/linux_socket_rx_port.hpp`
**Источник:** текущий `include/st2110/backends/socket/platform/linux_socket_rx_port.hpp`

**Ответственность файла:**
- concrete Linux implementation of `ISocketRxPort`;
- native UDP socket lifecycle:
    - create;
    - configure;
    - bind;
    - join/leave multicast;
    - receive;
    - close;
- map Linux socket/`errno` failures into project `Error`.

**Файл сейчас реализует:**
- IPv4 bind/open path;
- IPv6 bind path;
- IPv4 multicast join/leave через `IP_ADD_MEMBERSHIP` / `IP_DROP_MEMBERSHIP`;
- localized `Unsupported` for IPv6 multicast membership;
- backend-local factory `LinuxSocketRxPortFactory` и helper `make_linux_socket_rx_port_factory()`.

**Файл не содержит:**
- packet parsing;
- video/audio receive logic;
- backend selection.

### `include/st2110/backends/socket/platform/windows_socket_rx_port.hpp` (`FUTURE`)
**Ответственность файла:**
- future Winsock implementation of the same port contract.

**Почему файл обязателен в целевой карте уже сейчас:**
чтобы Windows expansion была **добавлением platform adapter**, а не переписыванием Socket media backend.
