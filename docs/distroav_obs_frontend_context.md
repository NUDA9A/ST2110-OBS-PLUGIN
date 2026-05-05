# ST2110-OBS-PLUGIN — DistroAV OBS frontend/config context

## Scope

Compact context for the DistroAV plugin-global config/UI/frontend layer.

This file captures the reference plugin structure around:

- config persistence;
- plugin-global settings dialog;
- frontend-driven output initialization/teardown;
- long-lived `obs_output_t` wrapper objects;
- preview output rendering path;
- custom OBS output implementation.

## Derived from

Repository: `NUDA9A/DistroAV`  
Branch: `distro-av-ref-v6.2.1`

## Primary originals

```text
src/config.cpp
src/config.h
src/forms/output-settings.cpp
src/forms/output-settings.h
src/main-output.cpp
src/main-output.h
src/preview-output.cpp
src/preview-output.h
src/ndi-output.cpp
```

## Config layer

`src/config.h` / `src/config.cpp` implement a plugin-global singleton config object.

Observed public lifecycle:

```text
Config::Initialize()
Config::Current(bool load = true)
Config::Destroy()
```

Observed stored plugin-global fields include:

- main output enable/name/groups
- preview output enable/name/groups
- tally enable flags

Observed additional app/global settings include:

- auto-update behavior
- last update check time
- skipped update version
- minimum auto-check interval

The config layer also contains non-persisted command-line/runtime flags.

## Config responsibilities

Observed responsibilities of the config layer:

- process plugin-specific command-line switches;
- set default values into OBS config stores;
- migrate settings between app/user stores;
- load/save plugin-global settings;
- expose small helper accessors for update-related values.

## Settings dialog

`src/forms/output-settings.*` implements one Qt dialog named `OutputSettings`.

Observed dialog behavior:

- constructs UI once;
- wires accept handling to `onFormAccepted()`;
- fills current widget values in `showEvent(...)`;
- writes updated values back into `Config`;
- calls `Save()`;
- triggers `main_output_init()/deinit()` and `preview_output_init()/deinit()` depending on changed settings.

The dialog also displays runtime/environment status, but the structural value is the populate/apply pattern.

## Dialog sections present in the reference implementation

Observed high-level dialog content includes:

- requirement/status display;
- main output settings;
- preview output settings;
- tally settings;
- update-check actions;
- install/help/community actions.

Only the structural parts are relevant as OBS/plugin context.

## Main output wrapper

`src/main-output.*` wraps one long-lived `obs_output_t*`.

Observed wrapper-level functions:

```text
main_output_init()
main_output_deinit()
main_output_start()
main_output_stop()
main_output_is_supported()
main_output_last_error()
```

Observed context fields include:

- output name/groups
- last error
- current source pointer
- `obs_output_t* output`

Observed behavior:

- create output object from current plugin config;
- attach signal handlers for start/stop;
- cache last error when start fails;
- deinit on frontend/profile lifecycle boundaries.

## Preview output wrapper

`src/preview-output.*` wraps a second long-lived `obs_output_t*` used for preview-scene export.

Observed wrapper responsibilities:

- create output object configured for video-only preview export;
- watch OBS frontend scene/preview events;
- track the current preview or current scene;
- render that source into a texture/stage surface;
- feed a custom `video_output` queue;
- attach that queue to the output object.

Observed context fields include:

- output name/groups
- current source
- `obs_output_t* output`
- custom `video_t*` queue
- dummy `audio_t*`
- `gs_texrender_t*`
- `gs_stagesurf_t*`
- mapped frame data/linesize
- `obs_video_info`

## Frontend event usage in preview output

Observed scene/frontend callbacks handle events such as:

- studio mode enabled
- studio mode disabled
- preview scene changed
- scene changed
- scene collection cleanup

The wrapper swaps `current_source` depending on current frontend state.

## Custom OBS output implementation

`src/ndi-output.cpp` implements the actual custom OBS output type used by `main-output` and `preview-output`.

Observed `obs_output_info` callback layout:

- `get_name`
- `get_properties`
- `get_defaults`
- `create`
- `start`
- `update`
- `stop`
- `destroy`
- `raw_video`
- `raw_audio`

Observed output instance fields include:

- `obs_output_t* output`
- output name/groups
- uses_video / uses_audio flags
- started flag
- output media format information
- conversion buffers
- sender mutex/runtime handle
- connection-count tracking

## Output properties/defaults

The custom output exposes simple text properties for:

- output name
- output groups

Observed defaults include:

- default output name
- default output groups
- uses_video = true
- uses_audio = true

## Output start/stop shape

Observed output lifecycle in `ndi-output.cpp`:

- validate available OBS video/audio media;
- inspect active OBS video/audio format;
- choose runtime output format or reject unsupported formats;
- create sender/runtime instance;
- call `obs_output_begin_data_capture(...)`;
- on stop, call `obs_output_end_data_capture(...)`;
- destroy runtime handles and conversion buffers.

## Raw video/audio callback shape

Observed custom output data callbacks:

- `raw_video(video_data* frame)`
- `raw_audio(audio_data* frame)`

The output implementation converts or repacks media when needed before passing it to its runtime sender.

From an OBS-side perspective, this file is a useful reference for how a custom output type receives media from OBS once started.

## File relationships

```text
config.*
 ├─ persists plugin-global settings
 └─ provides global values consumed by dialog/output wrappers

forms/output-settings.*
 ├─ displays current plugin-global settings
 ├─ writes updated settings back to Config
 └─ triggers output init/deinit based on changed values

main-output.*
 ├─ wraps one long-lived obs_output_t
 └─ uses the custom output type implemented in ndi-output.cpp

preview-output.*
 ├─ wraps one long-lived obs_output_t
 ├─ renders current preview/current scene
 └─ also uses the custom output type from ndi-output.cpp

ndi-output.cpp
 └─ implements the actual custom OBS output descriptor and raw callbacks
```

## Context boundary

This compact context describes plugin-global config/frontend/output structure present in the reference plugin.

It does not define ST 2110 media behavior, ST 2110 output behavior, or any future task-routing rules.
