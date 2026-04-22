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
- При формулировании задачи и при приемке ассистент обязан сверять не только текущую функциональность, но и соответствие целевой архитектуре из плана:
  - `PixelFormat` и `VideoScanMode` рассматриваются как независимые оси модели;
  - progressive-only поведение в MVP допустимо только как локализованное ограничение;
  - новые изменения не должны закреплять assumptions вида `marker == end of frame`, `timestamp group == frame`, `F is always zero` вне специально выделенных policy / mode-aware точек.
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
- Нельзя жестко зашивать video pipeline только под progressive semantics.
- Архитектурно должны быть разделены:
  - pixel/storage format;
  - scan mode (`Progressive | Interlaced | PsF`);
  - mode-dependent completion semantics (marker/timestamp/end-of-unit logic).
- Даже если MVP реализует только `Progressive`, код и API должны позволять добавить `Interlaced` и `PsF` через локальное расширение policy/state/config, а не через переписывание depacketizer/assembly pipeline.
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
- [ ] S002: While MVP behavior may stay progressive-only, internal configs, state machines, packet-to-unit grouping, completion logic, and segment placement must model scan mode separately from pixel format and must not hardcode assumptions such as “timestamp group == frame”, “marker == end of frame”, or “F is always zero” in ways that make future interlace / PsF support invasive. ST 2110-20 explicitly distinguishes progressive, interlaced, and PsF behavior for `F`, marker, row numbering, grouping, placement, and signaling, so these assumptions must remain localized in dedicated mode-aware / format-aware helpers.
- [x] S003: `Depacketizer::map_segment_to_frame_write()` currently treats `SRD Offset` as a byte offset, but ST 2110-20 defines it as the horizontal position of the first full-bandwidth sample in the image pixel matrix. For UYVY / 4:2:2 this must be mapped through format-aware logic instead of written directly as bytes. This must be fixed before video RX is considered spec-clean. :contentReference[oaicite:3]{index=3}
- [x] S004: Current UYVY receive path does not validate pgroup alignment constraints implied by ST 2110-20 4:2:2 sampling. For 8-bit 4:2:2, packetized data must respect the pgroup structure and `SRD Length` must remain a multiple of pgroup octet size; offset semantics must also remain aligned with full-bandwidth sample positions. Validation must be added in a localized, format-aware way. :contentReference[oaicite:4]{index=4}
- [x] S005: Current payload validation does not enforce monotonic ordering rules for `SRD Row Number` / `SRD Offset` within a packet. ST 2110-20 requires sample rows to progress top-to-bottom and offsets within a row to progress left-to-right. This must be validated explicitly. :contentReference[oaicite:5]{index=5}
- [x] S006: Task 022 covered only part of payload-header validation. Size/limit checks that depend on packet/payload sizing policy (including the path toward MAXUDP-aware validation) still need an explicit follow-up task so completed work and remaining work are not conflated. :contentReference[oaicite:6]{index=6}
- [x] S007: Public headers currently contain non-trivial function definitions in a way that risks ODR / multiple-definition problems once the project grows beyond the current “mostly one translation unit per test executable” shape. The linkage model must be made explicit (true header-only with `inline`, or moved implementations) before backend/app growth.
- [x] S008: `PacketParseStats` structures exist, but packet parsing does not yet expose a single integrated path that records stage-specific parse results through the real parse flow. This should be fixed so parse observability is not only nominal.

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
- [x] 033: Make current public-header implementation ODR-safe
- audit all non-template/non-class function definitions placed in headers
- either mark true header-only functions `inline` or move implementations into `.cpp`
- keep the decision consistent across the library
- add a multi-translation-unit link test so this does not regress
- [x] 034: Fix repo build/test script correctness
  - fix `scripts/build_and_test.sh` strict-mode typo
  - verify the script actually configures, builds, and runs tests from a clean checkout
  - add a minimal CI-oriented smoke check or documented manual verification step

### A1. Video packet model
- [x] 040: Define `PacketView` (rtp header + ext seq32 + srd list + payload bytes)
- [x] 041: Implement `PacketView::from_udp_datagram()` (parses all layers) + tests
- [x] 042: Add packet stats counters (parse_fail, bad_version, short_packet, bad_srd, etc.)
- [x] 043: Fix zero-length SRD special-case according to ST 2110-20 + tests :contentReference[oaicite:2]{index=2}
- [x] 044: Add localized format-aware segment constraints helper(s)
  - define helper/API boundary where packet/segment validation can depend on active video format
  - keep generic ST 2110-20 parsing separate from format-specific receive constraints
  - ensure adding a new format later only requires a new `case`/helper/tests
- [x] 045: Validate SRD ordering rules within one RTP packet
  - `SRD Row Number` must not go backwards
  - within the same row, `SRD Offset` must not go backwards
  - keep progressive-only assumptions localized so interlace/PsF support can be added later
  - add focused tests for valid and invalid 2-SRD / 3-SRD packets
- [x] 046: Add explicit follow-up validation for size-limit policy
  - separate pure wire-format parsing from size-limit/config-policy checks
  - define where MAXUDP-related constraints will live for MVP
  - add tests covering oversized payload / inconsistent header+payload sizing behavior
- [x] 047: Add integrated packet-parse stats recording path
  - provide one real parse entry point that records `PacketParseStage` failures/successes
  - make sure the counters reflect the actual parse pipeline instead of only helper-level unit tests
  - add tests for per-stage accounting

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
- [x] 058: Implement partial frame policy: drop / emit-with-flag (configurable) + tests
- [x] 059: Define `Depacketizer` API (push PacketView, returns 0..N completed video units)
- [x] 060: Implement current MVP grouping logic for video units (progressive path) + tests
- [x] 061: Implement current MVP completion behavior for progressive video units + tests
- [x] 062: Connect PacketView SRD list => FrameAssembler writes + tests
- [x] 063: Add depacketizer stats (`units_ok`, `units_partial`, `units_dropped`, `packets_used`)
- [x] 064: Define `VideoScanMode` as a transport / assembly property independent from `PixelFormat`
  - add enum for `Progressive | Interlaced | PsF`
  - thread it through `RxVideoConfig`, `DepacketizerConfig`, and other relevant internal video config/state types
  - keep current MVP behavior implemented only for `Progressive`
  - add tests proving scan mode is modeled separately from pixel format
- [x] 065: Generalize video receive completion semantics so marker/timestamp handling is scan-mode-aware by architecture
  - remove hardcoded internal assumption that `marker => end of frame`
  - remove hardcoded internal assumption that one RTP timestamp group always corresponds to a complete frame
  - introduce a localized mode-dependent policy point for end-of-unit / completion decisions
  - implement only the `Progressive` policy in MVP; non-progressive branches may stay localized as `Unsupported` / not-yet-implemented
  - add tests proving current progressive behavior is unchanged
- [x] 066: Refactor depacketizer / assembly contracts so future interlace / PsF support can be added by filling pre-defined extension points, not by rewriting the pipeline
  - make depacketizer output/unit model generic (`AssembledVideoUnit`, unit-oriented stats, unit-oriented public API)
  - avoid baking “frame-only” semantics into depacketizer public contract, state, and counters
  - keep `FrameAssembler` byte-oriented and format-agnostic
  - keep current public progressive behavior intact for MVP
  - document/localize the current non-progressive runtime boundary
  - add focused tests for architecture-level behavior and localized rejection of non-progressive modes
- [x] 066A: Introduce `VideoAssemblyKey` and move packet-to-unit grouping decisions out of `Depacketizer::push()`
  - define a mode-aware `VideoAssemblyKey` type for "which assembly unit this packet belongs to"
  - add helper(s) that derive assembly key from `PacketView` + `VideoScanMode`
  - add helper(s) for "same unit / starts new unit" comparison
  - make `Depacketizer::push()` use assembly-key helpers instead of hardcoding raw RTP timestamp grouping
  - implement only the `Progressive` case in MVP; keep `Interlaced` / `PsF` branches localized and explicitly unsupported / placeholder
  - add tests proving future scan modes can extend grouping semantics without changing `push()`
- [x] 067: Introduce an explicit video segment placement boundary and fix UYVY mapping so `SRD Offset` is interpreted in full-bandwidth sample units, not raw bytes
  - define a localized mode-aware + format-aware mapper from packet segment semantics to frame write operations
  - keep `FrameAssembler` byte-oriented and format-agnostic
  - implement only the current `Progressive + UYVY` case in MVP
  - keep `Interlaced` / `PsF` placement branches localized as placeholder / unsupported until later implementation
  - convert ST 2110-20 offset semantics to frame write byte offsets explicitly
  - add tests proving the write lands at the correct byte position
- [x] 068: Enforce pgroup alignment constraints for the current MVP video format through the localized segment-placement / validation boundary
  - for the current MVP format (`UYVY` / 4:2:2 / 8-bit), validate segment length/alignment rules implied by ST 2110-20 packetization
  - reject misaligned segment offsets/lengths explicitly
  - keep the checks localized so future formats, depths, and scan modes add their own rules rather than branching through generic assembler or depacketizer code
  - add positive/negative tests
- [x] 069: Ensure depacketizer / frame-assembly / future reconstruction path stays extensible
  - review where grouping, completion, placement, and validation logic live after 066 / 066A / 067 / 068
  - confirm that adding a second pixel format later requires localized additions only
  - confirm that adding `Interlaced` / `PsF` later requires filling mode-aware helpers / mappers, not rewriting `Depacketizer::push()`
  - define and document the boundary where future field / segment pairing and final picture reconstruction will plug in above depacketizer-emitted generic units
  - add a small architecture-focused test or compile-time check where useful
- [x] 069A: Add explicit video receive pipeline that composes depacketizer with video-unit reconstructor
  - keep `Depacketizer` and `IVideoUnitReconstructor` as separate layers
  - define `VideoReceivePipelineConfig` with `DepacketizerConfig` + `VideoUnitReconstructorConfig`
  - validate config consistency (`format` / `scan_mode`) between depacketizer and reconstructor configs
  - construct the reconstructor via `make_video_unit_reconstructor(...)`
  - implement `push(const PacketView&) -> std::vector<ReconstructedVideoFrame>` by feeding depacketizer-emitted units into the reconstructor
  - implement `reset()` so both depacketizer and reconstructor are reset
  - keep current MVP behavior implemented only for `Progressive`
  - keep non-progressive runtime boundary localized at reconstructor creation / factory path
  - add focused tests for composition, reset, and config mismatch

### A3. Video timestamp strategy
- [x] 070: Define internal timestamp type: `uint64_t ts_ns`
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
- [ ] 225: Implement interlaced video receive semantics
  - accept and interpret `F` for first/second field
  - implement correct marker semantics for end-of-field
  - implement row numbering semantics per field
  - fill the pre-defined scan-mode-aware grouping / completion / placement extension points introduced in MVP
  - keep `Depacketizer::push()` and the generic depacketizer pipeline unchanged
  - add focused tests
- [ ] 226: Implement PsF video receive semantics
  - accept and interpret `F` as segment indicator for PsF
  - implement correct marker semantics for end-of-segment
  - implement row numbering semantics per segment
  - fill the pre-defined scan-mode-aware grouping / completion / placement extension points introduced in MVP
  - keep `Depacketizer::push()` and the generic depacketizer pipeline unchanged
  - add focused tests
- [ ] 227: Implement field / segment pairing and final picture reconstruction policy
  - define how two fields or two PsF segments are paired into the final output picture
  - keep pairing / reconstruction logic separate from depacketizer packet grouping, completion, and byte-writing
  - make pairing consume generic depacketizer-emitted video units instead of changing depacketizer contracts
  - add tests for ordering, completeness, and partial/loss behavior
- [ ] 228: Add scan-mode signaling / selection path from stream description and config
  - define where `Progressive | Interlaced | PsF` is selected from SDP/config
  - validate consistency between signaled mode and runtime packet semantics
  - add tests for signaling/selection and mismatch handling

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