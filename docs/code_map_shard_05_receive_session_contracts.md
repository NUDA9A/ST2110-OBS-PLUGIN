# Code map shard 05 — Receive session contracts

## Ответственность блока

Блок описывает то, что нужно для построения и запуска receive session **без привязки к конкретной backend реализации**.

Он отвечает за:
- backend public API;
- backend selection contracts;
- receive session config types;
- signaling -> session projection;
- receiver bootstrap aggregates.

Блок **не отвечает** за:
- raw packet parsing;
- OS/platform socket runtime;
- MTL API projection;
- project frame/audio storage implementation.

## Подблоки

### Backend contracts
Generic receive backend interfaces и factory/selection contracts.

### Video session contracts
Backend-agnostic video session config и signaling/bootstrap projection.

### Audio session contracts
Backend-agnostic audio session config и signaling/bootstrap projection.

## Файлы блока

### `include/st2110/contracts/backend/backend.hpp`
**Источник:** текущий `include/st2110/backend.hpp`

**Ответственность файла:**
- `IRxBackend`, `IRxVideoBackend`, `IRxAudioBackend`;
- sink interfaces;
- backend lifecycle/state/stats contract.

**Файл не должен знать:**
- concrete socket/MTL runtime implementation details.

### `include/st2110/contracts/backend/backend_factory.hpp`
**Источник:** текущий `include/st2110/backend_factory.hpp`

**Ответственность файла:**
- `RxBackendKind`;
- `RxBackendDescriptor`;
- `RxBackendSelection`;
- `IRxBackendFactory`;
- generic backend creation/selection API.

**Что нужно убрать из текущей перегруженной роли файла:**
- video-specific config-aware support dispatch.

### `include/st2110/contracts/backend/backend_factory_registry.hpp` (`NEW`)
**Источник:** выделить из текущего `backend_factory.hpp` и `src/backend_factory_registry.cpp`

**Ответственность файла:**
- declaration-only surface builtin registry:
  - `default_rx_backend_factories()`;
  - `rx_backend_kind_built(...)`.

### `src/backend_factory_registry.cpp`
**Источник:** текущий `src/backend_factory_registry.cpp`

**Ответственность файла:**
- builtin registry composition и registration inventory;
- platform/build-level backend presence.

**Файл не должен содержать:**
- media parsing;
- backend-local runtime support logic.

### `include/st2110/contracts/video/rx_video_session_config.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/rx_config.hpp`

**Ответственность файла:**
- video receive session config type;
- transport/session fields, которые нужны backend'у;
- отсутствие project delivery narrowing и backend-specific support policy.

### `include/st2110/contracts/video/video_receiver_bootstrap.hpp` (`NEW`)
**Источник:** собрать из текущих `signaling_structs.hpp` и `video_signaling.hpp`

**Ответственность файла:**
- `VideoReceiverBootstrapConfig`;
- signaling -> bootstrap projection for video receive path.

**Файл не должен знать:**
- socket port internals;
- MTL `st20p_rx_ops`.

### `include/st2110/contracts/video/video_backend_selection.hpp` (`NEW`)
**Источник:** вынести из текущих `backend_factory.hpp`, `video_backend_support.hpp`, `src/backend_factory_registry.cpp`

**Ответственность файла:**
- video backend selection helpers above concrete backend support branches;
- dispatch selected backend to backend-local support boundary.

### `include/st2110/contracts/audio/rx_audio_session_config.hpp` (`NEW`)
**Источник:** выделить из текущего `include/st2110/rx_config.hpp`

**Ответственность файла:**
- audio receive session config type;
- only backend-agnostic runtime/session inputs.

### `include/st2110/contracts/audio/audio_signaling_rx_config.hpp`
**Источник:** текущий `include/st2110/audio_signaling_rx_config.hpp`

**Ответственность файла:**
- projection from typed `AudioStreamSignaling` into audio session config.

### `include/st2110/contracts/audio/audio_receiver_bootstrap.hpp`
**Источник:** текущий `include/st2110/audio_receiver_bootstrap.hpp`

**Ответственность файла:**
- final audio bootstrap aggregate;
- collect session config, packet policy, reorder config, timestamp mapping config, channel-order result.

## Файлы, которые должны быть распилены ради этого блока

### `include/st2110/rx_config.hpp`
**Целевой статус:** удалить.

**Распил:**
- video session config -> `contracts/video/rx_video_session_config.hpp`;
- audio session config -> `contracts/audio/rx_audio_session_config.hpp`.

### `include/st2110/signaling_structs.hpp`
**Целевой статус:** удалить.

**Распил для этого блока:**
- bootstrap aggregate -> `contracts/video/video_receiver_bootstrap.hpp`.

### `include/st2110/video_signaling.hpp`
**Целевой статус:** распилить.

**Распил для этого блока:**
- signaling -> session/bootstrap projection helpers -> `contracts/video/video_receiver_bootstrap.hpp`.
