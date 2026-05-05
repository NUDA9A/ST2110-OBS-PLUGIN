# ST2110-OBS-PLUGIN — MTL audio RX context

## Scope

This document captures the MTL ST30P RX context needed for `MtlRxAudioBackend` tasks.

Relevant plan tasks:

- `140`: define minimal viable MTL audio RX path and support boundary.
- `141`: `MtlRxAudioBackend` skeleton.
- `142`: minimal start/stop and frame/block delivery using ST30P RX.
- `143`: backend-local MTL audio stats.

Derived from MTL reference repository `NUDA9A/Media-Transport-Library` on branch `mtl-ref-v26.01`.

## Primary original references

```text
include/st30_pipeline_api.h
include/st30_api.h
lib/src/st2110/pipeline/st30_pipeline_rx.c
app/sample/rx_st30_pipeline_sample.c
```

Reopen these originals if this summary is not enough.

## Minimal MVP API path

Required lifecycle:

```c
mtl_handle mt = mtl_init(&params);

struct st30p_rx_ops ops = {};
st30p_rx_handle rx = st30p_rx_create(mt, &ops);

struct st30_frame* frame = st30p_rx_get_frame(rx);
/* consume synchronously */
st30p_rx_put_frame(rx, frame);

st30p_rx_free(rx);
mtl_uninit(mt);
```

Use blocking receive flow:

```c
ops.flags = ST30P_RX_FLAG_BLOCK_GET;
```

On stop, wake blocking receive before joining receive thread:

```c
st30p_rx_wake_block(rx);
```

## `st30p_rx_ops` projection target

Important fields:

```c
struct st30p_rx_ops {
  struct st_rx_port port;
  enum st30_fmt fmt;
  uint16_t channel;
  enum st30_sampling sampling;
  enum st30_ptime ptime;
  uint16_t framebuff_cnt;
  uint32_t framebuff_size;
  const char* name;
  void* priv;
  uint32_t flags;
  int (*notify_frame_available)(void* priv);
  int socket_id;
};
```

The project should project current `RxAudioConfig` into these fields through a named support/projection helper.

## Required MVP support boundary

Task `140` defines the minimal viable path:

- `mtl_init` / `mtl_uninit`;
- `st30p_rx_create` / `st30p_rx_free`;
- blocking `st30p_rx_get_frame` / `st30p_rx_put_frame`.

Current MVP support should align with ST 2110-30 Level A receiver baseline:

- `48 kHz`;
- `1 ms`;
- `1..8` channels;
- linear PCM only.

Everything else must be a localized `Unsupported` or `InvalidValue` branch:

- unsupported sampling rates;
- unsupported packet times;
- unsupported channel counts;
- unsupported wire formats;
- non-PCM/ST31 AM824 if project does not support it.

## MTL audio enums

Format:

```c
enum st30_fmt {
  ST30_FMT_PCM8 = 0,
  ST30_FMT_PCM16,
  ST30_FMT_PCM24,
  ST31_FMT_AM824,
  ST30_FMT_MAX,
};
```

Sampling:

```c
enum st30_sampling {
  ST30_SAMPLING_48K = 0,
  ST30_SAMPLING_96K,
  ST31_SAMPLING_44K,
  ST30_SAMPLING_MAX,
};
```

Packet time:

```c
enum st30_ptime {
  ST30_PTIME_1MS = 0,
  ST30_PTIME_125US,
  ST30_PTIME_250US,
  ST30_PTIME_333US,
  ST30_PTIME_4MS,
  ST31_PTIME_80US,
  ST31_PTIME_1_09MS,
  ST31_PTIME_0_14MS,
  ST31_PTIME_0_09MS,
  ST30_PTIME_MAX,
};
```

For MVP:

```text
AudioPcmBitDepth::Pcm16 -> ST30_FMT_PCM16
AudioPcmBitDepth::Pcm24 -> ST30_FMT_PCM24
sampling_rate_hz == 48000 -> ST30_SAMPLING_48K
packet_time_us == 1000 -> ST30_PTIME_1MS
channels in [1, 8] -> channel
```

If project supports PCM8, it may map to `ST30_FMT_PCM8`; otherwise keep it unsupported.

Do not map `ST31_FMT_AM824` to linear PCM unless an explicit ST31/AM824 decode boundary exists.

## Helper APIs for derived values

Use MTL helpers rather than hardcoding:

```c
double st30_get_packet_time(enum st30_ptime ptime);
int st30_get_sample_size(enum st30_fmt fmt);
int st30_get_sample_num(enum st30_ptime ptime, enum st30_sampling sampling);
int st30_get_sample_rate(enum st30_sampling sampling);
int st30_get_packet_size(enum st30_fmt fmt, enum st30_ptime ptime,
                         enum st30_sampling sampling, uint16_t channel);
int st30_calculate_framebuff_size(enum st30_fmt fmt, enum st30_ptime ptime,
                                  enum st30_sampling sampling, uint16_t channel,
                                  uint64_t desired_frame_time_ns, double* fps);
```

MVP should derive:

```text
samples_per_packet = sampling_rate_hz * packet_time_us / 1_000_000
```

or use MTL `st30_get_sample_num`.

Do not hardcode `48` samples except as the result of the named Level A derivation for `48 kHz / 1 ms`.

## Frame buffer size

The MTL sample computes `framebuff_size` for a desired frame time:

```c
int framebuff_size = st30_calculate_framebuff_size(
    ops_rx.fmt,
    ops_rx.ptime,
    ops_rx.sampling,
    ops_rx.channel,
    10 * NS_PER_MS,
    NULL);
ops_rx.framebuff_size = framebuff_size;
```

For project receive delivery, choose a named policy:

- packet-sized frames if supported by current audio sink model; or
- fixed short block duration, e.g. 10 ms, if it better matches current `AudioBuffer`/`AudioFrameView`.

The policy must be local to the MTL audio backend/projection layer.

## `struct st30_frame` fields relevant to project mapping

```c
void* addr;
enum st30_fmt fmt;
uint16_t channel;
enum st30_sampling sampling;
enum st30_ptime ptime;
size_t buffer_size;
size_t data_size;
enum st10_timestamp_fmt tfmt;
uint64_t timestamp;
uint64_t epoch;
uint32_t rtp_timestamp;
uint32_t pkts_total;
uint32_t pkts_recv[MTL_SESSION_PORT_MAX];
uint64_t receive_timestamp;
void* priv;
```

Audio delivery must happen before `st30p_rx_put_frame`.

Do not store pointers into `frame->addr` past `put_frame`.

## Mapping to project audio contract

Current task wording requires mapping supported MTL PCM frame data into current `AudioBuffer` / `AudioFrameView`.

Project boundary:

- current storage is `InterleavedS32`;
- MTL wire/input frame may be PCM16 or PCM24;
- conversion to S32 must be localized in the MTL audio backend or a named audio conversion helper;
- generic backend API must not be reshaped for MTL-only formats.

Expected local conversion policy:

```text
PCM16 -> sign-extend/shift to S32 according to project convention
PCM24 -> sign-extend/shift to S32 according to project convention
PCM8  -> unsupported unless project explicitly supports it
AM824 -> unsupported unless explicit ST31 decode exists
```

Need to verify exact endian representation before implementing conversion. MTL `st30_api.h` notes PCM format is interpreted as big endian. If the project stores host-endian S32, conversion must handle byte order explicitly.

## Frame lifetime

Valid pattern:

```c
struct st30_frame* frame = st30p_rx_get_frame(rx);
AudioFrameView view = map_or_convert_frame(frame);
sink(view);
st30p_rx_put_frame(rx, frame);
```

Invalid pattern:

```c
struct st30_frame* frame = st30p_rx_get_frame(rx);
store_audio_pointer_for_later(frame->addr);
st30p_rx_put_frame(rx, frame);
```

If conversion to project-owned `AudioBuffer` is required, the copied/converted buffer may outlive `put_frame`, but the MTL frame pointer must not.

## Start/stop behavior

Start success sequence:

1. Build and validate MTL runtime config.
2. `mtl_init`.
3. Project `RxAudioConfig` → `st30p_rx_ops`.
4. Calculate `framebuff_size` with named policy.
5. `st30p_rx_create`.
6. Set backend running state.
7. Start receive thread.
8. Receive thread loops on `st30p_rx_get_frame`.

Stop sequence:

1. Mark stop requested.
2. If session exists, call `st30p_rx_wake_block`.
3. Join receive thread.
4. Free session with `st30p_rx_free`.
5. Uninit device with `mtl_uninit`.
6. Clear handles.

Start failure cleanup:

- if thread creation fails after session create, wake/free session and uninit device;
- if session create fails after device init, uninit device;
- if device init fails, no session/thread cleanup is needed.

## Backend-local stats

Available ST30P/session-level fields:

From `struct st30_frame`:

```c
rtp_timestamp
receive_timestamp
data_size
pkts_total
pkts_recv[MTL_SESSION_PORT_P]
pkts_recv[MTL_SESSION_PORT_R]
```

From `struct st30_rx_user_stats`:

```c
common
stat_pkts_redundant
stat_pkts_dropped
stat_pkts_len_mismatch_dropped
stat_slot_get_frame_fail
```

ST30 timing parser meta exists at transport level:

```c
struct st30_rx_tp_meta {
  int32_t dpvr_max;
  int32_t dpvr_min;
  float dpvr_avg;
  int32_t ipt_max;
  int32_t ipt_min;
  float ipt_avg;
  int32_t tsdf;
  enum st_rx_tp_compliant compliant;
  char failed_cause[64];
  uint32_t pkts_cnt;
};
```

But the ST30P RX pipeline API does not expose timing-parser callback directly in `st30p_rx_ops`. Do not assume timing-parser meta is available through ST30P unless verified in implementation or enabled through lower-level ST30 API.

Project backend-local stats should include:

- audio frames/blocks delivered;
- bytes/samples delivered;
- unsupported/conversion failures;
- get-frame null/timeouts, if counted;
- MTL session counters that are actually retrieved.

Do not force video/socket counters onto audio MTL backend.

## Original-file fallback triggers

Reopen original MTL audio files when a task needs:

- exact `st30p_rx_ops` declaration beyond fields listed here;
- exact audio enum support in MTL;
- exact frame buffer sizing behavior;
- exact endian/layout expectations for PCM16/PCM24;
- exact `st30p_rx_get_frame` blocking behavior;
- exact session stats structure fields;
- exact lower-level timing parser support.
