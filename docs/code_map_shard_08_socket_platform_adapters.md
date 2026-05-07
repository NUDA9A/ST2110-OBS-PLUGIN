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
**Источник:** текущий `include/st2110/socket_runtime.hpp`

**Ответственность файла:**
- platform-neutral socket transport contract;
- address-family model;
- bind/multicast/open config model;
- receive port interface и port factory interface.

**Файл не должен знать:**
- video/audio parsing;
- video/audio receive pipeline;
- Socket backend media logic.

### `include/st2110/backends/socket/platform/socket_stub_rx_port.hpp`
**Источник:** текущий `include/st2110/socket_stub_rx_port.hpp`

**Ответственность файла:**
- non-networking fallback implementation of socket port contract;
- build/runtime fallback when real platform transport is absent.

### `include/st2110/backends/socket/platform/linux_socket_rx_port.hpp`
**Источник:** текущий `include/st2110/linux_socket_rx_port.hpp`

**Ответственность файла:**
- Linux UDP socket implementation of `ISocketRxPort`;
- bind/join/leave/recv error mapping below platform-neutral contract.

### `include/st2110/backends/socket/platform/windows_socket_rx_port.hpp` (`FUTURE`)
**Ответственность файла:**
- future Winsock implementation of the same port contract.

**Почему файл обязателен в целевой карте уже сейчас:**
чтобы Windows expansion была **добавлением platform adapter**, а не переписыванием Socket media backend.
