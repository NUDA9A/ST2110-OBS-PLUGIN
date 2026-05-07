# Code map shard 01 — Support / build / entrypoints

## Ответственность блока

Блок отвечает за:
- build integration;
- wiring внешних зависимостей;
- entrypoints отдельных приложений и plugin-layer;
- отсутствие media model, receive pipeline и backend runtime logic.

Этот блок **не должен**:
- определять оси стандарта;
- содержать packet parsing;
- содержать socket/MTL runtime assembly;
- содержать project delivery contracts.

## Файлы блока

### `libs/st2110core/CMakeLists.txt`
**Ответственность файла:**
- собрать `st2110core`;
- локализовать build-time wiring зависимостей;
- локализовать platform-derived build boundary для MTL backend;
- не быть местом, где живет backend selection policy или media logic.

**Что файл может знать:**
- source set;
- target definitions;
- compile definitions;
- pkg-config wiring;
- platform gating.

**Что файл не должен знать:**
- SDP;
- packet parsing;
- receive capability;
- Socket/MTL runtime semantics.

### `apps/st2110_rx_dump/main.cpp`
**Ответственность файла:**
- минимальный app/tool entrypoint;
- buildable stub или будущий CLI tool;
- не plugin layer.

**Что файл может знать:**
- top-level app startup;
- orchestration вызовов public API.

**Что файл не должен знать:**
- внутренние backend implementation details;
- packet/depacketizer internals.

### `libs/st2110core/src/stub.cpp`
**Ответственность файла:**
- временная build unit;
- отсутствие архитектурной ответственности.

**Целевой статус:**
- удалить, когда реальных `.cpp` в библиотеке станет достаточно.

### `plugins/obs_st2110/include/obs_st2110/plugin_api.hpp` (`NEW`)
**Ответственность файла:**
- public plugin-level API surface;
- registration-visible declarations;
- отсутствие backend/runtime internals.

### `plugins/obs_st2110/include/obs_st2110/source_config.hpp` (`NEW`)
**Ответственность файла:**
- OBS-facing persisted source settings model;
- отсутствие direct runtime start logic.

### `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp` (`NEW`)
**Ответственность файла:**
- high-level source runtime state object;
- orchestration state, а не packet/media logic.

### `plugins/obs_st2110/src/plugin_entry.cpp` (`NEW`)
**Ответственность файла:**
- OBS plugin entrypoint;
- module registration.

### `plugins/obs_st2110/src/source_registration.cpp` (`NEW`)
**Ответственность файла:**
- OBS source registration.

### `plugins/obs_st2110/src/source_settings_ui.cpp` (`NEW`)
**Ответственность файла:**
- UI/settings binding;
- отсутствие receive pipeline logic.

### `plugins/obs_st2110/src/source_runtime.cpp` (`NEW`)
**Ответственность файла:**
- lifecycle orchestration source runtime;
- start/stop on top of backend contracts.

### `plugins/obs_st2110/src/backend_wiring.cpp` (`NEW`)
**Ответственность файла:**
- translate OBS config -> receive session config + backend selection.

### `plugins/obs_st2110/src/obs_video_handoff.cpp` (`NEW`)
**Ответственность файла:**
- video handoff from project delivery contract to OBS contract.

### `plugins/obs_st2110/src/obs_audio_handoff.cpp` (`NEW`)
**Ответственность файла:**
- audio handoff from project delivery contract to OBS contract.
