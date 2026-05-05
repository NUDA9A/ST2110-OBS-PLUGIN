# ST2110-OBS-PLUGIN — DistroAV OBS plugin context

## Scope

Compact context for the DistroAV plugin/module layer.

This file captures the OBS-facing structure present in the reference plugin:

- how the plugin target is assembled;
- how module entrypoints are structured;
- where feature registration happens;
- how frontend actions and callbacks are attached.

## Derived from

Repository: `NUDA9A/DistroAV`  
Branch: `distro-av-ref-v6.2.1`

## Primary originals

```text
CMakeLists.txt
src/plugin-main.cpp
src/plugin-main.h
```

## Build target shape

`CMakeLists.txt` defines the plugin as a CMake `MODULE` target.

Observed build structure:

- `find_package(libobs REQUIRED)`
- optional `obs-frontend-api`
- optional Qt6 Widgets/Core/Network
- plugin target source aggregation through one `target_sources(...)`
- `AUTOMOC`, `AUTOUIC`, `AUTORCC`
- plugin-style target properties

The build file places plugin, source, UI, config, helper, and output-related files into one module target.

## Module entrypoints

`src/plugin-main.cpp` contains the central OBS module entrypoints:

```text
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(...)
obs_module_name()
obs_module_description()
obs_module_load()
obs_module_post_load()
obs_module_unload()
```

The file is the central location for:

- plugin-global state;
- source/output/filter descriptor registration;
- plugin-global dialog ownership;
- frontend event callback registration.

## Registration layout

`plugin-main.cpp` keeps the registration descriptors as plugin-global objects:

```text
obs_source_info ndi_source_info
obs_output_info ndi_output_info
obs_source_info ndi_filter_info
obs_source_info ndi_audiofilter_info
obs_source_info alpha_filter_info
```

Feature registration is grouped in a dedicated helper:

```text
register_plugin_features()
```

The helper creates descriptor objects via factory-like functions such as:

```text
create_ndi_source_info()
create_ndi_output_info()
create_ndi_filter_info()
create_ndi_audiofilter_info()
create_alpha_filter_info()
```

## Frontend wiring

`obs_module_load()` attaches plugin-global frontend/UI objects:

- adds a Tools menu action;
- creates one persistent `OutputSettings` dialog;
- registers frontend event callbacks;
- initializes/deinitializes plugin-global outputs on frontend lifecycle events.

Observed frontend events used in the reference plugin:

```text
OBS_FRONTEND_EVENT_FINISHED_LOADING
OBS_FRONTEND_EVENT_EXIT
OBS_FRONTEND_EVENT_PROFILE_CHANGING
OBS_FRONTEND_EVENT_PROFILE_CHANGED
```

## Dialog ownership pattern

The settings dialog is owned at plugin/module scope rather than by a source instance.

Observed pattern:

- dialog created once after the frontend main window is available;
- Tools menu action toggles the dialog;
- module-level helpers may bring that dialog to front from error/reporting paths.

## Post-load and unload split

`obs_module_post_load()` is used for deferred work after initial module load.

`obs_module_unload()` performs final cleanup of plugin-global resources.

This leaves `obs_module_load()` focused on:

- config init;
- runtime checks;
- feature registration;
- frontend/menu/dialog wiring.

## `plugin-main.h` role

`src/plugin-main.h` acts as the plugin-global shared header.

It aggregates:

- plugin-global constants;
- version/runtime helper declarations;
- URLs/text/helper declarations;
- global access to the NDI runtime handle used by the reference plugin.

From a structure standpoint, this header is the place where the reference plugin centralizes module-wide constants and declarations needed across multiple implementation files.

## Central file relationships

```text
plugin-main.cpp
 ├─ registers source/output/filter descriptors
 ├─ owns plugin-global dialog pointer
 ├─ hooks frontend events
 ├─ triggers main/preview output init/deinit
 └─ loads and unloads plugin-wide runtime dependencies

plugin-main.h
 ├─ includes config layer
 ├─ includes OBS/Qt support wrappers
 └─ exposes plugin-global constants/helpers
```

## Build-file source membership snapshot

The plugin target includes at least the following source groups:

- forms:
  - `src/forms/output-settings.cpp`
  - `src/forms/update.cpp`
- OBS support:
  - `src/obs-support/obs-app.hpp`
  - `src/obs-support/remote-text.cpp`
- config:
  - `src/config.cpp`
- outputs:
  - `src/main-output.cpp`
  - `src/preview-output.cpp`
  - `src/ndi-output.cpp`
- source/input:
  - `src/ndi-source.cpp`
  - `src/ndi-finder.cpp`
- module:
  - `src/plugin-main.cpp`

## Context boundary

This compact context is about OBS/plugin structure only.

It does not describe ST 2110 behavior, RTP semantics, SDP behavior, timestamp policy, or backend-specific media logic.
