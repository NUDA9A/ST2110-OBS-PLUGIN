# ST2110-OBS-PLUGIN — MTL video RX context

## Scope

This document captures the MTL ST20P RX context needed for `MtlRxVideoBackend` tasks.

Relevant plan tasks:

- `131`: `MtlRxVideoBackend` skeleton.
- `132`: minimal MTL video start/stop using ST20P RX frame API.
- `133`: map MTL video frame to current `VideoFrameView`.
- `134`: backend-local MTL video stats.

Derived from MTL reference repository `NUDA9A/Media-Transport-Library` on branch `mtl-ref-v26.01`.

## Primary original references

```text
include/st_pipeline_api.h
include/st20_api.h
lib/src/st2110/pipeline/st20_pipeline_rx.c
app/sample/rx_st20_pipeline_sample.c
```

Reopen these originals if this summary is not enough.

## Minimal MVP API path

Required lifecycle:

```c
mtl_handle mt = mtl_init(&params);

struct st20p_rx_ops ops = {};
st20p_rx_handle rx = st20p_rx_create(mt, &ops);

struct st_frame* frame = st20p_rx_get_frame(rx);
/* consume synchronously */
st20p_rx_put_frame(rx, frame);

st20p_rx_free(rx);
mtl_uninit(mt);
```

Use blocking receive flow:

```c
ops.flags = ST20P_RX_FLAG_BLOCK_GET;
```

On stop, wake the blocked receive call before joining the receive thread:

```c
st20p_rx_wake_block(rx);
```

## `st20p_rx_ops` fields that matter

The ST20P RX ops structure is the projection target from project video config into MTL.

Important fields:

```c
ops.name
ops.priv
ops.port.num_port
ops.port.ip_addr[MTL_SESSION_PORT_P]
ops.port.port[MTL_SESSION_PORT_P]
ops.port.udp_port[MTL_SESSION_PORT_P]
ops.port.payload_type
ops.width
ops.height
ops.fps
ops.interlaced
ops.transport_fmt
ops.output_fmt
ops.device
ops.framebuff_cnt
ops.flags
```

MVP should project only supported project configs. Unsupported values must fail at a localized support/projection boundary.

## Supported MVP projection boundary

Keep MVP support explicit. Do not silently coerce unsupported configs.

Recommended initial support boundary:

- one MTL session port only;
- ST20P RX frame-level pipeline only;
- blocking get enabled;
- progressive video only unless project scan-mode mapping for interlaced is explicitly implemented;
- video format only if it can map to existing `VideoFrameView` / `PixelFormat` without reshaping public frame APIs;
- no ext-frame, GPU direct, auto-detect, DMA offload, header split, or RTCP unless explicitly modeled;
- no public backend API changes.

## Relevant ST20P flags

```c
ST20P_RX_FLAG_DATA_PATH_ONLY
ST20P_RX_FLAG_ENABLE_VSYNC
ST20P_RX_FLAG_EXT_FRAME
ST20P_RX_FLAG_PKT_CONVERT
ST20P_RX_FLAG_ENABLE_RTCP
ST20P_RX_FLAG_SIMULATE_PKT_LOSS
ST20P_RX_FLAG_FORCE_NUMA
ST20P_RX_FLAG_BLOCK_GET
ST20P_RX_FLAG_RECEIVE_INCOMPLETE_FRAME
ST20P_RX_FLAG_DMA_OFFLOAD
ST20P_RX_FLAG_AUTO_DETECT
ST20P_RX_FLAG_HDR_SPLIT
ST20P_RX_FLAG_DISABLE_MIGRATE
ST20P_RX_FLAG_TIMING_PARSER_STAT
ST20P_RX_FLAG_TIMING_PARSER_META
ST20P_RX_FLAG_USE_MULTI_THREADS
ST20P_RX_FLAG_USE_GPU_DIRECT_FRAMEBUFFERS
```

For MVP receive thread, only `ST20P_RX_FLAG_BLOCK_GET` is required.

Stats/timing tasks may later enable:

```c
ST20P_RX_FLAG_TIMING_PARSER_STAT
ST20P_RX_FLAG_TIMING_PARSER_META
```

Only enable these behind a named local policy if the backend actually consumes the resulting fields.

## `struct st_frame` fields relevant to project mapping

Important fields:

```c
void* addr[ST_MAX_PLANES];
size_t linesize[ST_MAX_PLANES];
enum st_frame_fmt fmt;
bool interlaced;
bool second_field;
size_t buffer_size;
size_t data_size;
uint32_t width;
uint32_t height;
enum st10_timestamp_fmt tfmt;
uint64_t timestamp;
uint32_t rtp_timestamp;
enum st_frame_status status;
uint32_t pkts_total;
uint32_t pkts_recv[MTL_SESSION_PORT_MAX];
struct st20_rx_tp_meta* tp[MTL_SESSION_PORT_MAX];
uint64_t receive_timestamp;
```

The project should deliver `VideoFrameView` synchronously before `st20p_rx_put_frame`. Do not store a view that aliases MTL-owned memory after `put_frame`.

## Frame lifetime

`st20p_rx_get_frame` transfers a ready frame to the user.

`st20p_rx_put_frame` returns that frame to MTL.

Backend sink delivery must happen entirely between those two calls.

Invalid pattern:

```c
auto frame = st20p_rx_get_frame(rx);
store_pointer_for_later(frame->addr[0]);
st20p_rx_put_frame(rx, frame);
```

Valid pattern:

```c
auto frame = st20p_rx_get_frame(rx);
VideoFrameView view = map_frame(frame);
sink(view);
st20p_rx_put_frame(rx, frame);
```

## MTL video formats and project support

`enum st_frame_fmt` contains many possible output formats. MVP must not pretend all are supported.

Common ST20P output formats:

```c
ST_FRAME_FMT_YUV422PLANAR10LE
ST_FRAME_FMT_V210
ST_FRAME_FMT_Y210
ST_FRAME_FMT_YUV422PLANAR8
ST_FRAME_FMT_UYVY
ST_FRAME_FMT_YUV422RFC4175PG2BE10
ST_FRAME_FMT_YUV422RFC4175PG2BE12
ST_FRAME_FMT_YUV444PLANAR10LE
ST_FRAME_FMT_YUV444RFC4175PG4BE10
ST_FRAME_FMT_YUV444PLANAR12LE
ST_FRAME_FMT_YUV444RFC4175PG2BE12
ST_FRAME_FMT_YUV420PLANAR8
ST_FRAME_FMT_ARGB
ST_FRAME_FMT_BGRA
ST_FRAME_FMT_RGB8
```

MVP should support only formats that can be projected into current `PixelFormat` and `VideoFrameView`.

Likely first candidates, depending on existing project pixel format boundary:

- packed 8-bit 4:2:2 (`ST_FRAME_FMT_UYVY`) if project has an equivalent;
- packed RGB/BGRA/ARGB if project has compatible pixel format;
- explicitly unsupported for 10-bit/planar formats until project frame contract supports them.

Do not convert silently unless a named conversion boundary is implemented.

## RTP timestamp mapping

MTL frame exposes:

```c
frame->rtp_timestamp
frame->timestamp
frame->tfmt
frame->receive_timestamp
```

Project task `133` requires keeping `rtp_timestamp -> TimestampNs` mapping on the existing timestamp-mapping boundary. Therefore:

- do not map timestamps ad hoc inside sink delivery;
- use or add a localized helper that takes MTL frame timestamp fields and returns project `TimestampNs`;
- if `rtp_timestamp` alone is insufficient for exact epoch time, preserve current project policy and make the limitation explicit.

## Start/stop behavior

Start success sequence:

1. Build and validate MTL device config.
2. `mtl_init`.
3. Project video config → `st20p_rx_ops`.
4. `st20p_rx_create`.
5. Set backend state running.
6. Start receive thread.
7. Receive thread loops on `st20p_rx_get_frame`.

Stop sequence:

1. Mark stop requested.
2. If session exists, call `st20p_rx_wake_block`.
3. Join receive thread.
4. Free session with `st20p_rx_free`.
5. Uninit device with `mtl_uninit`.
6. Clear handles.

Start failure cleanup:

- if thread creation fails after session create, wake/free session and uninit device;
- if session create fails after device init, uninit device;
- if device init fails, no session/thread cleanup is needed.

## Backend-local stats

Useful MTL/video fields:

From `struct st_frame`:

```c
pkts_total
pkts_recv[MTL_SESSION_PORT_P]
pkts_recv[MTL_SESSION_PORT_R]
status
receive_timestamp
rtp_timestamp
```

From ST20 transport/session stats, if available through MTL APIs:

- common RX session stats;
- redundant packet count;
- dropped packet count;
- timing parser stats if enabled;
- queue/session counters.

Project backend-local stats should include:

- frames delivered to sink;
- frames dropped by backend-local unsupported mapping;
- get-frame null/timeouts, if counted;
- MTL frame `status` incompletes if incomplete frame delivery is enabled;
- packet totals/reception counts where available.

Do not force socket backend stats concepts onto MTL:

- no depacketizer reorder stats unless MTL exposes equivalent;
- no packet parse counters unless exposed by MTL;
- no synthetic jitter/VRX stats unless produced by MTL timing parser or project measurement code.

## Original-file fallback triggers

Reopen original MTL video files when a task needs:

- exact `st20p_rx_ops` declaration beyond fields listed here;
- exact `st20_fmt` values;
- exact `st_frame_fmt` to ST20 transport format relationship;
- exact session stats structures;
- timing parser meta definitions;
- implementation details of `st20p_rx_get_frame`, `put_frame`, `wake_block`, or `free`;
- conversion behavior for `ST20P_RX_FLAG_PKT_CONVERT`.
