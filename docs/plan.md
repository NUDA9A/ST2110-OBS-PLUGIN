# ST2110-OBS-PLUGIN — Plan

## Правила работы
- 1 задача за раз (маленькая, 15–60 мин).
- Реализацию пишу я сам; ассистент должен:
  - формулировать следующую задачу;
  - подробно описывать, что именно нужно сделать;
  - сразу писать/предлагать тест(ы) под задачу;
  - проверять результат после моей реализации.
- После завершения задачи я присылаю код; ассистент проверяет:
  - соответствие плану;
  - отсутствие ненужных упрощений;
  - отсутствие расхождений со стандартом;
  - расширяемость архитектуры;
  - качество тестового покрытия под задачу.
- Пока задача не принята, к следующей не переходим.
- Сложные детали реализации можно разбирать в отдельном чате, но приемка задачи — только по коду и тестам.

## Правила проектирования
- Код должен быть написан в **расширяемом виде**.
- При добавлении новых форматов / режимов / backend’ов / типов потоков в типичных местах должно быть достаточно:
  - добавить новый enum/value;
  - дописать `case`/adapter/mapper;
  - добавить тесты;
  - а не переписывать существующую реализацию целиком.
- Нельзя жестко зашивать архитектуру только под:
  - один pixel format;
  - только video без возможности добавить audio;
  - только socket backend;
  - только консольный pipeline без дальнейшей OBS-интеграции.
- Ассистент обязан отдельно проверять архитектуру на расширяемость при приемке каждой задачи.
- Все временные упрощения должны быть:
  - явно зафиксированы в плане;
  - локализованы;
  - сопровождаться отдельной задачей на устранение.
- **MVP не должен сознательно накапливать расхождения со стандартом**, если их можно избежать без взрывного роста сложности.
- Если найдено расхождение со стандартом, оно должно быть сразу занесено в:
  - раздел `Spec notes / deviations`;
  - отдельную задачу на исправление или доведение до полного соответствия.

## Цели этапов
- **MVP**: минимально полноценный прием ST 2110 для видео и аудио на Linux, с двумя backend’ами, базовой OBS-интеграцией и готовностью к ручному end-to-end тестированию.
- **Medium**: расширение форматов, улучшение устойчивости, более полная обработка edge-cases, улучшение UX/наблюдаемости, подготовка к полноценному тестированию.
- **Plugin**: доведение OBS-плагина до удобного и стабильного состояния для повседневного использования.
- **Tests**: систематическое полноценное тестирование, фиксация проблемных мест, regression-набор.
- **Hardening**: исправление узких мест, performance, recovery policy, дополнительная корректность.
- **Windows**: опциональный перенос только собственного socket backend, без MTL.

## Референсы (куда смотреть)
- SMPTE ST 2110-20:2022 (video, у меня есть PDF).
- RTP: RFC 3550 (структура заголовка, seq/timestamp/marker).
- Video over RTP: RFC 4175 (концепция packetization по строкам/фрагментам).
- Wireshark dissector ST2110-20 (для сверки полей SRD/ExtSeq/marker).
- Intel MTL (Media Transport Library) docs + st_pipeline_api (для MTL backend).
- Для audio-задач потребуется также ориентироваться на соответствующие документы семейства ST 2110 для аудио и связанные RTP/AES-спецификации (уточним отдельным списком, когда дойдем до них).

---

## Done
- [x] 001: Repo skeleton + buildable stub
- [x] 002: Fix WSL networking/DNS for git push
- [x] 003: CTest smoke test
- [x] 004: Endian helpers (read_be16/read_be32) + tests
- [x] 005: Add common error/result type (enum error codes) + tests
- [x] 006: Add `ByteSpan` alias (std::span<const uint8_t>) and conventions doc snippet
- [x] 007: Define `RxVideoConfig` (width/height/fps, ip/port, payload_type, format)
- [x] 008: Define `VideoFrame`/`VideoFrameView` contract (format, planes, stride, ts_ns)
- [x] 009: Define interfaces:
  - `IVideoFrameSink` (on_video_frame; stats optional later)
  - `IRxBackend` / `IRxVideoBackend`
  - Unit test with FakeBackend -> FakeVideoSink (one frame delivered)
- [x] 010: Define RTP header view struct (version, pt, marker, seq16, ts32, ssrc)
- [x] 011: Implement RTP header parser (validate version=2, min length=12) + tests
- [x] 012: Add helper for seq wrap comparison/distance + tests
- [x] 013: Add "extract payload span" (skip CSRC if present; ignore extensions in MVP) + tests
- [x] 020: Define structs for: ExtendedSeqHi16 + SRD header (len,row,offset,F,C)
- [x] 021: Implement parser for ExtSeqHi16 + 1..3 SRD headers + tests (synthetic bytes)
- [x] 022: Implement validation rules (SRD length > 0, <= MAXUDP, C chaining) + tests
- [x] 023: Implement helper: combine 16-bit RTP seq + ExtSeqHi16 => 32-bit ext seq + tests

---

## Spec notes / deviations
- [x] S001: `validate_st2110_20_payload_header()` currently rejects `SRD Length == 0` unconditionally, but ST 2110-20 allows this special case when there is exactly one SRD header and no sample row data follows. This must be fixed before video RX is considered spec-clean. :contentReference[oaicite:0]{index=0}
- [ ] S002: While MVP may stay progressive-only, code paths and types should not hardcode assumptions that make future interlace / PsF / audio support invasive. ST 2110-20 explicitly distinguishes progressive, interlaced, and PsF behavior for marker/F/row semantics, so these assumptions must remain localized. :contentReference[oaicite:1]{index=1}

---

# Phase 1 — MVP

## Track A — Core library foundations (shared / portable)

### A0. Common base abstractions
- [x] 030: Generalize media-facing naming where needed so types/interfaces can grow from video-only to media-oriented without rewrite
  - review current naming/API surface
  - keep video-specific contracts where appropriate
  - avoid blocking future audio path
- [x] 031: Add common stats structs for parsers / depacketizers / backends
- [x] 032: Add common config validation helpers and conventions doc for "strict parse, explicit fallback"

### A1. Video packet model
- [x] 040: Define `PacketView` (rtp header + ext seq32 + srd list + payload bytes)
- [x] 041: Implement `PacketView::from_udp_datagram()` (parses all layers) + tests
- [x] 042: Add packet stats counters (parse_fail, bad_version, short_packet, bad_srd, etc.)
- [x] 043: Fix zero-length SRD special-case according to ST 2110-20 + tests :contentReference[oaicite:2]{index=2}

### A2. Video reorder / assemble / depacketize
- [x] 050: Define interface for `ReorderBuffer` (push(packet), pop_next())
- [x] 051: Implement fixed window reorder by ext seq32 (size configurable) + tests
- [x] 052: Implement drop/late accounting (out_of_window, missing_seq) + tests
- [x] 053: Add simple timeout/flush policy (optional but localized) + tests
- [x] 054: Extend `VideoFrame` with mutable storage access for UYVY (`width/height/format`, `stride_bytes`, `data`, `row_data`) + tests
- Note:
- No separate `FrameBuffer` type for MVP video assembly.
- `VideoFrame` is the owning assembled-frame storage object.
- `VideoFrameView` remains the non-owning presentation/view type.
- `FrameAssembler` should write directly into `VideoFrame`.
- [x] 055: Define `FrameAssembler` lifecycle over `VideoFrame`: begin(ts_rtp), write_segment(row, byte_off, bytes), end(marker)
- [x] 056: Implement bounds checks (row range, offset+len <= stride) + tests
- [x] 057: Implement frame completeness rule:
  - marker seen => frame can be emitted
  - partial state must be tracked explicitly
- [ ] 058: Implement partial frame policy: drop / emit-with-flag (configurable) + tests
- [ ] 059: Define `Depacketizer` API (push PacketView, returns 0..N completed frames)
- [ ] 060: Implement grouping logic by RTP timestamp (new timestamp => new frame) + tests
- [ ] 061: Implement marker-based end-of-frame + tests
- [ ] 062: Connect PacketView SRD list => FrameAssembler writes + tests
- [ ] 063: Add stats (frames_ok, frames_partial, frames_dropped, packets_used)

### A3. Video timestamp strategy
- [ ] 070: Define internal timestamp type: `uint64_t ts_ns`
- [ ] 071: Decide mapping for MVP:
  - output cadence from fps (constant step) OR
  - local arrival time smoothed
- [ ] 072: Unit test timestamp monotonicity + step consistency

---

## Track B — Audio foundations (MVP scope)

> Конкретные нормы аудио будут уточняться по профильному стандарту, но архитектурно audio нужно заложить уже в MVP.

### B0. Audio common abstractions
- [ ] 080: Define `RxAudioConfig` (sample_rate, channels, packet_time / samples_per_packet, payload_type, ip/port, format)
- [ ] 081: Define `AudioBuffer` / `AudioFrameView` contract
- [ ] 082: Define audio sink/backend-facing interfaces or extend shared interfaces so audio can be supported without ломки video API
- [ ] 083: Add FakeAudioBackend -> FakeAudioSink test

### B1. Audio packet/depacketize MVP
- [ ] 090: Define audio RTP packet model needed by MVP
- [ ] 091: Implement minimal audio RTP parser integration + tests
- [ ] 092: Implement audio reorder/jitter handling MVP + tests
- [ ] 093: Implement audio frame/block assembly MVP + tests
- [ ] 094: Add audio stats (packets_ok, packets_lost, blocks_ok, blocks_partial/dropped)

### B2. Audio timestamp strategy
- [ ] 095: Define audio timestamp mapping to internal `ts_ns`
- [ ] 096: Add monotonicity / cadence tests for audio path

---

## Track C — Linux backends (both required in MVP)

### C0. Socket backend common
- [ ] 100: Refactor backend layer so socket/mtl can expose both video and audio capabilities without duplication explosion
- [ ] 101: Add backend factory / selector design (`socket|mtl`) in extendable form

### C1. Socket video RX
- [ ] 110: Implement `SocketRxVideoBackend` skeleton + smoke test
- [ ] 111: Implement UDP socket open/bind (unicast)
- [ ] 112: Implement multicast join/leave (Linux) if needed
- [ ] 113: Add receive loop (recvfrom/recvmmsg later) and feed PacketView pipeline
- [ ] 114: Add periodic stats print (pps, drops, frames/s)
- [ ] 115: Add graceful stop (SIGINT) and cleanup

### C2. Socket audio RX
- [ ] 120: Implement `SocketRxAudioBackend` skeleton + smoke test
- [ ] 121: Implement UDP receive path for audio
- [ ] 122: Connect audio parser/reorder/assembler pipeline
- [ ] 123: Add periodic stats print (pps, drops, audio blocks/s)
- [ ] 124: Add graceful stop and cleanup reuse

### C3. MTL video RX
- [ ] 130: CMake option `ST2110_WITH_MTL` + build guard
- [ ] 131: Implement `MtlRxVideoBackend` skeleton + smoke test
- [ ] 132: Implement minimal start/stop using MTL ST20P RX (get_frame/put_frame)
- [ ] 133: Map MTL frame -> `VideoFrame`/`VideoFrameView` and deliver to sink
- [ ] 134: Basic stats (frames, drops if available)

### C4. MTL audio RX
- [ ] 140: Investigate minimal viable MTL audio receive path/API
- [ ] 141: Implement `MtlRxAudioBackend` skeleton + smoke test
- [ ] 142: Implement minimal audio start/stop and frame/block delivery
- [ ] 143: Add basic audio stats

---

## Track D — Apps (MVP tools, no OBS yet)

### D0. Unified dump tools
- [ ] 150: Create app skeleton(s): args parsing for media type, width/height/fps or audio params, format, backend + help
- [ ] 151: Add backend selector: `--backend=socket|mtl`
- [ ] 152: Add media selector: `--media=video|audio`
- [ ] 153: Implement synthetic mode for video:
  - feed depacketizer with synthetic packets
  - produce N frames to files + basic stats
- [ ] 154: Implement synthetic mode for audio:
  - feed audio pipeline with synthetic packets
  - produce N output blocks/files + stats
- [ ] 155: Add video frame file writer (UYVY raw) + filename scheme + tests
- [ ] 156: Add audio dump writer + size/sanity tests
- [ ] 157: Add debug output for first N packet headers
- [ ] 158: Add ability to save bad packets for offline repro

---

## Track E — OBS plugin MVP (video + audio, basic UI)

### E0. Environment
- [ ] 170: Create Ubuntu 24.04 VM / environment for OBS plugin work
- [ ] 171: Install OBS and verify it runs
- [ ] 172: Decide code sync method (git clone / shared folder / artifacts)

### E1. Plugin skeleton
- [ ] 180: Add `obs_plugin/` target that builds `.so` and installs to OBS plugins dir
- [ ] 181: Implement source/input skeleton for ST 2110 media
- [ ] 182: Implement minimal UI/properties for:
  - media kind
  - backend selector
  - IP/port/payload type
  - video params
  - audio params
  - start/stop
- [ ] 183: Implement black video / silence audio fallback path with no network

### E2. Connect backends to OBS
- [ ] 190: Wire socket backend to OBS source path
- [ ] 191: Wire MTL backend to OBS source path where available
- [ ] 192: Implement background receive threads and handoff into OBS
- [ ] 193: Implement frame queue / audio queue from backend threads to OBS
- [ ] 194: Implement timestamp mapping for OBS
- [ ] 195: Verify start/stop stability and repeated reconfiguration

---

## Track F — MVP exit / readiness for testing
- [ ] 198: End-to-end video MVP demo on Linux: receive and display/save progressive ST2110-20 GPM stream
- [ ] 199: End-to-end audio MVP demo on Linux: receive and play/save audio stream
- [ ] 200: End-to-end OBS demo with selectable backend and basic UI
- [ ] 201: Document manual test procedure for MVP
- [ ] 202: Document known limitations still allowed at MVP exit

---

# Phase 2 — Medium

## Track G — Video formats / audio formats / extensibility
- [ ] 210: Add at least one more video pixel format beyond UYVY
- [ ] 211: Audit format-specific code paths so new formats require localized additions only
- [ ] 212: Add additional audio format/profile support if needed
- [ ] 213: Add shared format capability description/query API

## Track H — Correctness improvements
- [ ] 220: Improve loss handling policies for video (freeze/black/emit partial/drop)
- [ ] 221: Improve loss handling for audio (gap/conceal/drop policy)
- [ ] 222: Review parser/assembler behavior against spec corner-cases
- [ ] 223: Add stricter validation for payload sizes, pgroups, offsets, timestamps
- [ ] 224: Re-check known deviations list and burn it down

## Track I — Operational quality
- [ ] 230: Better logging and structured stats
- [ ] 231: Runtime config validation / better error messages
- [ ] 232: More ergonomic CLI for tools
- [ ] 233: More informative OBS UI and status readout
- [ ] 234: Better shutdown/restart/reconfigure stability

---

# Phase 3 — Plugin

## Track J — Plugin polish
- [ ] 240: Improve OBS properties UI and defaults
- [ ] 241: Add validation in UI before start
- [ ] 242: Add clearer runtime state/errors in OBS
- [ ] 243: Improve frame/audio queue behavior for live usage
- [ ] 244: Reduce plugin-specific code duplication
- [ ] 245: Verify plugin architecture stays backend-agnostic and media-agnostic

---

# Phase 4 — Tests

## Track K — Systematic testing
- [ ] 250: Expand unit tests for all parser/reorder/assembler paths
- [ ] 251: Add synthetic integration tests for full video pipeline
- [ ] 252: Add synthetic integration tests for full audio pipeline
- [ ] 253: Add backend smoke/integration tests (socket + mtl where possible)
- [ ] 254: Add OBS plugin smoke tests / scripted manual checklist
- [ ] 255: Add regression tests for every fixed bug from `Spec notes / deviations`
- [ ] 256: Add corpus of captured bad packets / sample streams for repro
- [ ] 257: Run focused testing and identify weakest subsystems before hardening

---

# Phase 5 — Hardening

## Track L — Performance / resilience / deeper correctness
- [ ] 260: Replace recvfrom with recvmmsg where useful + benchmark
- [ ] 261: Memory/pool optimizations and frame/buffer reuse
- [ ] 262: Optional: pcap ingest tool for offline tests
- [ ] 263: Better loss recovery and concealment policy
- [ ] 264: Investigate PTP / CLOCK_TAI / PHC only if really needed later
- [ ] 265: Harden thread lifecycle and shutdown ordering
- [ ] 266: Harden long-run stability and leak checks
- [ ] 267: Final audit of spec compliance and removal of temporary limitations

---

# Phase 6 — Windows port (optional, own backend only)

## Track M — Optional Windows support
- [ ] 300: Decide whether Windows port is worth doing after Linux result is stable
- [ ] 301: Introduce OS abstraction layer for sockets if still justified
- [ ] 302: Implement Winsock backend (unicast first)
- [ ] 303: Implement multicast join on Windows
- [ ] 304: Build & run dump tool(s) on Windows
- [ ] 305: Evaluate whether OBS Windows plugin integration is worth the effort
- [ ] 306: Do not port MTL backend; Linux-only by design