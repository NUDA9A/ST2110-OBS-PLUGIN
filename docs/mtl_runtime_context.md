# ST2110-OBS-PLUGIN — MTL runtime context

## Scope

This document captures MTL runtime/device context needed for `ST2110_WITH_MTL`, MTL backend construction, and common device lifecycle for video/audio RX backends.

Derived from MTL reference repository `NUDA9A/Media-Transport-Library` on branch `mtl-ref-v26.01`.

## Primary original references

```text
include/mtl_api.h
doc/design.md
doc/kernel_socket.md
README.md
app/sample/sample_util.h
```

Reopen these originals if this summary is not enough.

## Device lifecycle

Minimal device lifecycle for the project MTL RX backends:

```c
struct mtl_init_params params = {};
mtl_handle mt = mtl_init(&params);

/* create one or more ST20P/ST30P RX sessions against mt */

mtl_uninit(mt);
```

The MTL samples enable:

```c
ctx.param.flags |= MTL_FLAG_DEV_AUTO_START_STOP;
ctx.st = mtl_init(&ctx.param);
...
mtl_uninit(ctx.st);
```

For this project, keep MTL device ownership explicit inside the backend/runtime layer. Do not leak MTL headers, MTL handles, MTL PMD names, or MTL session handles into app/bootstrap/public backend-selection code.

## `mtl_handle`

MTL exposes the device context as:

```c
typedef struct mtl_main_impl* mtl_handle;
```

Treat this as an opaque owned resource.

Backend-local ownership model:

- MTL backend owns `mtl_handle`.
- Session handle owns ST20P/ST30P RX session state.
- Stop/shutdown must release session before `mtl_uninit`.
- Start failure must clean up every successfully created MTL resource.
- Retry-after-failure must start from a clean backend state.

## `mtl_init_params` fields relevant to this project

Relevant fields:

```c
char port[MTL_PORT_MAX][MTL_PORT_MAX_LEN];
uint8_t num_ports;
enum mtl_net_proto net_proto[MTL_PORT_MAX];
enum mtl_pmd_type pmd[MTL_PORT_MAX];
uint16_t tx_queues_cnt[MTL_PORT_MAX];
uint16_t rx_queues_cnt[MTL_PORT_MAX];
uint8_t sip_addr[MTL_PORT_MAX][MTL_IP_ADDR_LEN];
uint64_t flags;
enum mtl_log_level log_level;
char* lcores;
uint16_t dump_period_s;
void (*stat_dump_cb_fn)(void* priv);
uint16_t pkt_udp_suggest_max_size;
```

For RX-only MVP, the most important values are:

- `port[0]`
- `num_ports`
- `pmd[0]`
- `net_proto[0]`
- `sip_addr[0]` where required by selected PMD/mode
- `rx_queues_cnt[0]`
- `flags`

Do not hardcode MTL device defaults deep in video/audio backend logic. Keep defaults in a named MTL runtime config/projection helper.

## PMD selection

MTL PMD enum values relevant to local development/runtime projection:

```c
MTL_PMD_DPDK_USER = 0
MTL_PMD_NATIVE_AF_XDP = 4
MTL_PMD_KERNEL_SOCKET = 17
MTL_PMD_DPDK_AF_XDP = 19
MTL_PMD_DPDK_AF_PACKET = 20
```

Important boundary:

- `MTL_PMD_DPDK_USER` is the default/production-style DPDK PMD.
- `MTL_PMD_KERNEL_SOCKET` is marked experimental in MTL.
- Kernel socket naming uses `kernel:<ifname>`.
- Native AF_XDP naming uses `native_af_xdp:<ifname>`.

Project support boundary should explicitly model which PMDs are accepted for MVP, rather than hiding a literal string in backend code.

## Ports and session ports

MTL has device ports:

```c
MTL_PORT_P
MTL_PORT_R
...
```

MTL has session ports:

```c
MTL_SESSION_PORT_P
MTL_SESSION_PORT_R
MTL_SESSION_PORT_MAX
```

For current MVP, one-port RX should be the default support boundary. Two-port/redundant handling should remain structurally possible but may be `Unsupported` if not implemented.

## Useful flags

Common runtime flag:

```c
MTL_FLAG_DEV_AUTO_START_STOP
```

Meaning: do `mtl_start` in `mtl_init`, `mtl_stop` in `mtl_uninit`, and skip explicit `mtl_start`/`mtl_stop`.

Useful RX/runtime flags to recognize:

```c
MTL_FLAG_RX_SEPARATE_VIDEO_LCORE
MTL_FLAG_RX_VIDEO_MIGRATE
MTL_FLAG_TASKLET_THREAD
MTL_FLAG_TASKLET_SLEEP
MTL_FLAG_NO_MULTICAST
MTL_FLAG_RX_UDP_PORT_ONLY
MTL_FLAG_RX_USE_CNI
MTL_FLAG_ENABLE_HW_TIMESTAMP
```

MVP should avoid exposing these flags as ad hoc literals in feature code. If needed, introduce a localized runtime config policy.

## Stats available at device level

MTL port status:

```c
struct mtl_port_status {
  uint64_t rx_packets;
  uint64_t tx_packets;
  uint64_t rx_bytes;
  uint64_t tx_bytes;
  uint64_t rx_err_packets;
  uint64_t rx_hw_dropped_packets;
  uint64_t rx_nombuf_packets;
  uint64_t tx_err_packets;
};
```

Useful APIs:

```c
int mtl_get_port_stats(mtl_handle mt, enum mtl_port port, struct mtl_port_status* stats);
int mtl_get_fix_info(mtl_handle mt, struct mtl_fix_info* info);
int mtl_get_var_info(mtl_handle mt, struct mtl_var_info* info);
```

Do not invent packet-parse/reorder/depacketizer stats for the MTL backend. Use available MTL session/device counters and explicit unavailable values.

## Build/wiring implications for `ST2110_WITH_MTL`

Expected project boundary:

- `RxBackendKind::Mtl` remains in public backend-kind axis regardless of build flag.
- `ST2110_WITH_MTL=OFF` must build without MTL headers/libs and without compiling MTL backend code.
- `ST2110_WITH_MTL=ON` may include MTL headers only inside MTL-specific implementation/build/factory/runtime files.
- App/bootstrap code should not include MTL headers.
- Factory/build selection should localize temporary unavailability.
- Parser/selection should still recognize `mtl`.

## Error-handling policy

MTL APIs use NULL handles and negative return values for failures.

Backend policy:

- If `mtl_init` returns NULL, start fails and no session create is attempted.
- If session create returns NULL, free prior resources and uninit the device.
- If receive thread startup fails after session create, wake/free session and uninit device.
- Stop must wake blocking get before joining receive thread.
- Stop/shutdown must tolerate partial start state.
- Start-after-failure must not reuse stale handles.

## Original-file fallback triggers

Reopen original MTL files when a task needs:

- exact `mtl_init_params` field semantics not listed here;
- exact PMD naming or queue requirements;
- `mtl_start` / `mtl_stop` semantics;
- DPDK/AF_XDP/kernel socket details;
- device-level stats behavior;
- non-MVP PMD/runtime flags.
