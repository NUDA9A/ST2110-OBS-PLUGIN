# ST2110-OBS-PLUGIN — DistroAV context index

## Purpose

This document is the entry point for compact DistroAV-derived context related to OBS integration.

The material is limited to OBS/plugin/UI-side structure:

- plugin build wiring;
- module entrypoints and registration;
- source/input lifecycle;
- source properties and UI callbacks;
- plugin-global config and settings dialog patterns;
- frontend-driven initialization and teardown;
- secondary `obs_output_t` patterns present in the reference plugin.

## Derived from

Repository: `NUDA9A/DistroAV`  
Branch: `distro-av-ref-v6.2.1`

## Compact context documents

1. `distroav_obs_plugin_context.md`
   - plugin target structure;
   - `obs_module_*` entrypoints;
   - feature registration;
   - Tools menu integration;
   - frontend event hooks.

2. `distroav_obs_source_context.md`
   - `obs_source_info` structure;
   - source instance/config state split;
   - properties/defaults/update callbacks;
   - create/destroy/show/hide/activate/deactivate flow;
   - background receive-thread pattern;
   - handoff into `obs_source_output_video()` / `obs_source_output_audio()`.

3. `distroav_obs_frontend_context.md`
   - plugin-global config persistence;
   - settings dialog behavior;
   - `main-output` / `preview-output` wrappers;
   - `ndi-output` as a secondary OBS output reference.

## Primary original file set

```text
CMakeLists.txt
src/plugin-main.cpp
src/plugin-main.h
src/ndi-source.cpp
src/ndi-finder.cpp
src/ndi-finder.h
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

## Coverage split

### Plugin/module layer
Covered by:

```text
CMakeLists.txt
src/plugin-main.cpp
src/plugin-main.h
```

Main subjects:

- module target and dependencies;
- module load/post-load/unload structure;
- feature registration;
- plugin-global menu/dialog/frontend wiring.

### Source/input layer
Covered by:

```text
src/ndi-source.cpp
src/ndi-finder.cpp
src/ndi-finder.h
```

Main subjects:

- `obs_source_info`;
- properties/defaults/update;
- source runtime state;
- thread lifetime;
- dynamic source-list refresh.

### Frontend/config/output layer
Covered by:

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

Main subjects:

- plugin-global settings storage;
- settings dialog populate/apply flow;
- frontend callbacks;
- `obs_output_t` wrapper patterns;
- preview rendering/output path.

## File relationships inside the reference plugin

- `plugin-main.cpp` is the central module file.
- `plugin-main.h` is the plugin-wide include/constants hub.
- `ndi-source.cpp` is the main source/input implementation.
- `ndi-finder.*` feeds dynamic property population for the source.
- `config.*` stores plugin-global settings outside per-source properties.
- `forms/output-settings.*` is the plugin-global Qt dialog.
- `main-output.*` and `preview-output.*` wrap long-lived OBS output objects.
- `ndi-output.cpp` implements the custom OBS output type used by those wrappers.
