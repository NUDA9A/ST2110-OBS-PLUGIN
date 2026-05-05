# ST2110-OBS-PLUGIN — MTL context index

## Purpose

This file is the entry point for MTL-related task context.

It summarizes which compact MTL context documents should be read before working on `ST2110_WITH_MTL`, `MtlRxVideoBackend`, `MtlRxAudioBackend`, MTL lifecycle, MTL frame mapping, or MTL backend-local stats.

These notes are derived from `NUDA9A/Media-Transport-Library` on pinned branch `mtl-ref-v26.01`.

## Context documents

Read in this order for MTL tasks:

1. `mtl_runtime_context.md`
   - MTL device lifecycle.
   - `mtl_init` / `mtl_uninit`.
   - `mtl_init_params`.
   - port and PMD selection.
   - build/runtime localization expectations.

2. `mtl_video_rx_context.md`
   - ST20P RX API surface.
   - `st20p_rx_create` / `st20p_rx_free`.
   - blocking `st20p_rx_get_frame` / `st20p_rx_put_frame`.
   - `struct st_frame` fields relevant to project `VideoFrameView`.
   - support boundaries for video format mapping and stats.

3. `mtl_audio_rx_context.md`
   - ST30P RX API surface.
   - `st30p_rx_create` / `st30p_rx_free`.
   - blocking `st30p_rx_get_frame` / `st30p_rx_put_frame`.
   - `struct st30_frame` fields relevant to project `AudioFrameView`.
   - support boundaries for `RxAudioConfig` / `AudioPcmBitDepth` projection and stats.

4. `mtl_task_context_map.md`
   - Task-to-context mapping for plan tasks `130–143`.
   - Which original MTL reference files to reopen if the compact notes are insufficient.

## Original MTL reference paths

The compact notes do not remove the authority of the pinned MTL reference repository. If these notes are insufficient for a specific implementation, review, or verification step, reopen the relevant original files from:

Repository: `NUDA9A/Media-Transport-Library`  
Branch: `mtl-ref-v26.01`

Core original files:

```text
README.md
doc/design.md
doc/kernel_socket.md
doc/external_frame.md
include/mtl_api.h
include/st20_api.h
include/st30_api.h
include/st_pipeline_api.h
include/st30_pipeline_api.h
app/sample/sample_util.h
app/sample/rx_st20_pipeline_sample.c
app/sample/rx_st30_pipeline_sample.c
lib/src/st2110/pipeline/st20_pipeline_rx.c
lib/src/st2110/pipeline/st30_pipeline_rx.c
```

## Use rule

For MTL tasks, prefer these compact context documents first. If a concrete API field, enum value, return behavior, lifecycle edge case, or stat field is not covered here, reopen the original MTL reference file on `mtl-ref-v26.01` rather than guessing.
