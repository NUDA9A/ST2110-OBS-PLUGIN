# ST2110-OBS-PLUGIN — DistroAV OBS source context

## Scope

Compact context for the DistroAV source/input layer.

This file captures the OBS-facing source structure present in the reference plugin:

- `obs_source_info` composition;
- source instance/runtime/config fields;
- source properties/defaults/update callbacks;
- create/destroy/show/hide/activate/deactivate flow;
- receive-thread lifetime;
- video/audio handoff into OBS;
- dynamic source-list refresh pattern.

## Derived from

Repository: `NUDA9A/DistroAV`  
Branch: `distro-av-ref-v6.2.1`

## Primary originals

```text
src/ndi-source.cpp
src/ndi-finder.cpp
src/ndi-finder.h
```

## Source descriptor structure

`src/ndi-source.cpp` builds one `obs_source_info` with the following callback layout:

```text
get_name
get_properties
get_defaults
create
activate
show
update
hide
deactivate
destroy
get_width
get_height
```

Observed descriptor properties:

- `id = "ndi_source"`
- `type = OBS_SOURCE_TYPE_INPUT`
- camera-style icon type
- output flags for async video + audio + no-duplicate behavior

## Source state split

The implementation uses:

1. a top-level source instance structure;
2. a nested source-config structure.

Observed source instance fields include:

- `obs_source_t* obs_source`
- nested config
- `bool running`
- `pthread_t av_thread`
- current width/height
- last-frame timestamp

Observed config split:

### Reset-sensitive fields
Fields that trigger receiver recreation/reset in the reference source:

- receiver name
- selected source name
- bandwidth mode
- latency mode
- frame-sync toggle
- hardware-acceleration toggle

### Live-updatable fields
Fields updated without full receiver recreation:

- behavior mode
- timeout action
- sync mode
- YUV range
- YUV colorspace
- audio enable
- PTZ values
- tally state

## Property model

`get_properties()` builds the full source property set in one function.

Observed property categories:

- editable source selector list
- behavior list
- timeout list
- bandwidth list
- sync list
- bool toggles
- YUV range/colorspace lists
- latency list
- audio toggle
- PTZ property group with sliders

The implementation uses stable property IDs such as:

```text
PROP_SOURCE
PROP_BEHAVIOR
PROP_TIMEOUT
PROP_BANDWIDTH
PROP_SYNC
PROP_FRAMESYNC
PROP_HW_ACCEL
PROP_FIX_ALPHA
PROP_YUV_RANGE
PROP_YUV_COLORSPACE
PROP_LATENCY
PROP_AUDIO
PROP_PTZ
PROP_PAN
PROP_TILT
PROP_ZOOM
```

## Property dependency callback

The source properties include a modified callback attached to the bandwidth property.

Observed behavior:

- checks whether the current bandwidth mode is audio-only;
- toggles visibility of YUV range/colorspace properties accordingly.

This is the key intra-properties dependency example in the reference source.

## Defaults

`get_defaults()` assigns explicit defaults through `obs_data_set_default_*`.

Observed defaults include:

- bandwidth mode
- behavior mode
- timeout mode
- sync mode
- YUV range
- YUV colorspace
- latency mode
- audio enabled

## Update flow

`update()` is the central reconfiguration function.

Observed structure:

1. read each setting explicitly from `obs_data_t`;
2. compare against current source config;
3. accumulate a `reset_ndi_receiver` decision;
4. free/replace owned string fields;
5. normalize invalid/legacy behavior values;
6. update source-local runtime flags;
7. clear current OBS texture when required by behavior/reset mode;
8. toggle OBS async unbuffered mode for lowest latency;
9. toggle OBS audio active state;
10. update PTZ/tally state;
11. decide whether to stop, start, or just notify the running thread about reset.

## Source lifecycle callbacks

The source uses all of these callbacks:

- `create`
- `destroy`
- `show`
- `hide`
- `activate`
- `deactivate`
- `update`

Observed runtime behavior shape:

- `create` allocates source state, derives a receiver name, connects rename signal, calls `update`;
- `destroy` disconnects signals, stops thread, frees owned strings/state;
- `show` / `activate` may start the thread if not running;
- `hide` may stop the thread depending on current visibility behavior;
- `deactivate` updates state but does not necessarily destroy runtime.

## Rename signal handling

The source connects to the OBS source signal handler for rename events.

Observed flow:

- `create` connects `"rename"` signal;
- callback regenerates receiver name;
- callback marks the runtime for reset.

This is the main example of source-name-driven runtime reconfiguration in the reference implementation.

## Receive-thread pattern

The source owns a dedicated background thread.

Observed thread helpers:

```text
ndi_source_thread()
ndi_source_thread_start()
ndi_source_thread_stop()
```

Observed thread shape:

- source-local running flag;
- long-running receive loop;
- explicit reset branch inside the loop;
- stop path joins the thread;
- start path sets running, marks reset, creates thread.

## No-signal handling

The source tracks the last delivered frame time and has a timeout-based no-signal path.

Observed helper flow:

- `process_empty_frame(...)`
- `deactivate_source_output_video_texture(...)`

The reference implementation may clear the OBS texture by calling:

```text
obs_source_output_video(source, NULL)
```

after a timeout if configured to clear content.

## OBS handoff path

Video and audio are handed to OBS from the source thread.

### Audio
The source fills `obs_source_audio` and calls:

```text
obs_source_output_audio(...)
```

Mapped fields include:

- speakers/layout
- timestamp
- sample rate
- audio format
- frame count
- planar channel pointers

### Video
The source fills `obs_source_frame` and calls:

```text
obs_source_output_video(...)
```

Mapped fields include:

- format
- timestamp
- width/height
- line size
- data pointer

## Width/height exposure

The source updates current dimensions during video-frame processing and exposes them through:

```text
ndi_source_get_width()
ndi_source_get_height()
```

These are wired into the `obs_source_info` descriptor.

## Pixel-format mapping present in the reference source

Observed video FourCC → OBS format mapping includes:

- BGRA
- BGRX
- RGBA / RGBX
- UYVY / UYVA
- I420
- NV12

Unsupported formats log an explicit error path.

## Audio channel-layout mapping present in the reference source

The source has a helper mapping raw channel count to OBS speaker layout.

Observed mappings include:

- 1 → mono
- 2 → stereo
- 3 → 2.1
- 4 → quad / 4.0 depending on OBS API
- 5 → 4.1
- 6 → 5.1
- 8 → 7.1

## Dynamic source-list refresh

`NDIFinder` provides a cached asynchronously refreshed source-name list.

Observed mechanism:

- static cached list
- static mutex
- static last refresh time
- static refresh-in-progress flag
- refresh on a detached thread
- callback receives the refreshed list and triggers `obs_source_update_properties(...)`

Refresh cadence in the reference implementation is time-based and uses a short cache interval.

## Finder file relationships

```text
ndi-source.cpp
 ├─ creates list property
 ├─ asks NDIFinder for cached list
 └─ registers callback to repopulate property list and refresh properties UI

ndi-finder.cpp
 ├─ owns static cache
 ├─ performs refresh on detached worker thread
 └─ rebuilds source-name list from the runtime library
```

## Context boundary

This compact context describes the OBS/source shape present in the reference plugin.

It does not define ST 2110 property schema, ST 2110 timestamps, ST 2110 fallback policy, ST 2110 discovery, or backend-specific media semantics.
