# Code map shard 11 — OBS plugin / composition

## Ответственность блока

Это верхний слой проекта.

Блок отвечает за:
- OBS plugin/source integration;
- settings/UI;
- backend selection orchestration;
- source runtime lifecycle;
- handoff из project delivery contracts в OBS contracts.

Блок **не должен**:
- знать packet parsing;
- знать depacketizer/reorder internals;
- знать Linux socket syscalls;
- знать детали MTL projection.

## Файлы блока

### `plugins/obs_st2110/include/obs_st2110/plugin_api.hpp` (`NEW`)
**Ответственность файла:**
- public OBS-plugin API surface;
- module-visible declarations.

### `plugins/obs_st2110/include/obs_st2110/source_config.hpp` (`NEW`)
**Ответственность файла:**
- OBS-facing persisted source settings model.

### `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp` (`NEW`)
**Ответственность файла:**
- high-level source runtime state shape;
- owns backend instance and sink/handoff objects at orchestration level.

### `plugins/obs_st2110/src/plugin_entry.cpp` (`NEW`)
**Ответственность файла:**
- plugin entry and module registration.

### `plugins/obs_st2110/src/source_registration.cpp` (`NEW`)
**Ответственность файла:**
- OBS source registration and descriptor wiring.

### `plugins/obs_st2110/src/source_settings_ui.cpp` (`NEW`)
**Ответственность файла:**
- source settings/UI binding.

### `plugins/obs_st2110/src/source_runtime.cpp` (`NEW`)
**Ответственность файла:**
- source lifecycle orchestration;
- backend start/stop on top of backend contracts.

### `plugins/obs_st2110/src/backend_wiring.cpp` (`NEW`)
**Ответственность файла:**
- translate source settings into receive session config and backend selection request.

### `plugins/obs_st2110/src/obs_video_handoff.cpp` (`NEW`)
**Ответственность файла:**
- bridge project video delivery contract -> OBS video frame contract.

### `plugins/obs_st2110/src/obs_audio_handoff.cpp` (`NEW`)
**Ответственность файла:**
- bridge project audio delivery contract -> OBS audio contract.
