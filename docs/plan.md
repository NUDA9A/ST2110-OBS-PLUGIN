# ST2110-OBS-PLUGIN — Plan

## Правила работы
- 1 задача за раз (маленькая, 15–60 мин).
- Реализацию пишу я сам; ассистент может:
  - формулировать задачи;
  - давать направления/доки/термины;
  - писать/предлагать тесты (по запросу).
- Сначала делаем **st2110core** + консольные тулзы + юнит-тесты.
- OBS-интеграция только после того, как `st2110_rx_dump` умеет собирать кадры.
- MVP: **RX ST2110-20 progressive + GPM**, тесты 720p30 (и опционально 1080p60).
- Без PTP / без ST2110-21 / без NMOS в MVP.
- На Linux хотим уметь переключать backend: **Socket** (наш) и **MTL** (опционально).

## Референсы (куда смотреть)
- SMPTE ST 2110-20:2022 (у меня есть PDF).
- RTP: RFC 3550 (структура заголовка, seq/timestamp/marker).
- Video over RTP: RFC 4175 (концепция packetization по строкам/фрагментам).
- Wireshark dissector ST2110-20 (для сверки полей SRD/ExtSeq/marker).
- Intel MTL (Media Transport Library) docs + st_pipeline_api (для MTL backend).

---

## Done
- [x] 001: Repo skeleton + buildable stub
- [x] 002: Fix WSL networking/DNS for git push
- [x] 003: CTest smoke test

---

# Track A — Core library (portable)

## A0. Базовые утилиты/типы
- [x] 004: Endian helpers (read_be16/read_be32) + tests
- [x] 005: Add common error/result type (enum error codes) + tests
- [x] 006: Add `ByteSpan` alias (std::span<const uint8_t>) and conventions doc snippet

## A0.1 Backend abstraction (важно для Socket + MTL)
- [x] 007: Define `RxVideoConfig` (width/height/fps, ip/port, payload_type, format)
- [x] 008: Define `VideoFrame`/`FrameView` contract (format, planes, stride, ts_ns)
- [x] 009: Define interfaces:
  - `IFrameSink` (on_frame, on_stats optional)
  - `IRxVideoBackend` (start/stop, backend_name, get_stats optional)
  - Unit test with FakeBackend -> FakeSink (one frame delivered)

## A1. RTP (минимум для MVP)
- [ ] 010: Define RTP header view struct (version, pt, marker, seq16, ts32, ssrc)
- [ ] 011: Implement RTP header parser (validate version=2, min length=12) + tests
- [ ] 012: Add helper for seq wrap comparison/distance + tests
- [ ] 013: Add "extract payload span" (skip CSRC if present; ignore extensions in MVP) + tests

## A2. ST2110-20 payload header + SRD headers
- [ ] 020: Define structs for: ExtendedSeqHi16 + SRD header (len,row,offset,F,C)
- [ ] 021: Implement parser for ExtSeqHi16 + 1..3 SRD headers + tests (synthetic bytes)
- [ ] 022: Implement validation rules (SRD length > 0, <= MAXUDP, C chaining) + tests
- [ ] 023: Implement helper: combine 16-bit RTP seq + ExtSeqHi16 => 32-bit ext seq + tests

## A3. Packet model / storage primitives
- [ ] 030: Define `PacketView` (rtp header + ext seq32 + srd list + payload bytes)
- [ ] 031: Implement `PacketView::from_udp_datagram()` (parses all layers) + tests
- [ ] 032: Add packet stats counters (parse_fail, bad_version, short_packet, bad_srd, etc.)

## A4. Reorder buffer (jitter window) — MVP version
- [ ] 040: Define interface for ReorderBuffer (push(packet), pop_next())
- [ ] 041: Implement fixed window reorder by ext seq32 (size configurable) + tests
- [ ] 042: Implement drop/late accounting (out_of_window, missing_seq) + tests
- [ ] 043: Add simple timeout/flush policy (optional for MVP) + tests

## A5. Frame format + assembler (UYVY first)
- [ ] 050: Define FrameBuffer for UYVY (width,height,stride,storage) + tests
- [ ] 051: Define FrameAssembler lifecycle: begin(ts_rtp), write_segment(row, byte_off, bytes), end(marker)
- [ ] 052: Implement bounds checks (row range, offset+len <= stride) + tests
- [ ] 053: Implement "frame completeness" MVP rule:
  - marker seen => frame can be emitted (even if partial) + stats
- [ ] 054: Implement "partial frame policy": drop / emit with flag (configurable) + tests

## A6. Depacketizer (ST2110-20 -> assembled frames)
- [ ] 060: Define Depacketizer API (push PacketView, returns 0..N completed frames)
- [ ] 061: Implement grouping logic by RTP timestamp (new timestamp => new frame) + tests
- [ ] 062: Implement marker-based end-of-frame + tests
- [ ] 063: Connect PacketView SRD list => FrameAssembler writes + tests
- [ ] 064: Add stats (frames_ok, frames_partial, frames_dropped, packets_used)

## A7. Timestamp strategy (MVP, без PTP)
- [ ] 070: Define internal timestamp type: `uint64_t ts_ns`
- [ ] 071: Decide mapping for MVP:
  - output cadence from fps (constant step) OR
  - local arrival time smoothed
- [ ] 072: Unit test timestamp monotonicity + step consistency

---

# Track B — Apps (без OBS)

## B0. st2110_rx_dump (backend-switchable, synthetic first)
- [ ] 100: Create app skeleton: args parsing (width,height,fps,format,backend) + help
- [ ] 101: Add backend selector: `--backend=socket|mtl` (mtl optional build)
- [ ] 102: Implement `--synthetic` mode (через Socket pipeline, без сети):
  - feed depacketizer with synthetic packets
  - produce N frames to files + basic stats
- [ ] 103: Add frame file writer (UYVY raw) + filename scheme + tests (size check)

## B1. Socket backend: UDP receive on Linux (WSL)
- [ ] 110: Implement `SocketRxBackend` (implements IRxVideoBackend) skeleton + smoke test
- [ ] 111: Implement UDP socket open/bind (unicast)
- [ ] 112: Implement multicast join/leave (Linux) (optional if test source multicast)
- [ ] 113: Add receive loop (recvfrom/recvmmsg) and feed PacketView pipeline
- [ ] 114: Add periodic stats print (pps, drops, frames/s)
- [ ] 115: Add graceful stop (SIGINT) and cleanup

## B1.1 MTL backend (Linux-only, optional)
- [ ] 130: CMake option `ST2110_WITH_MTL` + build guard
- [ ] 131: Implement `MtlRxBackend` skeleton (implements IRxVideoBackend) + smoke test
- [ ] 132: Implement minimal start/stop using MTL ST20P RX (get_frame/put_frame)
- [ ] 133: Map MTL frame -> VideoFrame/FrameView and deliver to sink
- [ ] 134: Basic stats (frames, drops if available)

## B2. Debug helpers
- [ ] 120: Add "dump first N packet headers" (seq, ts, marker, srd summary)
- [ ] 121: Add "save bad packets" to file for offline repro (optional)
- [ ] 122: docs: Wireshark filters/fields notes for comparing SRD/ext seq

---

# Track C — OBS integration (позже, VirtualBox Ubuntu)

## C0. VirtualBox environment
- [ ] 200: Create Ubuntu 24.04 VM (GUI, 3D accel)
- [ ] 201: Install OBS (PPA or Flatpak), verify it runs
- [ ] 202: Decide code sync method:
  - git clone inside VM OR
  - shared folder OR
  - build artifacts copy

## C1. OBS plugin skeleton (no network)
- [ ] 210: Add `obs_plugin/` target that builds `.so` and installs to OBS plugins dir
- [ ] 211: Implement Source "ST2110 Input" that outputs black 720p30 (no backend)
- [ ] 212: Add properties UI: start/stop + config fields (not wired yet)

## C2. Wire backends to OBS source
- [ ] 220: Add backend selector in UI (socket/mtl if available)
- [ ] 221: Run backend in background thread, push frames into OBS
- [ ] 222: Implement frame queue from backend thread to OBS output
- [ ] 223: Timestamp mapping for OBS (MVP monotonic ns)
- [ ] 224: Start/Stop stability tests (start/stop 100 times, no crash/leak)

---

# Track D — Hardening (после MVP)

## D0. Formats & correctness
- [ ] 300: Add support for another pixel format (e.g., 4:2:2 10-bit) if needed
- [ ] 301: Add BPM parsing support (optional)
- [ ] 302: Better loss handling (freeze/black/conceal policy)

## D1. Performance (если потребуется)
- [ ] 310: Replace recvfrom with recvmmsg where possible + benchmark
- [ ] 311: Memory/pool optimizations (frame buffers reuse)
- [ ] 312: Optional: pcap ingest tool for offline tests

## D2. PTP (optional, later)
- [ ] 320: Investigate linuxptp / CLOCK_TAI / PHC access plan
- [ ] 321: Implement clock discipline layer (only if multi-source sync needed)

---

# Track E — Windows port (после Linux MVP)

- [ ] 400: Introduce OS abstraction layer for sockets
- [ ] 401: Implement Winsock backend (unicast first)
- [ ] 402: Implement multicast join on Windows
- [ ] 403: Build & run rx_dump on Windows
- [ ] 410: OBS Windows plugin integration (after core works)