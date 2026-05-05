# ST2110-OBS-PLUGIN — MTL task context map

## Purpose

This file maps plan tasks `130–143` to compact MTL context documents and original MTL reference files.

Use this to avoid reopening all MTL reference files for every MTL task.

## C3. MTL video RX

### 130 — `ST2110_WITH_MTL` build option and localized MTL guard

Read compact context:

```text
mtl_context_index.md
mtl_runtime_context.md
```

Original MTL fallback files:

```text
include/mtl_api.h
doc/design.md
doc/kernel_socket.md
README.md
app/sample/sample_util.h
```

Key concerns:

- keep public `RxBackendKind::Mtl`;
- build without MTL headers/libs when `ST2110_WITH_MTL=OFF`;
- localize MTL headers to MTL backend/build/factory/runtime implementation;
- keep temporary unavailability in factory/build selection.

### 131 — `MtlRxVideoBackend` skeleton

Read compact context:

```text
mtl_runtime_context.md
mtl_video_rx_context.md
```

Original MTL fallback files:

```text
include/st_pipeline_api.h
include/st20_api.h
app/sample/rx_st20_pipeline_sample.c
lib/src/st2110/pipeline/st20_pipeline_rx.c
```

Key concerns:

- reuse existing backend lifecycle/state/stats/factory contracts;
- explicit `mtl_handle` and `st20p_rx_handle` ownership;
- no parallel backend API;
- explicit support boundary for project video config → MTL ST20P ops.

### 132 — minimal MTL video start/stop

Read compact context:

```text
mtl_runtime_context.md
mtl_video_rx_context.md
```

Original MTL fallback files:

```text
include/mtl_api.h
include/st_pipeline_api.h
app/sample/rx_st20_pipeline_sample.c
lib/src/st2110/pipeline/st20_pipeline_rx.c
```

Key concerns:

- `mtl_init` / `mtl_uninit`;
- `st20p_rx_create` / `st20p_rx_free`;
- blocking `st20p_rx_get_frame` / `st20p_rx_put_frame`;
- `st20p_rx_wake_block` on stop;
- cleanup after partial start failure.

### 133 — map MTL video frame to `VideoFrameView`

Read compact context:

```text
mtl_video_rx_context.md
```

Original MTL fallback files:

```text
include/st_pipeline_api.h
include/st20_api.h
lib/src/st2110/pipeline/st20_pipeline_rx.c
```

Key concerns:

- use `struct st_frame` only during valid lifetime before `st20p_rx_put_frame`;
- map `addr`, `linesize`, `fmt`, `width`, `height`, `interlaced`, `second_field`;
- keep `rtp_timestamp -> TimestampNs` mapping on project timestamp boundary;
- unsupported format/scan/packing combinations must be explicit.

### 134 — backend-local MTL video stats

Read compact context:

```text
mtl_video_rx_context.md
mtl_runtime_context.md
```

Original MTL fallback files:

```text
include/st_pipeline_api.h
include/st20_api.h
lib/src/st2110/pipeline/st20_pipeline_rx.c
include/mtl_api.h
```

Key concerns:

- use available MTL counters and frame metadata;
- do not invent socket/depacketizer/reorder counters;
- make unavailable counters explicit.

## C4. MTL audio RX

### 140 — define minimal viable MTL audio RX path and support boundary

Read compact context:

```text
mtl_runtime_context.md
mtl_audio_rx_context.md
```

Original MTL fallback files:

```text
include/st30_pipeline_api.h
include/st30_api.h
app/sample/rx_st30_pipeline_sample.c
```

Key concerns:

- `mtl_init` / `mtl_uninit`;
- `st30p_rx_create` / `st30p_rx_free`;
- blocking `st30p_rx_get_frame` / `st30p_rx_put_frame`;
- project `RxAudioConfig` / `AudioPcmBitDepth` → `st30p_rx_ops` / `st30_fmt`;
- Level A MVP: `48 kHz`, `1 ms`, `1..8` channels, linear PCM.

### 141 — `MtlRxAudioBackend` skeleton

Read compact context:

```text
mtl_runtime_context.md
mtl_audio_rx_context.md
```

Original MTL fallback files:

```text
include/st30_pipeline_api.h
include/st30_api.h
lib/src/st2110/pipeline/st30_pipeline_rx.c
```

Key concerns:

- reuse existing backend lifecycle/state/stats/factory contracts;
- explicit `mtl_handle` and `st30p_rx_handle` ownership;
- no parallel audio backend API;
- explicit support limits.

### 142 — minimal audio start/stop and frame/block delivery

Read compact context:

```text
mtl_runtime_context.md
mtl_audio_rx_context.md
```

Original MTL fallback files:

```text
include/st30_pipeline_api.h
include/st30_api.h
app/sample/rx_st30_pipeline_sample.c
lib/src/st2110/pipeline/st30_pipeline_rx.c
```

Key concerns:

- blocking `st30p_rx_get_frame` / `st30p_rx_put_frame`;
- `st30p_rx_wake_block` on stop;
- convert supported PCM16/PCM24 into current `InterleavedS32` storage boundary;
- do not leak unsupported wire formats into generic backend API.

### 143 — backend-local MTL audio stats

Read compact context:

```text
mtl_audio_rx_context.md
mtl_runtime_context.md
```

Original MTL fallback files:

```text
include/st30_pipeline_api.h
include/st30_api.h
lib/src/st2110/pipeline/st30_pipeline_rx.c
include/mtl_api.h
```

Key concerns:

- use delivery counters and actual MTL ST30 session stats;
- do not force socket/video packet-parse stats onto audio backend;
- keep unavailable counters explicit.

## Global fallback rule

If compact context files do not answer a concrete implementation/review question, reopen only the relevant original MTL files listed for the current task. Do not reopen the entire configured MTL reference set by default.
