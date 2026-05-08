# ST2110-OBS-PLUGIN — Plan

> Рабочие правила и правила проектирования: см. `plan_rules.md`.
> Production code map: см. `code_map.md`.
> Tests file map: см. `tests_file_map.md`.
>
> `plan.md` теперь хранит:
> - Done;
> - Spec notes / deviations;
> - task backlog / phases.

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
- [x] 013: Add "extract payload span" (skip CSRC; explicit RTP header-extension tolerance remains a separate follow-up task) + tests
- [x] 020: Define structs for: ExtendedSeqHi16 + SRD header (len,row,offset,F,C)
- [x] 021: Implement parser for ExtSeqHi16 + 1..3 SRD headers + tests (synthetic bytes)
- [x] 022: Implement validation rules (SRD length > 0, <= MAXUDP, C chaining) + tests
- [x] 023: Implement helper: combine 16-bit RTP seq + ExtSeqHi16 => 32-bit ext seq + tests
---

## Spec notes / deviations
- [x] S001: `validate_st2110_20_payload_header()` currently rejects `SRD Length == 0` unconditionally, but ST 2110-20 allows this special case when there is exactly one SRD header and no sample row data follows. This must be fixed before video RX is considered spec-clean.
- [ ] S002: While MVP behavior may stay progressive-only, internal configs, state machines, packet-to-unit grouping, completion logic, and segment placement must model scan mode separately from pixel format and must not hardcode assumptions such as “timestamp group == frame”, “marker == end of frame”, or “F is always zero” in ways that make future interlace / PsF support invasive. ST 2110-20 explicitly distinguishes progressive, interlaced, and PsF behavior for `F`, marker, row numbering, grouping, placement, and signaling, so these assumptions must remain localized in dedicated mode-aware / format-aware helpers.
- [x] S003: `Depacketizer::map_segment_to_frame_write()` currently treats `SRD Offset` as a byte offset, but ST 2110-20 defines it as the horizontal position of the first full-bandwidth sample in the image pixel matrix. For UYVY / 4:2:2 this must be mapped through format-aware logic instead of written directly as bytes. This must be fixed before video RX is considered spec-clean.
- [x] S004: Current UYVY receive path does not validate pgroup alignment constraints implied by ST 2110-20 4:2:2 sampling. For 8-bit 4:2:2, packetized data must respect the pgroup structure and `SRD Length` must remain a multiple of pgroup octet size; offset semantics must also remain aligned with full-bandwidth sample positions. Validation must be added in a localized, format-aware way.
- [x] S005: Current payload validation does not enforce monotonic ordering rules for `SRD Row Number` / `SRD Offset` within a packet. ST 2110-20 requires sample rows to progress top-to-bottom and offsets within a row to progress left-to-right. This must be validated explicitly.
- [x] S006: Task 022 covered only part of payload-header validation. Size/limit checks that depend on packet/payload sizing policy (including the path toward MAXUDP-aware validation) still need an explicit follow-up task so completed work and remaining work are not conflated.
- [x] S007: Public headers currently contain non-trivial function definitions in a way that risks ODR / multiple-definition problems once the project grows beyond the current “mostly one translation unit per test executable” shape. The linkage model must be made explicit (true header-only with `inline`, or moved implementations) before backend/app growth.
- [x] S008: `PacketParseStats` structures exist, but packet parsing does not yet expose a single integrated path that records stage-specific parse results through the real parse flow. This should be fixed so parse observability is not only nominal.
- [ ] S009: Current packet size policy models a configurable UDP payload-size limit, but ST 2110-10 defines `MAXUDP` and receiver size expectations in terms of UDP datagram size, not only essence payload size. Standard UDP Size Limit and Extended UDP Size Limit handling, default behavior when `MAXUDP` is absent, and the receiver assumption around fragmented IP datagrams must be aligned with ST 2110-10 before packet sizing is considered spec-clean.
- [ ] S010: The project now has standards-aware video signaling model boundaries, structural validation, runtime/bootstrap projection helpers, raw SDP media-section parsing, per-attribute SDP parsers, and a final SDP-to-`VideoStreamSignaling` ingestion entry point. However, the receive path is still primarily driven by manual config in actual runtime/backend usage, and signaling-derived receiver bootstrap is not yet wired as the primary operational path. ST 2110-10 / -20 / -21 stream interpretation through SDP is now architecturally represented, but runtime integration of SDP-derived configuration, receiver timing policy, and end-to-end backend/app usage must still be completed without expanding ad hoc manual config assumptions.
- [x] S011: The timestamp strategy now has a standards-aware RTP timestamp mapping boundary in `video_timestamp_mapping.hpp`. RTP-domain timestamps are mapped to internal `TimestampNs` separately from synthetic fps-based timestamp generation, and the synthetic mapper is explicitly scoped to tests/scaffolding rather than standards-facing receive behavior.
- [ ] S012: Receiver timing / conformance boundary for ST 2110-21 is now explicitly modeled through `video_receiver_timing.hpp` and `video_receiver_timing_signaling.hpp`, including receiver capability/requirements, receiver-vs-signaling consistency checks, and timing-aware bootstrap composition. However, fuller ST 2110-21 receiver behavior (buffering/tolerance/release policy and conformance-related runtime behavior) is not yet implemented and must continue to be added through this existing boundary rather than being pushed into parser/reorder/depacketizer internals.
- [x] S013: `parse_packet_view_staged()` currently accepts arbitrary trailing octets after the bytes covered by `SRD Length` values. ST 2110-20 allows octets after the last Sample Row Data Segment only as terminal field/frame padding, and GPM/BPM padding octets are zero-valued. This must be validated through a localized packing-mode-aware / mode-aware boundary rather than silently tolerated on any packet.
- [x] S014: Current RTP parsing/payload extraction path does not yet provide explicit receiver-side tolerance to RTP header extensions. For a standards-aware receiver, packets with valid RTP header extensions must still have payload location derived correctly rather than being handled only under an implicit “no extensions” assumption. This must be fixed locally in RTP parsing / payload extraction logic and not spread across the rest of the receive pipeline.
- [x] S015: `VideoPackingMode` was initially modeled only in video signaling and not carried as an explicit runtime receive axis through depacketizer/runtime config/padding validation. This gap is now closed at the architecture/config level. If `BPM` remains unsupported in current MVP runtime behavior, that limitation must stay explicit, localized, and implemented through already-existing runtime branches/boundaries rather than by absence of architecture.
- [x] S016: Current standards-aware video signaling representation is still too close to internal runtime/storage concepts and does not yet model enough signaled SDP/media properties separately from internal `PixelFormat` / storage format. This must be expanded so signaled stream description is not collapsed prematurely into runtime-only concepts. In particular, the modeled representation must explicitly account for signaled stream-description properties such as `sampling`, `width`, `height`, `exactframerate`, `depth`, `colorimetry`, `TCS`, `PM`, and `SSN`, with `RANGE` allowed as optional / future-expansion coverage.
- [x] S017: Audio path now has a first-class ST 2110-30 signaling/model boundary, explicit structural validation, SDP ingestion into `AudioStreamSignaling`, signaling-to-runtime `RxAudioConfig` projection, a modeled channel-order boundary distinct from internal audio buffer layout, and an initial audio receiver bootstrap composition boundary. The MVP audio baseline is explicit as a Level A-oriented receiver baseline (`48 kHz`, `1 ms packet time`, `1..8 channels`). Remaining audio work is packet/depacketize/runtime pipeline/backend integration and buffer/layout implementation through the existing boundaries, not absence of the signaling/bootstrap architecture.
- [x] S018: Receiver-side playout / reconstruction timing boundary is now explicit in `video_playout_timing.hpp`. RTP-domain timestamp mapping remains separate from receiver playout/reconstruction timing, and receiver-side offset/delay behavior has a localized boundary above reconstructed video frames. Fuller buffering/release scheduling remains future work through the existing timing/playout boundary.
- [x] S019: RTP timestamp wraparound and long-running-stream continuity are now explicitly covered by timestamp-mapping tests. `video_timestamp_mapping_invariants_test.cpp` verifies monotonic internal timestamps, continuity across 32-bit RTP timestamp wraparound, rejection of backward/ambiguous deltas, and separation of RTP-domain mapping from synthetic/manual timing and playout timing.
- [x] S020: Generic ST 2110-20 payload-header validation currently rejects non-zero `F` / `field_id` too early. This progressive-only restriction must not live in the low-level structural payload-header layer. Generic parsing/structural validation should accept packets for all already-modeled scan-mode variants, while mode-specific acceptance/rejection of `F` must remain localized in explicit mode-aware runtime boundaries so future `Interlaced` / `PsF` work can be implemented by filling existing branches rather than rewriting the parser/validator layer.
- [x] S021: SDP ingestion path no longer treats ST 2110 timing/sender fields such as `TP`, `TROFF`, `CMAX`, `TSMODE`, and `TSDELAY` only as standalone `a=` attributes. Known ST 2110 timing/sender media type parameters are now parsed from `a=fmtp` into explicit raw fields, mapped into `VideoStreamSignaling`, and checked for conflicts with standalone compatibility attributes instead of remaining only in `unknown_parameters` and being silently ignored.
- [x] S022: SDP ingestion now parses `MAXUDP` from video `a=fmtp` through the existing raw fmtp parsing / SDP-to-signaling adapter path. Parsed `MAXUDP` is mapped to `VideoStreamSignaling::max_udp_datagram_bytes` and then reaches `PacketParsePolicy` through the existing signaling projection path, without introducing a parallel runtime config mechanism.
- [x] S023: SDP `depth=16f` is now accepted in the video SDP fmtp parsing path and mapped to the existing `VideoBitDepth{bits=16, floating_point=true}` signaling representation. Runtime pixel-format/depacketizer support remains unchanged and rejects unsupported floating-point formats through existing projection/support boundaries.
- [x] S024: The standards-aware video SDP media-property model now explicitly enumerates the known ST 2110-20 media-description variants covered by `069D8`, including additional `sampling`, `colorimetry`, and `TCS` values. `Other + raw_token` remains reserved for forward compatibility / truly unknown future values. Runtime support still rejects unsupported combinations through the existing projection/support boundaries.
- [x] S025: Receiver-side signaling validation no longer treats optional ST 2110-21 sender timing parameters, especially `CMAX` for `TP=2110TPW`, as mandatory for SDP ingestion. Structural receiver-side SDP/signaling validation accepts standard-valid Wide sender signaling when optional parameters are absent, rejects malformed present values such as `CMAX=0`, and leaves stricter sender/conformance policy to a separate localized validation mode.
- [x] S026: Video SDP ingestion now has an explicit raw boundary for session/media transport and redundancy-related SDP constructs such as `c=`, `a=source-filter`, `a=mid`, and `a=group:DUP`. These constructs are preserved in the raw SDP media-section model separately from `VideoStreamSignaling`; runtime/backend integration and redundant-stream selection policy remain future work through `214C` / `214D`.
- [x] S027: Video signaling structural validation now includes localized media-description cross-field constraints from ST 2110-20. `4:2:0` sampling variants are accepted only with progressive scan signaling, and `KEY` sampling requires alpha colorimetry and rejects normal `TCS` presence. These checks live in the signaling/model validation boundary, not in runtime `PixelFormat` projection behavior.
- [x] S028: Raw SDP `a=source-filter` handling now preserves session-vs-media scope and minimally parses the RFC-style source-filter fields while preserving the original raw attribute value. Runtime/backend consumption remains future work through `214C`.
- [x] S029: Raw SDP redundancy handling now ties `a=group:DUP` membership to actual `a=mid` values and preserves duplicate RTP video media-section candidates as explicit raw candidate summaries. Full redundant-stream selection / ST 2022-7-style behavior remains future work through `214D`.
- [x] S030: SDP `a=fmtp` parser now uses a localized strict media-parameter parsing boundary in `video_sdp_fmtp.hpp`. It rejects whitespace inside parameter tokens including around `=`, rejects malformed separators such as `sampling=...;width=...` where `;` is not followed by required whitespace, rejects empty parameters caused by doubled/trailing separators, rejects malformed `name=value` tokens, and still preserves unknown syntactically valid parameters.
- [x] S031: Raw SDP `c=` connection data now preserves the original connection address string and also exposes parsed connection-address components for future backend bootstrap: `base_address`, optional TTL, and optional address count / numaddr. Runtime/backend consumption remains future work through `214C`.
- [x] S032: SDP timing/reference-clock attributes such as `ts-refclk` and `mediaclk` can be session-level or media-level. The raw SDP ingestion boundary now preserves this scope explicitly and applies localized session/media resolution for the selected video stream. Media-level values override session-level values where allowed; duplicate values within the same scope are rejected; `fmtp` timing media parameters are treated as media-level signaling and conflict only with media-level standalone attributes for the same semantic field.
- [x] S033: Video SDP/signaling validation now enforces the ST 2110-20 `SSN` cross-field rule in the existing signaling/media-description validation boundary. `SSN=ST2110-20:2017` is accepted for normal non-`ALPHA` / non-`ST2115LOGS3` streams, while `colorimetry=ALPHA` or `TCS=ST2115LOGS3` requires `SSN=ST2110-20:2022`. Structurally unknown future `SSN` values may still be represented via `Other + raw_token`, and runtime `PixelFormat` projection remains unchanged.
- [x] S034: Video SDP/signaling validation now models `RANGE=FULLPROTECT` explicitly as a known `VideoRange` value and enforces the ST 2110-20 `RANGE` cross-field rule in the existing signaling/media-description validation boundary. With `colorimetry=BT2100`, only `NARROW` and `FULL` are accepted; outside the BT2100 context, `NARROW`, `FULLPROTECT`, and `FULL` are accepted. Structurally unknown future `RANGE` values may still be represented via `Other + raw_token`, and runtime frame storage / depacketizer behavior remains unchanged.
- [x] S035: Video SDP ingestion now models SDP `PAR` from `a=fmtp` as an explicit signaling/media-description property. `PAR` is represented as `VideoPixelAspectRatio` in `VideoMediaDescription`, defaults to `1:1` when absent at signaling level, validates positive integer parts, and is parsed/canonicalized locally in the SDP fmtp parser without changing runtime frame storage, `PixelFormat`, depacketizer, placement, or runtime projection behavior.
- [x] S036: Signaled video `width` / `height` are now validated in the standards-aware signaling/media-description boundary as `1..32767`, matching the ST 2110-20 SDP limits. This is kept separate from lower-level runtime/frame validation such as the current UYVY-specific even-width runtime constraint.
- [x] S037: SDP `exactframerate` parsing in `video_sdp_fmtp.hpp` now enforces the ST 2110-20 canonical form. Integer frame rates are accepted only as a single decimal integer (for example `25`), rational frame rates are accepted only in the smallest numerator/denominator representation (for example `30000/1001`), zero numerator/denominator and malformed forms are rejected as `InvalidValue`, and this tightening remains localized to SDP fmtp parsing without changing timestamp mapping or runtime cadence behavior.
- [x] S038: Final ST 2110 video SDP ingestion now requires `mediaclk` to be present as a media-level SDP attribute, as required by ST 2110-10. Raw SDP timing parsing continues to preserve session/media scope non-destructively, and session-level `mediaclk` may still exist in the raw model, but session-level-only `mediaclk` no longer makes final video SDP ingestion standards-clean. Media-level `mediaclk` continues to override a session-level value where both are present.
- [x] S039: Final ST 2110 video SDP ingestion now requires `ts-refclk`, and the accepted known reference-clock forms are validated strictly. Raw SDP timing parsing preserves unknown/open-ended forms only through the existing `Other` model path, while malformed known `ptp=` / `localmac=` forms are rejected as `InvalidValue`. Final ingestion now rejects SDP without `ts-refclk`, and modeled `ReferenceClock` validation also rejects empty/all-zero known reference-clock payloads unless the PTP form is explicitly `traceable`.
- [x] S040: SDP `MAXUDP` parsing is now wired through the correct path, and accepted values are now finalized against ST 2110-10 Standard UDP Size Limit / Extended UDP Size Limit semantics through the existing `PacketParsePolicy` boundary. Absent `MAXUDP` defaults to the Standard UDP Size Limit; when present, only Standard and Extended UDP size-limit values are accepted; no parallel size policy was introduced.
- [x] S041: Raw SDP `a=source-filter` parser is now tightened for known grammar details while remaining a raw transport-metadata boundary. Accepted filter-mode tokens are validated explicitly, required `nettype` / `addrtype` / destination / source-list presence is checked, malformed packed source-list forms are rejected, and the original raw value plus parsed fields remain preserved. Runtime/backend source-filter application remains future work through the existing transport/bootstrap boundary.
- [x] S042: Runtime video packet admission now has an explicit RTP payload-type boundary separated from generic RTP parsing. `RtpHeaderView` / `PacketView` parsing still only parses RTP/PT structure, while `packet_admission.hpp` performs stream-specific payload-type admission against the configured/signaled expected payload type. Wrong-PT packets are ignored/dropped locally before reorder/depacketizer use, without mutating depacketizer state.
- [x] S043: Raw video SDP `m=video` parsing / final SDP ingestion now tightens the media-line validation locally in the raw SDP media-section boundary. Selected ST 2110 video payload types are accepted only in the dynamic RTP payload type range `96..127`, the `m=video` port token must be structurally valid, and the media-line protocol is currently constrained to the explicitly supported RTP profile `RTP/AVP`. Raw media-line text remains preserved for future transport/bootstrap use, and no socket bind/join behavior was mixed into this boundary.
- [x] S044: Runtime UDP datagram handling now has a local RTP/RTCP tolerance boundary before media packet parsing/pipeline use. RTCP-like datagrams are classified and tolerated locally, counted as control datagrams, not fed into `PacketView::from_udp_datagram()` / `parse_packet_view_staged(...)`, and not counted as malformed ST 2110-20 media packets. Actual RTCP semantic interpretation remains out of MVP.
- [x] S045: Cross-packet SRD ordering validation is now enforced inside the depacketizer assembly-unit boundary instead of remaining packet-local only. For the current `Progressive + GPM` MVP path, `SRD Row Number` is rejected if it goes backwards within the current assembly unit, and within the same row `SRD Offset` must strictly increase across successive segments/packets. Regressing or overlapping later segments are rejected before any write mutates the current frame assembly state.
- [x] S046: `Depacketizer::write_packet_segments()` now validates/maps the whole packet atomically before mutating the current `FrameAssembler`. All `VideoFrameWriteOp`s for the packet are collected and validated first, including assembly-unit-local ordering checks, and only then are writes applied. An invalid later SRD segment in the same packet no longer partially mutates the current assembly unit.
- [x] S047: Standards-clean `VideoStreamSignaling` validation now requires `VideoMediaDescription::signal_standard` explicitly. The gap between SDP ingestion, which already required `SSN`, and manually constructed signaling objects is closed in `video_signaling.hpp` through an explicit required-`SSN` validation helper. No implicit synthetic/manual fallback path is used by generic signaling validation or by final SDP/runtime/bootstrap projection paths. Cross-field `SSN` rules about `ST2110-20:2017` vs `ST2110-20:2022` remain separate from this boundary.
- [x] S048: ST 2110-21 sender timing signaling is now corrected in the existing video SDP ingestion / signaling validation boundaries. Final SDP ingestion no longer silently defaults absent `TP` to `VideoSenderType::Narrow`; standards-clean video SDP now requires `TP` to be explicitly present in `a=fmtp`. At the same time, `validate_video_sender_signaling()` no longer rejects `TROFF` / `CMAX` for `Narrow` or `NarrowLinear` solely by sender class; these parameters are treated according to ST 2110-21 optional-parameter semantics, with validation remaining localized to “present and structurally/policy valid” cases. `TROFF=0` is rejected, malformed/invalid `CMAX` remains rejected by the local modeled policy, and stricter receiver/conformance checks remain separated in the receiver-timing boundary rather than being pushed into normal SDP ingestion.
- [x] S049: Raw SDP `c=` connection-data parsing is now tightened in the existing raw SDP transport boundary. Known structural forms are validated explicitly for `nettype=IN`, `addrtype=IP4` and `addrtype=IP6`, non-empty base connection address is required, and slash parameters are accepted only where they are structurally meaningful for the address type / multicast shape. Malformed TTL / address-count forms are rejected locally in raw SDP parsing, while `c=` remains preserved as raw transport metadata outside `VideoStreamSignaling` and no socket join / multicast-source-filter behavior is introduced here.
- [x] S050: Backend lifecycle boundary is no longer skeleton-oriented. `IRxVideoBackend::start_video(...)`, `IRxAudioBackend::start_audio(...)`, and `IRxBackend::stop()` now use an explicit result-returning, state-aware lifecycle boundary via `RxBackendState` / `RxBackendLifecycleResult`, with localized policy for repeated start, stop-before-start, repeated stop, and retry after failed start. Future socket/MTL runtime work must extend this existing boundary rather than reintroducing silent no-op behavior, ad hoc logging-only failure handling, or sink-coupled runtime error reporting.
- [x] S051: The socket platform boundary is no longer deferred behind concrete Linux receive-loop work. An OS-neutral socket runtime boundary now exists in `socket_runtime.hpp`, modeling socket address family, bind endpoint, multicast membership, socket-open config, config projection from `RxVideoConfig`, and abstract receive-port lifecycle separately from Linux/Winsock implementation details. Future Linux and Windows socket backends must fill this boundary rather than reshaping public backend/runtime contracts.
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
- [x] 035: Extend common error/result model for operational backend/runtime failures
  - keep parse/validation failures and backend/runtime failures explicitly distinguishable
  - cover at least:
    - invalid backend state / wrong lifecycle transition
    - socket/system operation failure
    - bind failure
    - multicast join/leave failure
    - receive-loop failure / interrupted/aborted I/O
  - avoid silently collapsing real runtime failures into generic parse errors or no-op behavior
  - keep the model reusable for:
    - socket and MTL backends
    - video and audio backends
  - add focused tests
- [x] 036: Add explicit backend lifecycle result/state boundary before real socket/MTL runtime work
  - replace the current placeholder-only `void` start/stop contract with an explicit result-returning and state-aware backend lifecycle boundary, or an equivalent explicit boundary with the same guarantees
  - define localized policy for:
    - start on an already-started backend
    - stop before successful start
    - repeated stop
    - failed start followed by retry / cleanup
  - preserve:
    - existing media capability model
    - future combined video+audio backend extensibility
    - backend-kind-agnostic factory/selection shape
  - keep transport/runtime failure reporting out of sinks and out of ad hoc logging-only paths
  - add focused backend-interface regression tests

### A1. Video packet model
- [x] 040: Define `PacketView` (rtp header + ext seq32 + srd list + payload bytes)
- [x] 041: Implement `PacketView::from_udp_datagram()` (parses all layers) + tests
- [x] 042: Add packet stats counters (parse_fail, bad_version, short_packet, bad_srd, etc.)
- [x] 043: Fix zero-length SRD special-case according to ST 2110-20 + tests
- [x] 044: Add localized format-aware segment constraints helper(s)
  - define helper/API boundary where packet/segment validation can depend on active video format
  - keep generic ST 2110-20 parsing separate from format-specific receive constraints
  - ensure adding a new format later only requires a new `case`/helper/tests
- [x] 045: Validate SRD ordering rules within one RTP packet
  - `SRD Row Number` must not go backwards
  - within the same row, `SRD Offset` must not go backwards
  - keep progressive-only assumptions localized so interlace/PsF support can be added later
  - add focused tests for valid and invalid 2-SRD / 3-SRD packets
- [x] 045A: Move `F` / `field_id` handling out of generic ST 2110-20 payload-header validation
  - keep `parse_st2110_20_payload_header(...)` and generic payload-header validation structural rather than progressive-only
  - generic low-level validation may validate wire-format structure, ordering, and non-mode-specific constraints, but must not reject non-zero `F` merely because current runtime MVP behavior is progressive-only
  - move scan-mode-specific acceptance/rejection of `F` to explicit mode-aware/runtime-support boundaries above the low-level payload-header layer
  - add focused tests proving that:
    - generic payload-header parsing/validation accepts structurally valid packets with `F != 0`
    - current progressive runtime path still rejects such packets only through an explicit localized mode-aware boundary
- [x] 046: Add explicit follow-up validation for size-limit policy
  - separate pure wire-format parsing from size-limit/config-policy checks
  - define where MAXUDP-related constraints will live for MVP
  - add tests covering oversized payload / inconsistent header+payload sizing behavior
- [x] 046A: Align packet size policy with ST 2110-10 UDP datagram size semantics
  - model packet-size policy in terms of UDP datagram size, not only essence payload bytes
  - define Standard UDP Size Limit / Extended UDP Size Limit behavior for MVP
  - define default behavior when `MAXUDP` is absent
  - keep pure wire-format parsing separate from SDP/signaling-derived sizing policy
  - document/localize the current stance on fragmented IP datagrams
  - add focused positive/negative tests
- [x] 046B: Add localized validation for trailing payload padding semantics
  - distinguish bytes covered by SRD segments from optional trailing payload padding
  - for current MVP progressive path, allow trailing padding only where current completion / packing policy permits terminal-packet padding
  - validate that accepted padding octets are zero-valued
  - keep generic `PacketView` parsing separate from packing-mode / scan-mode-specific padding policy
  - add focused positive/negative tests
- [x] 047: Add integrated packet-parse stats recording path
  - provide one real parse entry point that records `PacketParseStage` failures/successes
  - make sure the counters reflect the actual parse pipeline instead of only helper-level unit tests
  - add tests for per-stage accounting
- [x] 047A: Add receiver-side RTP header extension tolerance in RTP parsing / payload extraction path
  - correctly skip RFC 3550 RTP header extension area when extension bit is set
  - keep extension handling localized in RTP parsing / payload-span logic
  - payload extraction must remain correct with combinations of CSRC and header extensions
  - extension contents do not need semantic interpretation in MVP unless required later
  - add focused positive/negative tests

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
- [x] 069B: Add a standards-aware video SDP/signaling model boundary
  - **цель этой группы задач в MVP — заложить полную архитектурную ось signaling model и projection boundaries, даже если часть runtime branches и parsing coverage будет заполняться позже**
  - [x] 069B1: Define modeled video stream/signaling types separate from low-level depacketizer/runtime config
    - include key stream-description properties needed for current MVP architecture:
      - video packing mode (`GPM` / `BPM`)
      - timing-related signaling such as `mediaclk`, `ts-refclk`, `MAXUDP`, `TSMODE`, `TSDELAY`
      - ST 2110-21 sender timing/signaling properties such as sender type (`TP`) and optional `TROFF` / `CMAX`
    - model reference-clock signaling through an extensible `ReferenceClock` structure rather than a closed enum-only representation
    - предусмотреть modeled video SDP/media properties, которые нельзя сводить только к internal `PixelFormat` / runtime storage format
    - explicitly cover modeled representation for signaled stream-description properties such as:
      - `sampling`
      - `width`
      - `height`
      - `exactframerate`
      - `depth`
      - `colorimetry`
      - `TCS`
      - `PM`
      - `SSN`
      - `RANGE` as optional / future-expansion field
  - [x] 069B2: Add explicit structural validation boundaries inside signaling model
    - validate reference clock consistency
    - validate sender timing signaling consistency
    - validate media clock / timestamp mode enums
    - add a localized future timing-related interpretation entry point where `TSDELAY` is carried through even if full semantics are not yet implemented
  - [x] 069B3: Add explicit adapters/projections from `VideoStreamSignaling`
    - derive `PacketParsePolicy` from signaling model
    - derive runtime/manual `RxVideoConfig` from signaling model while injecting transport/network fields separately
    - validate signaling first, then validate projected runtime config
  - [x] 069B4: Add explicit projection from `VideoStreamSignaling` to runtime video receive pipeline config
    - derive `DepacketizerConfig` from signaling model
    - derive `VideoUnitReconstructorConfig` from signaling model
    - derive `VideoReceivePipelineConfig` from signaling model
    - keep runtime policy inputs that are not signaled (for example `PartialFramePolicy`) as explicit adapter parameters rather than hiding them inside signaling model
    - ensure future non-progressive modes are projected structurally without baking runtime-support assumptions into the adapter
  - [x] 069B5: Define the runtime integration boundary where signaling-derived config becomes the primary receiver bootstrap path
    - make signaling-derived config a first-class runtime input alongside the current manual/synthetic path
    - keep current manual-config path usable for tests and scaffolding
    - do not require full SDP parser yet; only make the receiver-side integration boundary explicit
    - include composition of signaling-derived packet parse policy and signaling-derived receive pipeline config as one receiver bootstrap path
    - add focused tests for signaling-driven config composition / mismatch handling
- [x] 069C: Define an explicit ST 2110-21 video receiver timing/conformance boundary
  - **цель этой задачи в MVP — заложить capability/timing/tolerance architecture boundary, even if полный standards-aware behavior будет реализовываться позже**
  - [x] 069C1: Define modeled receiver timing/capability boundary
    - add explicit receiver timing capability model separate from depacketizer/reorder internals
    - add explicit receiver timing requirements model for which signaled timing inputs are required/consumed
    - add structural validation for receiver timing config
    - keep this layer independent from packet parsing and unit assembly internals
    - add focused tests
  - [x] 069C2: Add explicit consistency validation between receiver timing boundary and video signaling model
    - validate receiver capability against signaled sender timing type
    - validate required timing/signaling inputs against `VideoStreamSignaling`
    - keep this consistency check separate from depacketizer/pipeline logic
    - add focused positive/negative tests
  - [x] 069C3: Add signaling-driven bootstrap/composition boundary for receiver timing config
    - thread receiver timing config into signaling-driven receiver bootstrap path as an explicit input
    - keep manual/test scaffolding path explicit
    - do not yet implement full buffering/playout behavior
    - add focused composition tests
  - [x] 069C4: Add architecture-focused regression tests for receiver timing boundary placement
    - prove receiver timing assumptions remain outside parser/reorder/depacketizer internals
    - prove later ST 2110-21 behavior can be filled into the existing boundary without reshaping public contracts
- [x] 069D: Add SDP parsing / ingestion path for video signaling model
  - **цель этой группы задач в MVP — заложить полную SDP/media-section ingestion architecture уже сейчас, even if coverage of many specific `a=` attributes will be expanded later**
  - [x] 069D0: Define raw SDP video media-section model / parsing boundary
    - define a dedicated raw parsed representation for one SDP video media section before mapping to `VideoStreamSignaling`
    - keep this raw layer separate from:
      - `VideoStreamSignaling` modeled representation
      - structural signaling validation
      - runtime/manual config projection
      - receiver bootstrap composition
    - the raw media-section boundary must explicitly model at least:
      - `m=` media line information relevant to the selected video payload type
      - payload-type binding for the selected video stream
      - raw `a=rtpmap` data for the selected payload type
      - raw `a=fmtp` payload for the selected payload type
      - timing/reference/sender-related `a=` attributes relevant to the current modeled boundary
      - preservation of currently unhandled/unknown `a=` attributes so future SDP coverage extends locally
    - make payload-type-specific matching explicit so later SDP support extends by filling existing per-attribute/per-PT branches rather than reshaping the ingestion pipeline
    - add focused tests for:
      - selecting the correct video media section / payload type
      - payload-type mismatch
      - missing required raw attribute association
      - duplicate relevant attributes
      - preservation of unknown `a=` attributes in the raw media-section model
  - [x] 069D1: Add pure SDP `a=fmtp` parsing layer for video media description
    - parse one ST 2110 video `a=fmtp` attribute payload into a dedicated raw parsed structure
    - keep parsing separate from `VideoStreamSignaling` mapping, validation, and runtime config projection
    - parse current core media-description fields needed by the modeled boundary:
      - `sampling`
      - `width`
      - `height`
      - `exactframerate`
      - `depth`
      - `colorimetry`
      - `PM`
      - `SSN`
      - optional `TCS`
      - optional `RANGE`
      - flag parameters such as `interlace` / `segmented`
    - preserve unknown parameters/flags instead of rejecting them, so future SDP coverage extends locally
    - add focused tests for valid parsing, payload-type mismatch, malformed numeric values, duplicate keys, and unknown-token preservation
  - [x] 069D2: Map parsed SDP video media-description attributes to `VideoStreamSignaling`
    - convert parsed raw fmtp values to modeled enums/fields in `VideoStreamSignaling`
    - derive `scan_mode` from parsed SDP flags in a localized adapter
    - derive `packing_mode` and signaled media-description properties without mixing runtime-support assumptions into the parser
    - keep structural signaling validation in existing validation helpers
    - add focused tests
  - [x] 069D3: Add parsing for timing/reference-clock/sender-timing SDP attributes and media type parameters
    - parse standalone SDP attributes such as `ts-refclk` and `mediaclk`
    - parse ST 2110 timing/sender media type parameters from `a=fmtp` where they are signaled as media type parameters:
      - `TP`
      - `TROFF`
      - `CMAX`
      - `TSMODE` where applicable
      - `TSDELAY` where applicable
    - keep standalone-attribute parsing, if currently supported, as explicit compatibility branches rather than the only parsing path
    - ensure known ST 2110 timing/sender parameters do not remain only in `unknown_parameters`
    - map parsed values into `VideoStreamSignaling` through the existing SDP-to-signaling adapter
    - keep parsing separate from validation and separate from runtime projection
    - add focused tests for:
      - `TP` / `TROFF` / `CMAX` inside `a=fmtp`
      - `TSMODE` / `TSDELAY` inside `a=fmtp`
      - optional standalone compatibility attributes
      - conflict handling when the same semantic value appears both in `a=fmtp` and as a standalone attribute
  - [x] 069D3A: Wire known ST 2110 timing/sender fmtp parameters through raw fmtp parsing and mapping
    - extend the raw `a=fmtp` parsed structure with explicit optional fields for known timing/sender parameters instead of leaving them only in unknown-parameter storage:
      - `TP`
      - `TROFF`
      - `CMAX`
      - `TSMODE`
      - `TSDELAY`
    - update `parse_video_sdp_fmtp_payload(...)` to recognize these keys as known parameters
    - update SDP-to-`VideoStreamSignaling` mapping so these values populate:
      - `sender_type`
      - `troff_us`
      - `cmax`
      - `timestamp_mode`
      - `ts_delay_sender_ticks`
    - preserve unknown-parameter behavior for truly unknown future parameters
    - keep runtime support checks out of the parser
    - add focused tests proving that known timing/sender fields inside `a=fmtp` are not silently ignored
  - [x] 069D4: Add pure SDP `a=rtpmap` parsing/binding for the selected video payload type
    - parse and bind `a=rtpmap` for the selected payload type inside the raw SDP media-section boundary
    - keep `a=rtpmap` parsing separate from `a=fmtp` parsing and separate from signaling validation
    - ensure the binding between payload type, media section, `a=rtpmap`, and `a=fmtp` remains explicit and localized
    - add focused tests for valid/invalid `a=rtpmap` syntax, payload-type mismatch, and missing/broken bindings
  - [x] 069D5: Add final SDP-to-`VideoStreamSignaling` ingestion entry point
    - parse a video SDP/media section into `VideoStreamSignaling`
    - compose raw media-section parsing, fmtp/media-description parsing, `a=rtpmap` binding, and timing/reference-clock parsing
    - keep transport/network/bootstrap projection separate from SDP parsing
    - add focused end-to-end ingestion tests for valid/invalid SDP field mapping
  - [x] 069D6: Parse and map `MAXUDP` from video SDP `a=fmtp`
    - extend the existing raw `a=fmtp` parsing structure with `MAXUDP`
    - parse `MAXUDP` through the existing numeric parsing conventions
    - map parsed `MAXUDP` into `VideoStreamSignaling::max_udp_datagram_bytes`
    - keep packet-size policy derivation in the existing `packet_parse_policy_from_video_stream_signaling(...)` path
    - when `MAXUDP` is absent, preserve the existing default Standard UDP Size Limit behavior
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_fmtp.hpp`
      - `video_sdp_signaling_adapter.hpp`
      - `video_signaling.hpp` only if validation/projection needs a tiny adjustment
      - related tests only
    - do not introduce a parallel runtime config path for `MAXUDP`
    - add focused tests for:
      - valid `MAXUDP`
      - absent `MAXUDP`
      - malformed `MAXUDP`
      - propagation into `VideoStreamSignaling`
      - propagation into packet parse policy
  - [x] 069D7: Support SDP `depth=16f` in video media-description parsing
    - keep `depth` as a signaling/media-description property, not as a runtime `PixelFormat`
    - accept standard `depth=16f` and map it to the existing floating-point depth representation
    - keep integer depths working unchanged
    - runtime projection may still reject unsupported floating-point formats through existing `PixelFormat` / support boundaries
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_fmtp.hpp`
      - `video_sdp_signaling_adapter.hpp`
      - `signaling_structs.hpp` only if the current raw representation cannot carry `16f` cleanly
      - `video_signaling.hpp` only for local validation acceptance
      - related tests only
    - do not expand actual frame storage / depacketizer runtime pixel-format support in this task
    - add focused tests for:
      - `depth=8`
      - `depth=10`
      - `depth=12`
      - `depth=16`
      - `depth=16f`
      - malformed depth tokens
  - [x] 069D8: Complete explicit known ST 2110-20 video SDP media-property enum coverage
    - add explicit modeled values for known standard SDP media-description variants currently falling into `Other`
    - cover at least:
      - sampling variants:
        - `CLYCbCr-4:4:4`
        - `CLYCbCr-4:2:2`
        - `CLYCbCr-4:2:0`
        - `ICtCp-4:4:4`
        - `ICtCp-4:2:2`
        - `ICtCp-4:2:0`
      - colorimetry variants:
        - `ST2065-3`
        - `UNSPECIFIED`
        - `XYZ`
        - `ALPHA`
      - transfer characteristic system variants:
        - `LINEAR`
        - `BT2100LINPQ`
        - `BT2100LINHLG`
        - `ST2065-1`
        - `ST428-1`
        - `DENSITY`
        - `ST2115LOGS3`
        - `UNSPECIFIED`
    - keep `Other` for forward compatibility and truly unknown future values
    - update only the existing signaling enum / mapping / validation boundaries where possible
    - runtime support may continue to reject combinations that cannot be projected to current `PixelFormat`
    - prefer minimal changes to existing `.hpp` files:
      - `signaling_structs.hpp`
      - `video_sdp_signaling_adapter.hpp`
      - `video_signaling.hpp`
      - related tests only
    - do not implement new pixel formats or frame storage layouts in this task
    - add focused tests proving:
      - known standard tokens map to explicit enum values
      - unknown future tokens are preserved through `Other`
      - runtime projection remains localized and does not silently accept unsupported formats
  - [x] 069D9: Relax receiver-side optional sender-timing validation for SDP ingestion
    - ensure structural receiver-side signaling validation accepts absent optional sender timing parameters when the standard permits them
    - specifically, `TP=2110TPW` must not require explicit `CMAX` merely for receiver-side SDP ingestion
    - keep stricter sender/conformance validation, if needed, as a separate localized helper or future policy mode
    - prefer minimal changes to existing `.hpp` files:
      - `video_signaling.hpp`
      - `video_receiver_timing_signaling.hpp` only if receiver-timing consistency currently depends on stricter semantics
      - related tests only
    - avoid renaming existing public helpers unless necessary
    - add focused tests for:
      - `TP=2110TPW` with `CMAX`
      - `TP=2110TPW` without `CMAX`
      - malformed `CMAX`
      - receiver capability rejection still happening in the receiver timing boundary rather than generic SDP validation
  - [x] 069D10: Add raw SDP session/media transport and redundancy boundary for video ingestion
    - introduce or extend a raw SDP parsing structure that can preserve transport/redundancy-related SDP constructs separately from `VideoStreamSignaling`
    - cover at least:
      - media-level or session-level `c=` connection data relevant to the selected video media section
      - `a=source-filter`
      - `a=mid`
      - `a=group:DUP`
      - unknown session/media attributes needed for future extension
    - this task is architectural: it does not need to implement full backend/runtime redundant-stream behavior
    - keep transport/network/backend projection separate from pure SDP parsing
    - prefer minimal changes to existing `.hpp` files where practical:
      - `video_sdp_media_section.hpp`
      - `video_sdp_ingestion.hpp`
      - possibly `signaling_structs.hpp` only if a small raw/bootstrap carrier is needed
      - related tests only
    - do not reshape existing `VideoStreamSignaling` unless a field is truly part of the modeled stream essence/timing description
    - add focused tests proving:
      - `c=` is parsed/preserved
      - `a=source-filter` is parsed/preserved
      - `a=mid` is parsed/preserved
      - `a=group:DUP` is parsed/preserved
      - unknown attributes are preserved rather than rejected
      - existing single-media-section ingestion behavior remains unchanged
  - [x] 069D11: Add ST 2110-20 video media-description cross-field validation
    - add localized structural validation for cross-field constraints that cannot be checked by validating each media-description enum/value independently
    - cover at least:
      - 4:2:0 sampling variants are valid only for progressive scan signaling:
        - `YCbCr-4:2:0`
        - `CLYCbCr-4:2:0`
        - `ICtCp-4:2:0`
      - `KEY` sampling requires alpha-oriented signaling:
        - require `colorimetry=ALPHA`
        - reject normal `TCS` presence for `KEY` unless a later explicit standard-backed branch says otherwise
    - keep this as signaling/model validation, not runtime `PixelFormat` projection
    - prefer minimal changes to existing `.hpp` files:
      - `video_signaling.hpp`
      - related tests only
    - do not change `PixelFormat`, `VideoFrame`, depacketizer, segment placement, or runtime projection shape
    - add focused tests proving:
      - progressive 4:2:0 signaling is structurally accepted even if runtime projection is still unsupported
      - interlaced/PsF 4:2:0 signaling is structurally rejected
      - `KEY + ALPHA` without TCS is accepted
      - `KEY` with non-alpha colorimetry is rejected
      - `KEY` with normal TCS is rejected
      - unsupported-but-standard media combinations still fail only at localized runtime projection boundaries
  - [x] 069D12: Preserve and minimally parse SDP `a=source-filter` scope in raw media-section model
    - keep `source-filter` handling in the raw SDP layer, separate from `VideoStreamSignaling`
    - preserve whether each source filter came from:
      - session level
      - selected media section level
    - minimally parse the source-filter structure into fields while preserving the raw value:
      - filter mode / type token
      - network type
      - address type
      - destination address
      - one or more source addresses
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_media_section.hpp`
      - related tests only
    - acceptable minimal implementation options:
      - either split storage into `session_source_filters` and `media_source_filters`
      - or add an explicit `Scope { Session, Media }` field to `RawSdpSourceFilter`
    - do not wire source filters into socket/backend behavior in this task
    - add focused tests proving:
      - session-level source-filter is preserved with session scope
      - media-level source-filter is preserved with media scope
      - both can coexist
      - parsed fields are populated for valid source-filter syntax
      - original raw value is still preserved
      - unknown/unsupported source-filter shape is rejected only if structurally malformed, not because runtime multicast support is missing
  - [x] 069D13: Strengthen raw SDP `a=group:DUP` / duplicate media-section boundary
    - keep redundancy modeling in the raw SDP layer, separate from `VideoStreamSignaling`
    - tie `group:DUP` membership to actual `a=mid` values rather than treating the mere presence of any `group:DUP` as sufficient
    - preserve duplicate video media-section candidates for future redundant-stream selection
    - a minimal candidate model is enough for MVP, for example:
      - media line
      - selected payload type
      - `mid`
      - media-level `c=`
      - media-level source filters
      - selected payload-bound `rtpmap` / `fmtp` raw strings if already available without duplicating the full parser
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_media_section.hpp`
      - possibly `video_sdp_ingestion.hpp` only if final ingestion needs to ignore candidates explicitly
      - related tests only
    - do not implement full redundant RTP stream selection or ST 2022-7 behavior in this task
    - add focused tests proving:
      - two matching video media sections without `group:DUP` are rejected
      - two matching video media sections with unrelated `group:DUP` are rejected
      - two matching video media sections whose `mid`s are in `group:DUP` preserve the primary/default section and duplicate candidate summary
      - existing single-media-section SDP ingestion remains unchanged
  - [x] 069D14: Make SDP `a=fmtp` media-parameter parsing strict enough for ST 2110-20 grammar
    - keep the parser strict according to current project rule “strict parse, explicit fallback”
    - reject whitespace around `=` in known and unknown `name=value` parameters
    - reject malformed separators such as `sampling=YCbCr-4:2:2;width=1920` when the grammar requires semicolon followed by whitespace
    - reject empty parameters caused by doubled separators
    - continue preserving unknown syntactically valid parameters
    - keep external line trimming / CRLF handling, but do not use trimming to silently accept malformed parameter tokens
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_fmtp.hpp`
      - related tests only
    - do not change signaling adapter, runtime projection, or SDP media-section selection unless absolutely necessary
    - add focused tests proving:
      - valid `; ` separated parameters are accepted
      - missing whitespace after `;` is rejected
      - whitespace before or after `=` is rejected
      - unknown valid parameters are preserved
      - duplicate known fields still reject as before
      - existing valid ST 2110 fmtp examples remain accepted
  - [x] 069D15: Preserve session/media scope for SDP timing and reference-clock attributes
    - explicitly model whether timing/reference-clock attributes came from session level or selected media level
    - cover at least:
      - `ts-refclk`
      - `mediaclk`
      - optional compatibility forms currently modeled as standalone attributes:
        - `TSMODE`
        - `TSDELAY`
        - `TP`
        - `TROFF`
        - `CMAX`
    - define localized resolution behavior for final video stream ingestion:
      - media-level value overrides / specializes session-level value where the SDP/RFC rules allow it
      - conflicting duplicate media-level values remain invalid
      - conflicting duplicate session-level values remain invalid
    - keep this resolution in SDP ingestion / raw-to-signaling composition, not in runtime projection
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_media_section.hpp`
      - `video_sdp_timing_attributes.hpp`
      - `video_sdp_ingestion.hpp`
      - related tests only
    - add focused tests proving:
      - session-level `ts-refclk` / `mediaclk` are applied to selected video stream
      - media-level value overrides session-level value where allowed
      - duplicate media-level timing attributes are rejected
      - existing media-level-only SDP behavior remains unchanged
  - [x] 069D16: Parse multicast address parameters from SDP `c=` connection data in raw SDP boundary
    - keep `c=` data as raw SDP transport metadata, not as `VideoStreamSignaling`
    - preserve the original connection address string
    - additionally expose parsed connection-address components where present:
      - base address
      - optional TTL for IPv4 multicast-style form
      - optional address count / numaddr
    - keep backend/socket join behavior out of this task
    - prefer minimal changes to existing `.hpp` files:
      - `video_sdp_media_section.hpp`
      - related tests only
    - add focused tests proving:
      - unicast `c=IN IP4 192.0.2.10` is parsed/preserved
      - multicast `c=IN IP4 239.1.1.1/32` is parsed/preserved
      - multicast address count form is parsed/preserved if supported by the chosen raw model
      - malformed `c=` lines are rejected structurally
      - session-level and media-level `c=` behavior remains unchanged
- [x] 069E: Thread `VideoPackingMode` into runtime receive path as an explicit axis
  - **цель этой задачи в MVP — протащить packing mode как runtime/config/policy axis уже сейчас, even if часть branches пока останется `Unsupported`**
  - extend runtime receive configs / projections so packing mode reaches depacketizer, packet interpretation, and padding-validation boundaries
  - localize GPM/BPM-specific receive rules instead of leaving packing behavior implicit
  - if current MVP runtime remains GPM-only, reject BPM through an explicit localized runtime-support boundary rather than silently ignoring it
  - ensure later BPM work can be done by filling already-existing branches without changing pipeline shape/contracts
  - add focused tests for config projection and localized packing-mode behavior / rejection
- [x] 069E1: Materialize explicit packing-mode branches inside placement and padding boundaries
  - keep `VideoPackingMode` not only as a runtime config/support axis, but also as an explicit behavior-dispatch dimension inside packet-to-frame placement and trailing-padding validation
  - introduce explicit packing-mode branch structure in these boundaries (for example `GPM` branch and `BPM` branch) instead of only "validate support, then dispatch by scan mode"
  - it is acceptable that the `BPM` branch still returns localized `Unsupported` in MVP
  - ensure future BPM work (`229`) can be implemented by filling already-existing `BPM` branches/helpers without reshaping public/runtime boundary signatures
  - add focused tests proving that:
    - placement and padding logic pass through explicit packing-mode dispatch points
    - `BPM` reaches a localized explicit branch/boundary rather than being absent from the behavior shape

### A3. Video timestamp strategy
- [x] 070: Define internal timestamp type: `uint64_t ts_ns`
- [x] 071: Define a standards-aware video timestamp mapping boundary from RTP timestamp domain to internal `ts_ns`
  - **цель этой задачи в MVP — заложить correct timing architecture boundary, even if fuller standards-aware implementation comes later**
  - keep RTP timestamp domain distinct from internal nanoseconds-domain timestamps
  - explicitly handle 32-bit RTP timestamp wraparound and long-running streams
  - define where `mediaclk` / `ts-refclk` / `TSMODE` / `TSDELAY`-related interpretation will plug into the receive pipeline
  - allow a localized synthetic/manual timing path for tests and offline tools, but do not make standalone fps cadence the primary standards-facing timing model
  - keep timestamp mapping above depacketizer and separate from segment placement / packet grouping logic
- [x] 071A: Define explicit receiver-side playout / reconstruction timing boundary
  - separate RTP-domain timestamp mapping from receiver playout / reconstruction release policy
  - define where receiver-side offset/delay configuration (including future Link Offset Delay-like boundary) will live
  - define how this boundary interacts with reconstructed units / frames without burying policy in parser/reorder/depacketizer code
  - keep arrival-time smoothing, if any, as a localized optional policy and not the standards-facing timing model
- [x] 072: Add tests for video timestamp mapping invariants
  - monotonicity of emitted internal timestamps
  - correct behavior across 32-bit RTP timestamp wraparound
  - stable mapping behavior across packet grouping / reconstruction boundaries
  - long-running stream continuity tests
  - focused tests for the synthetic/manual timing path used in MVP scaffolding

---

## Track B — Audio foundations (MVP scope)

> Audio MVP should be planned against ST 2110-30 from the start. Current MVP target should assume a narrow but explicit standards-aware baseline first: a **Level A-oriented receiver baseline** with `48 kHz`, `1 ms packet time`, and `1..8 channels`, with broader profile/level expansion later on top of the already-laid architecture.

### B0. Audio common abstractions
- [x] 079: Define a standards-aware audio SDP/signaling model boundary for ST 2110-30
  - **цель этой группы задач в MVP — заложить audio signaling/model architecture boundary уже сейчас, even if only a narrow baseline is fully implemented**
  - define modeled audio stream/signaling config separate from low-level `RxAudioConfig`
  - make the MVP target explicit as a **Level A-oriented receiver baseline** rather than a generic PCM placeholder
  - capture at least the signaled media properties needed for the initial baseline (`48 kHz`, `1 ms packet time`, `1..8 channels`, AES67-compatible receive assumptions where applicable)
  - keep signaled audio/media properties separate from internal audio buffer layout/runtime config
- [x] 079A: Add explicit structural validation boundary inside audio signaling model
  - validate structural consistency of modeled audio signaling
  - validate MVP baseline / conformance assumptions for the initial Level A-oriented receiver baseline
  - validate sample rate / packet-time / channel-count consistency within the chosen baseline
  - validate signaled channel-order / channel-mapping consistency where applicable
  - keep this validation boundary separate from SDP parsing and separate from runtime config projection
- [x] 079B: Add explicit projection/adapters from audio signaling model to runtime config
  - derive `RxAudioConfig` and later runtime audio receive config from modeled signaling
  - keep transport/network fields and local receiver policy inputs explicit rather than hiding them inside signaling model
  - add focused tests for signaling-to-runtime projection and mismatch handling
- [x] 079C0: Add raw SDP audio media-section parsing boundary
  - implemented `audio_sdp_media_section.hpp` raw parsing model for selected audio payload type;
  - parses `m=audio`, payload-bound `a=rtpmap`, `a=ptime`, and payload-bound `a=fmtp` with `channel-order`;
  - preserves unknown session/media attributes;
  - keeps raw SDP parsing separate from `AudioStreamSignaling`, `RxAudioConfig`, backend transport, and runtime audio layout;
  - deliberately treats standalone `a=channel-order:` as unknown instead of standards-facing channel-order signaling.
- [x] 079C: Add SDP parsing / ingestion path for audio signaling model
  - **цель этой группы задач в MVP — довести audio SDP ingestion от raw SDP media-section до validated `AudioStreamSignaling`, не смешивая parsing, signaling validation, runtime config projection, channel layout и backend transport**
  - parse relevant ST 2110-30 / SDP attributes into modeled audio signaling structures;
  - keep parsing separate from validation and separate from runtime config projection;
  - keep raw SDP transport/session/media details outside `AudioStreamSignaling`;
  - add focused tests for valid/invalid SDP field mapping.
  - [x] 079C1: Add raw audio SDP media-section to `AudioStreamSignaling` adapter
    - implemented `audio_sdp_signaling_adapter.hpp` as an explicit raw-SDP-to-signaling adapter layer;
    - consumes `RawAudioSdpMediaSection` from `audio_sdp_media_section.hpp`;
    - maps selected payload-bound `a=rtpmap` into `AudioMediaDescription`:
      - PCM encoding from encoding name;
      - sampling rate;
      - channel count;
    - maps `a=ptime` into `packet_time_us`;
    - maps payload-bound `fmtp channel-order=...` into modeled `AudioChannelOrderSignaling`;
    - validates the resulting `AudioStreamSignaling` through the existing audio signaling validation boundary;
    - keeps transport fields, UDP port, local/destination IP and runtime `RxAudioConfig` projection out of this adapter;
    - rejects malformed or unsupported raw audio SDP combinations through localized `InvalidValue` / `Unsupported` results.
  - [x] 079C2: Add final audio SDP-to-`AudioStreamSignaling` ingestion entry point
    - implemented `audio_sdp_ingestion.hpp` as the final audio SDP-to-signaling composition boundary;
    - composes `select_raw_audio_sdp_media_section(...)` with `audio_stream_signaling_from_raw_audio_sdp_media_section(...)`;
    - provides `parse_audio_stream_signaling_from_sdp(...)` as the signaling-only final entry point;
    - validates required ST 2110 clock-signaling presence at the ingestion boundary:
      - requires `ts-refclk` at session or selected media scope;
      - requires media-level `mediaclk`;
    - keeps this entry point signaling-only:
      - no socket/backend config;
      - no runtime `RxAudioConfig` projection;
      - no audio buffer layout/channel reordering;
    - preserves unknown SDP attributes at the raw layer and ignores them in final signaling mapping unless explicitly modeled.
- [x] 079D: Add channel-order / channel-mapping modeled boundary and validation
  - implemented `audio_channel_order.hpp` as an explicit modeled boundary for parsed/effective audio channel order;
  - parses `SMPTE2110.(...)` channel-order values into ordered channel grouping records;
  - supports ST 2110-30 grouping symbols:
    - `M`;
    - `DM`;
    - `ST`;
    - `LtRt`;
    - `51`;
    - `71`;
    - `222`;
    - `SGRP`;
    - `U01`..`U64`;
  - validates declared group channel count against signaled stream channel count;
  - allows under-declared SMPTE2110 channel-order and treats the remaining stream channels as an appended Undefined group;
  - treats absent / unspecified channel-order as an effective Undefined group covering all stream channels;
  - keeps channel-order parsing and effective grouping separate from internal audio buffer layout and future reordering/adaptation behavior.
- [x] 079E: Define the runtime integration boundary where signaling-derived audio config becomes the primary receiver bootstrap path
  - implemented `audio_receiver_bootstrap.hpp` as the first explicit audio receiver bootstrap composition boundary;
  - added `AudioReceiverBootstrapConfig`, carrying:
    - projected runtime `RxAudioConfig`;
    - effective parsed `ParsedAudioChannelOrder`;
  - added `audio_receiver_bootstrap_config_from_audio_stream_signaling(...)`;
  - composes existing boundaries instead of duplicating logic:
    - `rx_audio_config_from_audio_stream_signaling(...)` for signaling-to-runtime projection;
    - `effective_audio_channel_order_from_audio_stream_signaling(...)` for absent / unspecified / under-declared channel-order handling;
  - keeps current manual/runtime transport inputs explicit:
    - UDP port;
    - RTP payload type;
    - local IP;
    - destination IP;
    - runtime audio sample format;
  - keeps current manual-config path usable for tests and scaffolding;
  - does not implement audio buffer layout, audio packet pipeline, channel reordering, socket/backend behavior, or OBS integration;
  - future audio runtime pipeline and channel mapping code should consume `AudioReceiverBootstrapConfig` / `ParsedAudioChannelOrder` rather than reparsing SDP/raw channel-order strings;
  - added focused bootstrap tests for:
    - absent channel-order becoming effective Undefined;
    - exact SMPTE2110 channel-order preservation;
    - under-declared SMPTE2110 channel-order appending Undefined remainder;
    - `Other` channel-order preservation;
    - over-declared channel-order rejection;
    - invalid runtime transport fields;
    - unsupported runtime audio sample format.
- [x] 080: Define `RxAudioConfig` (sample_rate, channels, packet_time / samples_per_packet, payload_type, ip/port, format)
  - make the initial MVP target a narrow ST 2110-30 baseline rather than a format-free placeholder
  - make that baseline explicit as a **Level A-oriented receiver baseline** with `48 kHz`, `1 ms packet time`, and `1..8 channels`
  - capture at least the parameters needed for that initial receive path
- [x] 081: Define `AudioBuffer` / `AudioFrameView` contract
  - implemented `audio_frame.hpp` as the initial owning audio sample-buffer storage and non-owning audio frame view boundary;
  - introduced `AudioSampleStorageFormat` with current MVP storage layout `InterleavedS32`;
  - added `AudioBuffer` owning storage for interleaved signed 32-bit samples;
  - added `AudioFrameView` carrying storage format, sampling rate, channel count, samples per channel, sample pointer, total sample count, frame stride, byte size, and timestamp;
  - added construction from explicit audio dimensions and from `RxAudioConfig`;
  - kept runtime/signaling audio validation outside the buffer contract;
  - kept channel-order / channel mapping / reordering outside this file and future-facing through the existing `ParsedAudioChannelOrder` boundary.
- [x] 082: Define audio sink/backend-facing interfaces or extend shared interfaces so audio can be supported without ломки video API
  - extended `backend.hpp` with audio-facing backend contracts;
  - added `IAudioFrameSink::on_audio_frame(const AudioFrameView&)`;
  - added `IRxAudioBackend`, derived from `IRxBackend`, with `start_audio(const RxAudioConfig&, IAudioFrameSink&)`;
  - preserved existing video API shape:
    - `IVideoFrameSink`;
    - `IRxVideoBackend`;
    - `IRxBackend`;
  - kept backend-facing audio API based on existing runtime/storage boundaries:
    - `RxAudioConfig`;
    - `AudioFrameView`;
  - did not introduce SDP parsing, channel-order mapping, RTP packet parsing, jitter/reorder, playout policy, socket/MTL behavior, or OBS integration into the backend interface layer;
  - updated backend interface test coverage with FakeAudioBackend -> FakeAudioSink delivery path.
- [x] 083: Add FakeAudioBackend -> FakeAudioSink test
  - covered in `tests/test_backend_interface.cpp`;
  - verifies `IRxAudioBackend` is an abstract backend specialization derived from `IRxBackend`;
  - verifies `IAudioFrameSink` receives an `AudioFrameView` emitted by a fake audio backend;
  - checks core audio view fields needed by the backend-facing contract:
    - sampling rate;
    - channel count;
    - samples per channel;
    - timestamp;
    - sample pointer;
    - sample-frame stride;
    - total sample count;
    - byte size;
  - confirms audio backend support was added without breaking existing video backend interface behavior.

### B1. Audio packet/depacketize MVP
- [x] 090: Define audio RTP packet model needed by MVP
  - implemented `audio_packet.hpp` as the first explicit audio RTP packet model boundary;
  - added `AudioPcmWireFormat` for wire PCM sample packing:
    - `L16`;
    - `L24`;
  - added `AudioRtpPacketPolicy` carrying packet-level runtime interpretation:
    - sampling rate;
    - channel count;
    - samples per packet;
    - expected RTP payload type;
    - wire PCM format.
  - added `AudioRtpPacketView` as a non-owning view over parsed RTP header metadata plus audio payload bytes;
  - added helpers:
    - `audio_pcm_wire_sample_bytes(...)`;
    - `audio_rtp_packet_policy_from_rx_audio_config(...)`;
    - `audio_rtp_packet_payload_size_bytes(...)`;
    - `make_audio_rtp_packet_view(...)`.
  - packet payload sizing is derived from:
    - `samples_per_packet`;
    - `channel_count`;
    - wire-format bytes per sample;
    - not from hardcoded `48`, stereo-only assumptions, or internal `InterleavedS32` storage.
  - packet view creation validates expected RTP payload type and exact payload byte size;
  - this task intentionally does not implement:
    - audio RTP datagram parser integration;
    - jitter/reorder;
    - audio block assembly;
    - RTP timestamp mapping to `TimestampNs`;
    - channel-order mapping / reordering;
    - socket / MTL backend behavior.
- [x] 091: Implement minimal audio RTP parser integration + tests
  - extended `audio_packet.hpp` with `parse_audio_rtp_packet_view(...)`;
  - composes existing RTP parsing helpers:
    - `parse_rtp_header(...)`;
    - `rtp_payload_span(...)`;
    - `make_audio_rtp_packet_view(...)`;
  - keeps RTP parsing generic and keeps audio stream admission policy in the audio packet boundary;
  - validates expected RTP payload type and exact audio RTP payload size through `AudioRtpPacketPolicy`;
  - preserves RTP marker, sequence number, timestamp, SSRC, payload type, and payload span metadata;
  - inherits existing RTP CSRC/header-extension tolerance from `rtp.hpp`;
  - intentionally does not interpret RTP marker as an audio block boundary;
  - intentionally does not map RTP timestamps to `TimestampNs`;
  - intentionally does not implement jitter/reorder, audio block assembly, channel-order mapping, `AudioBuffer` creation, playout policy, socket backend behavior, or MTL backend behavior.
- [x] 092: Implement audio reorder/jitter handling MVP + tests
  - implemented `audio_reorder_buffer.hpp` as the first MVP audio reorder/jitter boundary;
  - added `AudioReorderBufferConfig` with explicit fixed reorder window size;
  - added `AudioReorderBufferStats` for:
    - pushed packets;
    - popped packets;
    - duplicates;
    - late packets;
    - out-of-window packets;
    - flushed missing packets.
  - added `StoredAudioRtpPacket` as an owning stored-packet representation for audio RTP packets;
  - added `AudioFixedWindowReorderBuffer`:
    - accepts validated `AudioRtpPacketView`;
    - stores payload bytes with ownership;
    - reorders by RTP sequence number;
    - emits packets only when the expected sequence number is available;
    - rejects duplicates, late packets and packets beyond the configured window;
    - supports explicit one-step missing-packet flush through `flush_missing_once()`;
    - supports reset and pending-state inspection.
  - keeps audio reorder/jitter handling separate from:
    - RTP parsing;
    - audio RTP packet validation;
    - audio block/frame assembly;
    - RTP timestamp mapping;
    - playout timing;
    - channel-order mapping / reordering;
    - socket / MTL backend behavior.
- [x] 093: Implement audio frame/block assembly MVP + tests
  - implemented `audio_frame_assembler.hpp` as the first MVP audio RTP packet -> internal audio block assembly boundary;
  - added `AssembledAudioBlock` carrying:
    - owning `AudioBuffer`;
    - source RTP timestamp;
    - source RTP sequence number;
    - source RTP marker bit preserved as metadata;
    - `complete` flag for current one-packet-one-block MVP behavior.
  - added `AudioFrameAssemblerConfig` with explicit internal storage-format selection;
  - added `AudioFrameAssemblerStats` for:
    - packets used;
    - packets rejected;
    - blocks emitted.
  - added localized validation helpers:
    - `validate_audio_frame_assembler_config(...)`;
    - checked payload-size derivation from `samples_per_channel * channel_count * wire_sample_bytes`;
    - `decode_audio_pcm_wire_sample_to_s32(...)` for signed big-endian L16/L24 PCM sample decoding.
  - added `AudioFrameAssembler::push(const AudioRtpPacketView&)`:
    - validates assembler config;
    - validates non-zero packet audio dimensions;
    - validates wire format through `audio_pcm_wire_sample_bytes(...)`;
    - validates exact payload size;
    - creates an owning `AudioBuffer`;
    - decodes interleaved wire PCM samples into internal signed 32-bit interleaved storage;
    - preserves RTP timestamp / sequence / marker metadata without interpreting marker as a block boundary.
  - added `reset()` and `stats()` support.
  - kept audio block assembly separate from:
    - RTP parsing;
    - payload type admission;
    - reorder/jitter buffering;
    - RTP timestamp mapping to `TimestampNs`;
    - receiver playout timing;
    - channel-order / channel-mapping / reordering;
    - socket / MTL backend behavior.
- [x] 094: Add audio stats (packets_ok, packets_lost, blocks_ok, blocks_partial/dropped)
  - implemented `audio_stats.hpp` as the first shared audio receive stats boundary;
  - added `AudioReceiveStats` counters for:
    - `packets_ok`;
    - `packets_lost`;
    - `packets_rejected`;
    - `blocks_ok`;
    - `blocks_partial`;
    - `blocks_dropped`.
  - added `AudioBlockCompletionStatus` as an explicit block-completion result axis:
    - `Complete`;
    - `Partial`;
    - `Dropped`.
  - added localized helper functions:
    - `validate_audio_block_completion_status(...)`;
    - `record_audio_packet_ok(...)`;
    - `record_audio_packet_lost(...)`;
    - `record_audio_packet_rejected(...)`;
    - `record_audio_block_result(...)`;
    - `reset_audio_receive_stats(...)`.
  - kept stats accounting separate from:
    - RTP parsing;
    - RTP payload type admission;
    - audio reorder/jitter buffering;
    - audio frame/block assembly;
    - RTP timestamp mapping to `TimestampNs`;
    - receiver playout timing;
    - channel-order / channel-mapping / reordering;
    - socket / MTL backend behavior.

### B2. Audio timestamp strategy
- [x] 095: Define audio timestamp mapping / playout timing boundary to internal `ts_ns`
  - implemented `audio_timestamp_mapping.hpp` as the first explicit audio RTP timestamp mapping and receiver-side playout timing boundary;
  - added `AudioRtpTimestampMapperConfig` and `AudioRtpTimestampMapper`;
  - keeps RTP timestamp domain distinct from internal `TimestampNs`;
  - validates RTP clock rate explicitly instead of assuming a fixed hardcoded audio clock;
  - maps RTP timestamp deltas to nanoseconds through `audio_rtp_ticks_to_timestamp_ns(...)`;
  - handles 32-bit RTP timestamp wraparound through forward modulo-delta tracking;
  - rejects backward / ambiguous timestamp movement at or above half the 32-bit RTP timestamp range;
  - preserves long-running continuity through accumulated RTP ticks since an explicit anchor;
  - added `AudioReceiverPlayoutTimingConfig` and `audio_receiver_playout_timing_decision(...)` as a separate receiver-side playout-delay boundary;
  - added `AudioBlockTiming` / `audio_block_timing(...)` as a small adapter for carrying RTP timestamp, mapped media timestamp, and playout timestamp together;
  - keeps timestamp mapping separate from:
    - RTP parsing;
    - RTP payload type admission;
    - reorder/jitter buffering;
    - audio frame/block assembly;
    - packet time / samples-per-packet derivation;
    - channel-order / channel-mapping;
    - socket / MTL backend behavior.
- [x] 096: Add monotonicity / cadence tests for audio path
  - added `audio_timestamp_mapping_invariants_test.cpp`;
  - verifies regular 48 kHz / 1 ms audio RTP timestamp cadence maps to exact monotonic nanosecond timestamps;
  - verifies non-default RTP clock rates are handled through explicit clock-rate configuration rather than hardcoded 48 kHz assumptions;
  - verifies 32-bit RTP timestamp wraparound continuity;
  - verifies long-running continuity across more than one 32-bit RTP timestamp epoch;
  - verifies backward / ambiguous deltas are rejected without advancing mapper state;
  - verifies reset reanchors mapping state;
  - verifies receiver-side playout delay preserves media cadence while shifting playout timestamps.

---

## Track C — Linux backends (both required in MVP)

### C0. Socket backend common
- [x] 100: Refactor backend layer so socket/mtl can expose both video and audio capabilities without duplication explosion
- [x] 100A: Align manual/backend-facing runtime video config contracts with already-modeled runtime axes
  - `RxVideoConfig` now carries `VideoPackingMode` as an explicit manual/backend-facing runtime axis.
  - default manual/runtime video config remains `VideoPackingMode::Gpm`.
  - current MVP runtime support remains GPM-only through `validate_runtime_video_packing_mode_support(...)`:
    - `Gpm` => `Ok`;
    - `Bpm` => `Unsupported`;
    - invalid enum => `InvalidValue`.
  - signaling-to-runtime projection now preserves `signaling.packing_mode` in projected `RxVideoConfig`.
  - signaling-vs-manual config matching now verifies `cfg.packing_mode == signaling.packing_mode`.
  - no BPM runtime depacketize behavior was implemented in this task; future BPM behavior remains task `229` through the already-modeled packing-mode branches.
- [x] 101: Add backend factory / selector design (`socket|mtl`) in extendable form
  - add explicit `RxBackendKind` modeled axis separate from `RxMediaKind`
  - add backend descriptor / selection model with validation helpers
  - add abstract `IRxBackendFactory` exposing descriptor + `create_backend()`
  - add selector / creation helpers over registered backend factories
  - keep backend selection separate from concrete socket/MTL implementation details and separate from media runtime config / packet pipeline logic
  - represent temporary backend availability through validated descriptors rather than ad hoc branching
  - reject invalid factory entries and null factory-create results through localized validation/result handling
  - add focused tests for:
    - backend kind validation / parse / name mapping;
    - descriptor and selection validation;
    - media-aware factory selection;
    - unavailable / missing backend rejection;
    - null factory entry rejection;
    - null backend creation rejection

### C1. Socket video RX
- [x] 110: Implement `SocketRxVideoBackend` skeleton + smoke test
  - added concrete `SocketRxVideoBackend` as the first socket video backend skeleton;
  - added `SocketRxVideoBackendFactory` exposing the backend through the existing `IRxBackendFactory` boundary;
  - current skeleton behavior remains intentionally minimal and video-only;
  - current backend lifecycle is explicit through the existing `RxBackendLifecycleResult` / `RxBackendState` boundary, but no real socket runtime is used yet;
  - added focused smoke coverage for:
    - direct backend interface shape;
    - capability reporting;
    - lifecycle-aware start/stop placeholder path;
    - factory descriptor;
    - backend creation through the factory;
    - rejection of unsupported audio cast/use on the video-only backend.
- [x] 110A: Add OS-neutral socket runtime boundary before real Linux RX implementation
  - introduced `socket_runtime.hpp` as an OS-neutral socket runtime boundary for the project’s own socket backend;
  - modeled socket address family explicitly through `SocketAddressFamily`:
    - `IPv4`;
    - `IPv6`.
  - added family-aware structural runtime types:
    - `SocketEndpoint`;
    - `SocketMulticastMembership`;
    - `SocketRxOpenConfig`;
    - `SocketReceiveResult`.
  - added explicit validation/helpers for the boundary:
    - socket family validation/name mapping;
    - textual IPv4/IPv6 address validation helpers;
    - IPv4/IPv6 multicast detection helpers;
    - endpoint validation;
    - multicast-membership validation;
    - socket-open-config validation;
    - multicast-presence helper.
  - added explicit `RxVideoConfig -> SocketRxOpenConfig` projection through `socket_rx_open_config_from_video_config(...)`;
  - projection behavior now:
    - resolves address family from `local_ip`, or from `dest_ip` when `local_ip` is empty;
    - chooses wildcard bind address by family:
      - `0.0.0.0` for `IPv4`;
      - `::` for `IPv6`;
    - validates `dest_ip` against the resolved family;
    - creates multicast membership only for multicast destinations.
  - added abstract receive-port lifecycle boundary:
    - `ISocketRxPort`;
    - `ISocketRxPortFactory`.
  - kept Linux syscalls / Winsock APIs, packet parsing, RTCP classification, depacketizer/reconstructor pipeline, and backend receive loop outside this task;
  - added focused interface/projection tests including:
    - IPv4 unicast/multicast;
    - IPv6 unicast/multicast;
    - wildcard bind selection by family;
    - cross-family rejection.
- [x] 110B: Rebase `SocketRxVideoBackend` onto the explicit socket runtime boundary
  - introduced `socket_stub_rx_port.hpp` as a temporary concrete stub implementation of the socket runtime boundary:
    - `SocketStubRxPort`;
    - `SocketStubRxPortFactory`;
    - `make_socket_stub_rx_port_factory()`.
  - `SocketRxVideoBackend` now owns and uses `ISocketRxPortFactory` / `ISocketRxPort` rather than remaining a transport-free stub.
  - added two backend construction paths:
    - default constructor using the temporary stub port factory;
    - injected-factory constructor for tests and future concrete runtime wiring.
  - `start_video(...)` now:
    - validates runtime dependencies;
    - projects `RxVideoConfig` to `SocketRxOpenConfig` through `socket_rx_open_config_from_video_config(...)`;
    - creates a candidate port through the injected/default port factory;
    - opens the port through the runtime boundary;
    - transitions backend state to active only after successful port open.
  - `stop()` now:
    - closes the active port through the runtime boundary;
    - propagates close failures explicitly;
    - clears runtime objects after successful cleanup;
    - preserves the backend factory dependency so restart after successful stop remains valid.
  - kept backend public contract video-only and lifecycle-driven;
  - kept real Linux syscalls, Winsock calls, socket receive loop, and frame delivery out of this task;
  - added focused regression coverage for:
    - injected fake port factory wiring;
    - IPv4/IPv6 projection through backend start;
    - projection failure propagation;
    - null-port rejection;
    - port-open failure propagation and retry;
    - close-failure propagation without losing active state;
    - successful restart after stop;
    - default backend-factory creation path.
- [x] 111: Implement first concrete Linux socket receive-port runtime behind `ISocketRxPort`
  - added `linux_socket_rx_port.hpp` as the first concrete Linux implementation of the existing OS-neutral socket runtime boundary;
  - introduced `LinuxSocketRxPort` and `LinuxSocketRxPortFactory` plus `make_linux_socket_rx_port_factory()`;
  - `LinuxSocketRxPort` now provides real Linux socket behavior for the current unicast runtime path:
    - UDP socket creation by explicit `SocketAddressFamily`;
    - pre-bind socket configuration through `SO_REUSEADDR`;
    - family-aware bind for `IPv4` and `IPv6`;
    - idempotent close/cleanup behavior;
    - one-datagram receive boundary via `recv(...)`.
  - `open(...)` is transactional:
    - validates the full open request before syscalls;
    - rejects repeated open with `InvalidBackendState`;
    - leaves the object closed on create/configure/bind failure;
    - commits runtime state only after successful socket create + configure + bind.
  - current Linux runtime support boundary for this task is explicit:
    - unicast `IPv4` supported;
    - unicast `IPv6` supported when the host supports it;
    - any `multicast_membership` currently rejected with `Unsupported`.
  - `receive(...)` remains a pure socket/datagram boundary for now:
    - no RTP parsing;
    - no ST 2110 packet classification;
    - no depacketizer/frame delivery logic.
  - low-level error mapping is localized in the Linux runtime implementation:
    - bind failure -> `BindFailed`;
    - receive interruption/aborted/generic receive failures -> corresponding receive errors;
    - other socket/runtime failures -> `SystemFailure`.
  - kept backend public API unchanged;
  - kept `SocketRxVideoBackend` default runtime dependency on the stub factory for now;
  - kept multicast join/leave, backend default-factory switch, receive-loop integration, RTCP tolerance/classification, and frame delivery as follow-up tasks;
  - added focused Linux runtime tests covering:
    - IPv4 unicast open/close and repeated-open rejection;
    - IPv6 unicast open/close when supported by the host;
    - invalid config rejection;
    - explicit multicast `Unsupported`;
    - bind-failure mapping;
    - receive contract before open / with empty buffer;
    - successful receipt of one IPv4 UDP datagram without parser logic;
    - factory creation of distinct closed instances.
- [x] 111A: Switch default `SocketRxVideoBackend` runtime dependency from stub factory to the concrete Linux port factory on supported builds
  - keep injected-factory constructor for tests and future platform/runtime variants;
  - make default backend creation use the real Linux socket-port factory where available instead of the temporary stub factory;
  - keep backend public API unchanged;
  - keep platform selection localized and avoid spreading platform branching into app/bootstrap code.
- [x] 112: Implement multicast join/leave for the Linux socket receive-port through the existing family-aware socket runtime boundary
  - implement multicast join/leave on top of the Linux `ISocketRxPort` implementation rather than in backend code;
  - keep IPv4/IPv6 family handling explicit through the already-modeled runtime boundary;
  - if some concrete family branch remains temporarily unsupported, keep that limitation localized in the Linux socket runtime implementation rather than removing family coverage from the boundary;
  - keep backend public contracts unchanged;
  - add focused tests where practical for multicast lifecycle and failure mapping.
- [x] 113: Add socket video datagram receive path and feed the existing video receive pipeline
  - create/open the socket port from `SocketRxVideoBackend`;
  - receive UDP datagrams through `ISocketRxPort::receive(...)`;
  - keep datagram classification / stream admission as explicit local boundaries rather than burying them in ad hoc parser failures;
  - parse media datagrams into `PacketView`;
  - feed the current packet/reorder/video-receive pipeline;
  - deliver reconstructed video frames to `IVideoFrameSink`;
  - keep socket runtime, packet parsing, pipeline reconstruction, and timing/playout as separate layers;
  - add focused tests/smoke coverage where practical.
- [x] 114: Add periodic socket video RX stats
  - report backend/runtime-oriented stats such as:
    - received datagrams;
    - parsed/rejected packets;
    - ignored control/non-media datagrams if tracked;
    - frames delivered;
    - drops / loss-related counters exposed by existing pipeline stats.
  - keep stats collection localized and avoid spreading backend observability across sink code.
- [x] 115: Add graceful stop and cleanup for socket video RX
  - stop the receive path cleanly;
  - close the socket port through the runtime boundary;
  - release/reset backend-owned runtime objects;
  - preserve existing lifecycle guarantees for:
    - stop before successful start;
    - repeated stop;
    - retry after failed start.

### C2. Socket audio RX
- [x] 120: Implement `SocketRxAudioBackend` skeleton + smoke test
- [x] 121: Implement UDP socket open/bind for audio
- [x] 121A: Implement multicast join/leave (Linux) for socket audio receive path
- [x] 122: Connect audio parser/reorder/assembler pipeline
  - current `SocketRxAudioBackend` now connects:
    - audio RTP packet parsing;
    - audio reorder buffering;
    - audio block assembly;
    - audio RTP timestamp mapping;
    - sink delivery for assembled audio blocks;
  - before marking this task done, keep `record_rejected_packet(...)` stats mutation fully synchronized in the common socket single-media base;
  - keep temporary audio wire-format support localized through the explicit backend helper boundary.
- [x] 122A: Model explicit audio RTP wire-format / bit-depth axis through signaling/runtime/backend path
  - current audio packet model already distinguishes `AudioPcmWireFormat::{L16, L24}`;
  - current socket audio RX runtime no longer relies on a localized `LinearPcm -> L24` assumption;
  - signaling/runtime/backend path now carries explicit PCM bit depth via `AudioMediaDescription::pcm_bit_depth`, `RxAudioConfig::pcm_bit_depth`, SDP `L16`/`L24` mapping, and `audio_rtp_packet_policy_from_rx_audio_config(...)`.
- [x] 123: Add graceful stop and cleanup reuse

# Phase R — Responsibility-block refactoring

> Цель этой фазы:
> - привести каждый responsibility block к виду, который удовлетворяет `architecture_rules.md`;
> - убрать смешение обязанностей между блоками;
> - убрать generic enum-validation / helper-level `Unsupported` architecture;
> - довести архитектуру до состояния, где расширение поддержки означает дописывание concrete logic branch в нужном месте, а не переписывание соседних подсистем.
>
> Порядок блоков ниже выбран по dependency order:
> - сначала самые низкие и общие блоки;
> - затем блоки, которые потребляют их как public contracts;
> - затем backend/runtime composition;
> - затем OBS/plugin composition;
> - в самом конце build/wiring/entrypoints, чтобы фиксировать уже стабилизированную структуру.
>
> Важное уточнение:
> - `Phase R` стабилизирует архитектуру и responsibility-block decomposition, но сама по себе не завершает MVP.
> - После завершения `Phase R` remaining MVP work должен сводиться в основном к дописыванию concrete logic branches и runtime/plugin integration в уже подготовленных boundaries, а не к новому reshaping архитектуры.
> - Основные ожидаемые post-`Phase R` функциональные блокеры:
>   - незавершенный MTL video runtime/projection/delivery path;
>   - отсутствующий/незавершенный MTL audio path;
>   - еще не реализованный OBS plugin MVP layer;
>   - финальные end-to-end/demo/readiness tasks.

---

## Track R0 — Foundation block refactor

> Это блок рефакторинга responsibility block `foundation`.
>
> Ответственность блока:
> - media/backend/platform-agnostic primitives;
> - byte/endian helpers;
> - common error/result vocabulary;
> - timestamp/stat primitives;
> - named derived-value helpers.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен знать:
> - SDP;
> - ST 2110 signaling/media semantics;
> - receive pipelines;
> - socket/MTL runtime;
> - OBS/plugin concerns.

- [x] R001: Refactor `libs/st2110core/include/st2110/foundation/bytes.hpp`
  - file responsibility:
    - common byte-span / byte-view aliases and tiny byte-oriented helpers only.
  - after refactor file must answer only for:
    - byte-level generic primitives with no RTP/ST2110/media/backend meaning.
  - file must not contain:
    - RTP parsing;
    - packet validation;
    - video/audio/domain-specific helpers.

- [x] R002: Refactor `libs/st2110core/include/st2110/foundation/endian.hpp`
  - file responsibility:
    - endian-aware primitive load/store helpers only.
  - after refactor file must answer only for:
    - integer byte-order conversion primitives.
  - file must not contain:
    - RTP/ST2110 field interpretation;
    - payload-header semantics;
    - transport-policy logic.

- [x] R003: Refactor `libs/st2110core/include/st2110/foundation/error.hpp`
  - file responsibility:
    - common error codes / result helpers only.
  - after refactor file must answer only for:
    - project-wide failure vocabulary.
  - keep explicit distinction between:
    - malformed external input;
    - unsupported missing concrete logic;
    - runtime/system/backend failure.
  - file must not contain:
    - subsystem-specific validation logic;
    - hidden error mapping policy for socket/MTL/OBS.

- [x] R004: Refactor `libs/st2110core/include/st2110/foundation/timestamp.hpp`
  - file responsibility:
    - internal timestamp scalar/types only.
  - after refactor file must answer only for:
    - common timestamp primitives and tiny generic utilities.
  - file must not contain:
    - RTP timestamp interpretation;
    - playout policy;
    - media-specific timing logic.

- [x] R005: Refactor `libs/st2110core/include/st2110/foundation/stats.hpp`
  - file responsibility:
    - common backend-agnostic stats primitives and small shared stat carriers only.
  - after refactor file must answer only for:
    - generic stat vocabulary that is truly cross-block.
  - file must not become:
    - a dump of video/audio/socket/MTL-specific counters.

- [x] R006: Refactor `libs/st2110core/include/st2110/foundation/rtp_timestamp_anchor_policy.hpp`
  - file responsibility:
    - explicit named policy for initial RTP-to-internal anchor semantics only.
  - after refactor file must answer only for:
    - modeled anchor-policy axis, not actual mapping execution.
  - move/refactor from:
    - implicit hardcoded mapper anchoring currently buried in runtime/timestamp-construction paths.
  - file must not contain:
    - backend-local defaults;
    - socket runtime code;
    - synthetic fallback hidden inside constructors.

- [x] R007: Refactor `libs/st2110core/include/st2110/foundation/derived_values.hpp`
  - file responsibility:
    - named derived-value helpers only.
  - after refactor file must answer only for:
    - values that are computed from already-modeled typed inputs.
  - move/refactor from:
    - the pure derivation part of `libs/st2110core/include/st2110/config_validation.hpp`.
  - move here only helpers such as:
    - `samples_per_packet`-style derivations;
    - packet/frame sizing derivations;
    - other formula-based helpers that are not ingress validation.
  - file must not contain:
    - enum validity checks;
    - broad self-consistency validators;
    - backend support decisions.

- [x] R008: Refactor/delete `libs/st2110core/include/st2110/config_validation.hpp`
  - file responsibility:
    - temporary compatibility shim only while logic is being redistributed.
  - move/refactor out of this file:
    - pure derivations -> `foundation/derived_values.hpp`;
    - ingress-only validation -> exact ingress/adapter files that consume external input;
    - config-cross-consistency checks -> exact contract/bootstrap files that own those typed boundaries.
  - file must not retain:
    - generic enum validators;
    - “is this internal config valid?” theater;
    - catch-all validation helpers spanning several blocks.
  - if after this redistribution no declarations/helpers remain that are still the unique responsibility of this file, delete `libs/st2110core/include/st2110/config_validation.hpp` in this same task.
  - do not leave this file in the tree as:
    - an empty shim;
    - a deprecated forwarding header;
    - a compatibility placeholder with no remaining architectural responsibility.

---

## Track R1 — Standard media model block refactor

> Это блок рефакторинга responsibility block `standard media model`.
>
> Ответственность блока:
> - описать полную common typed model осей, задаваемых стандартами и required external APIs;
> - отдельно от runtime support, storage format, backend limits, OBS limits.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - сужаться до текущего Socket MVP;
> - знать delivery/storage limits;
> - знать socket/MTL runtime internals;
> - возвращать `Unsupported` как substitute for missing model.

- [x] R009: Refactor `libs/st2110core/include/st2110/model/video/video_scan_mode.hpp`
  - file responsibility:
    - modeled scan-mode axis only.
  - after refactor file must answer only for:
    - `Progressive | Interlaced | PsF` as common standard-defined axis.
  - file must not contain:
    - packet grouping policy;
    - runtime support validation;
    - backend-specific acceptance logic.

- [x] R010: Refactor `libs/st2110core/include/st2110/model/video/video_packing_mode.hpp`
  - file responsibility:
    - modeled packing-mode axis only.
  - after refactor file must answer only for:
    - recognized packing-mode variants such as `GPM` / `BPM`.
  - file must not contain:
    - payload mapping logic;
    - support matrix logic;
    - parser/runtime coupling.

- [x] R011: Refactor `libs/st2110core/include/st2110/model/video/video_media_types.hpp`
  - file responsibility:
    - standards-facing typed representation of signaled/recognized video media-description axes only.
  - after refactor file must answer only for:
    - sampling;
    - depth;
    - colorimetry;
    - TCS;
    - SSN;
    - RANGE;
    - PAR;
    - frame-rate representation;
    - other common video media-description types.
  - move/refactor from:
    - the video media-description/model part of `libs/st2110core/include/st2110/video_receive_capability.hpp`.
  - file must not contain:
    - `PixelFormat` projection;
    - MTL output-format projection;
    - delivery/handoff narrowing;
    - backend support checks.

- [x] R012: Refactor `libs/st2110core/include/st2110/model/video/video_signaling_types.hpp`
  - file responsibility:
    - typed video signaling structures only.
  - after refactor file must answer only for:
    - common video signaling carriers and related strongly-typed fields.
  - move/refactor from:
    - the typed signaling-structure part of `libs/st2110core/include/st2110/signaling_structs.hpp`;
    - the pure typed-model part of `libs/st2110core/include/st2110/video_signaling.hpp`.
  - file must not contain:
    - SDP parsing;
    - bootstrap projection;
    - backend support logic;
    - runtime pipeline config assembly.

- [x] R013: Refactor `libs/st2110core/include/st2110/model/audio/audio_signaling.hpp`
  - file responsibility:
    - standards-facing typed audio signaling/media model only.
  - after refactor file must answer only for:
    - PCM bit depth;
    - sampling rate;
    - packet time;
    - channel count;
    - channel-order signaling carrier;
    - other in-scope ST 2110-30 modeled axes.
  - file must not contain:
    - runtime audio buffer layout;
    - assembler/reorder logic;
    - Level-A-only support narrowing.

- [ ] R014: Refactor `libs/st2110core/include/st2110/model/audio/audio_channel_order.hpp`
  - file responsibility:
    - modeled channel-order convention and parsed channel-group representation only.
  - after refactor file must answer only for:
    - typed parsed/effective channel-order model.
  - file must not contain:
    - runtime reordering;
    - audio buffer layout policy;
    - backend-specific adaptation behavior.

- [ ] R015: Refactor/delete `libs/st2110core/include/st2110/signaling_structs.hpp`
  - file responsibility:
    - temporary compatibility shim only while typed signaling structures are moved out.
  - move/refactor out of this file:
    - typed video signaling structures -> `model/video/video_signaling_types.hpp`;
    - any bootstrap-oriented carriers -> `contracts/video/video_receiver_bootstrap.hpp`.
  - after refactor file must not remain as a mixed model/bootstrap dump.
  - if after this move no declarations remain that are still uniquely owned by this file and no production code still includes it for real responsibility-bearing content, delete `libs/st2110core/include/st2110/signaling_structs.hpp` in this same task.
  - do not keep this file as a compatibility umbrella once all real content has been redistributed.

- [ ] R016: Refactor/delete `libs/st2110core/include/st2110/video_signaling.hpp`
  - file responsibility:
    - temporary compatibility shim only while pure model and bootstrap/projection logic are split.
  - move/refactor out of this file:
    - typed signaling/model content -> `model/video/video_signaling_types.hpp`;
    - bootstrap/projection logic -> `contracts/video/video_receiver_bootstrap.hpp`.
  - after refactor file must not mix:
    - structural model;
    - runtime projection;
    - receiver timing bootstrap;
    - support logic.
  - if after this split no declarations/helpers remain that are still uniquely owned by this file and no production code still depends on it for non-forwarding content, delete `libs/st2110core/include/st2110/video_signaling.hpp` in this same task.
  - do not leave this file as a broad transitional façade after the split is complete.

- [ ] R017: Refactor/delete `libs/st2110core/include/st2110/video_receive_capability.hpp`
  - file responsibility:
    - temporary compatibility shim only while receive/session model and delivery/handoff model are separated.
  - move/refactor out of this file:
    - media-description/common capability types -> `model/video/video_media_types.hpp`;
    - receive-session description -> `receive/video/video_receive_description.hpp`;
    - delivery/handoff format axis -> `delivery/video/video_handoff_format.hpp`.
  - after refactor file must not remain as a combined:
    - standard model;
    - runtime receive config;
    - storage/handoff compatibility.
  - if after this split no declarations remain that are still uniquely owned by this file and no production code still includes it for real content, delete `libs/st2110core/include/st2110/video_receive_capability.hpp` in this same task.
  - do not preserve this file as a legacy umbrella over the newly separated model/receive/delivery boundaries.

---

## Track R2 — External ingress block refactor

> Это блок рефакторинга responsibility block `external ingress`.
>
> Ответственность блока:
> - strict parsing of raw external input;
> - preservation of raw external structure where needed;
> - translation of raw external input into internal typed model.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - решать runtime backend support;
> - строить backend operational configs;
> - знать delivery/storage limits;
> - валидировать внутренние enum’ы как routine architecture.

- [ ] R018: Refactor `libs/st2110core/include/st2110/ingress/shared/rtp.hpp`
  - file responsibility:
    - raw RTP header parsing / payload-span extraction only.
  - after refactor file must answer only for:
    - RFC3550/RFC8285-level structure parsing.
  - file must not contain:
    - stream payload-type admission;
    - video/audio-specific packet policy;
    - backend/runtime support logic.

- [ ] R019: Refactor `libs/st2110core/include/st2110/ingress/shared/st2110_20.hpp`
  - file responsibility:
    - raw ST 2110-20 payload-header parsing and raw structural payload constraints only.
  - after refactor file must answer only for:
    - payload-header structure;
    - SRD header parsing;
    - raw wire-level field extraction.
  - file must not contain:
    - progressive-only runtime narrowing;
    - placement into frame memory;
    - backend support logic.

- [ ] R020: Refactor `libs/st2110core/include/st2110/ingress/shared/packet_view.hpp`
  - file responsibility:
    - parsed non-owning packet view only.
  - after refactor file must answer only for:
    - parsed RTP + ST2110-20 raw packet representation.
  - file must not contain:
    - receive-pipeline behavior;
    - reorder semantics;
    - depacketizer mutation logic.

- [ ] R021: Refactor `libs/st2110core/include/st2110/ingress/shared/packet_parse.hpp`
  - file responsibility:
    - staged packet parse entry points and ingress-local parse policy only.
  - after refactor file must answer only for:
    - parse orchestration from UDP datagram to `PacketView`;
    - ingress-local size policy enforcement.
  - file must not contain:
    - payload-type admission;
    - frame assembly;
    - backend-specific runtime behavior.

- [ ] R022: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_media_section.hpp`
  - file responsibility:
    - raw video SDP media-section model and parsing only.
  - after refactor file must answer only for:
    - selected `m=video` section;
    - payload-type association;
    - raw preserved attributes/transport metadata.
  - file must not contain:
    - final `VideoStreamSignaling` semantics;
    - runtime config projection;
    - backend transport implementation.

- [ ] R023: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_fmtp.hpp`
  - file responsibility:
    - strict raw parsing of video `a=fmtp` only.
  - after refactor file must answer only for:
    - raw parsed parameter values and syntax validation.
  - file must not contain:
    - final enum mapping;
    - runtime support rejection;
    - `PixelFormat` projection.

- [ ] R024: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_rtpmap.hpp`
  - file responsibility:
    - strict raw parsing/binding of selected video `a=rtpmap` only.
  - after refactor file must answer only for:
    - payload-type-specific `rtpmap` parsing and binding.
  - file must not contain:
    - final video signaling validation;
    - backend/runtime assumptions.

- [ ] R025: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_timing_attributes.hpp`
  - file responsibility:
    - strict raw parsing of video timing/reference-clock SDP attributes only.
  - after refactor file must answer only for:
    - `ts-refclk`;
    - `mediaclk`;
    - timing-related raw SDP attribute parsing/preservation.
  - file must not contain:
    - receiver timing capability checks;
    - playout/runtime behavior;
    - backend bootstrap assembly.

- [ ] R026: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_signaling_adapter.hpp`
  - file responsibility:
    - raw-video-SDP to typed-video-signaling adaptation only.
  - after refactor file must answer only for:
    - mapping raw parsed fields into `model/video/*` types.
  - file must not contain:
    - backend support checks;
    - delivery/handoff narrowing;
    - socket/MTL runtime config creation.

- [ ] R027: Refactor `libs/st2110core/include/st2110/ingress/video/video_sdp_ingestion.hpp`
  - file responsibility:
    - final composition of raw video SDP parsing + raw-to-typed signaling ingestion only.
  - after refactor file must answer only for:
    - final video SDP -> typed signaling entry point.
  - file must not contain:
    - manual runtime transport overrides;
    - backend bootstrap assembly;
    - receive-pipeline config construction.

- [ ] R028: Refactor `libs/st2110core/include/st2110/ingress/audio/audio_sdp_media_section.hpp`
  - file responsibility:
    - raw audio SDP media-section model and parsing only.
  - after refactor file must answer only for:
    - selected `m=audio` section and raw payload-bound attributes.
  - file must not contain:
    - final `AudioStreamSignaling` support narrowing;
    - runtime config projection.

- [ ] R029: Refactor `libs/st2110core/include/st2110/ingress/audio/audio_sdp_timing_attributes.hpp`
  - file responsibility:
    - strict raw parsing of audio timing/reference-clock SDP attributes only.
  - after refactor file must answer only for:
    - structured `ts-refclk` / media-level `mediaclk` raw parsing.
  - move/refactor from:
    - any current attribute-name-only presence checks buried in audio ingestion helpers.
  - file must not contain:
    - audio receiver baseline support checks;
    - runtime playout behavior.

- [ ] R030: Refactor `libs/st2110core/include/st2110/ingress/audio/audio_sdp_signaling_adapter.hpp`
  - file responsibility:
    - raw-audio-SDP to typed-audio-signaling adaptation only.
  - after refactor file must answer only for:
    - typed audio signaling construction from raw parsed SDP.
  - file must not contain:
    - Level A support narrowing;
    - runtime `RxAudioConfig` assembly.

- [ ] R031: Refactor `libs/st2110core/include/st2110/ingress/audio/audio_sdp_ingestion.hpp`
  - file responsibility:
    - final composition of raw audio SDP parsing + raw-to-typed signaling ingestion only.
  - after refactor file must answer only for:
    - final audio SDP -> typed signaling entry point.
  - file must not contain:
    - backend bootstrap;
    - audio buffer/storage logic;
    - socket/MTL support checks.

---

## Track R3 — Delivery and conversion block refactor

> Это блок рефакторинга responsibility block `delivery and conversion`.
>
> Ответственность блока:
> - project-local storage/handoff/conversion contracts;
> - nothing about what the standard allows to receive;
> - nothing about backend capability.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок должен отделять:
> - receive-session capability;
> - project delivery/handoff capability.

- [ ] R032: Refactor `libs/st2110core/include/st2110/delivery/video/pixel_format.hpp`
  - file responsibility:
    - project-local video storage/presentation pixel-format axis only.
  - after refactor file must answer only for:
    - formats currently representable by project-local frame/view contracts.
  - file must not be treated as:
    - the full standard video model;
    - the MTL/ST20 transport truth.

- [ ] R033: Refactor `libs/st2110core/include/st2110/delivery/video/video_handoff_format.hpp`
  - file responsibility:
    - project-local video handoff/output-format axis only.
  - after refactor file must answer only for:
    - what project delivery contracts can expose.
  - move/refactor from:
    - the handoff/output/storage part of `libs/st2110core/include/st2110/video_receive_capability.hpp`.
  - file must not contain:
    - standard signaling/media-description semantics;
    - backend receive capability semantics.

- [ ] R034: Refactor `libs/st2110core/include/st2110/delivery/video/video_frame.hpp`
  - file responsibility:
    - owning/non-owning project-local video frame storage/view contract only.
  - after refactor file must answer only for:
    - local frame memory layout;
    - plane descriptions;
    - local frame/view invariants.
  - explicit expected cleanup:
    - dispatch such as `fill_planes()` must be a concrete-logic dispatch over the local delivery format;
    - unsupported local delivery formats must be localized as missing concrete logic in exact per-format branches;
    - file must not encode “what the receiver may accept from the network”.
  - file must not contain:
    - ST2110 signaling validation;
    - socket/MTL receive support logic;
    - OBS handoff logic.

- [ ] R035: Refactor `libs/st2110core/include/st2110/delivery/video/video_frame_conversion.hpp`
  - file responsibility:
    - video delivery-format conversion boundary only.
  - after refactor file must answer only for:
    - explicit conversion branches between project-local video delivery formats.
  - move/refactor from:
    - any local frame-format adaptation now buried in `video_frame.hpp`, `mtl_rx_video_backend.*`, or future OBS handoff code.
  - file must not contain:
    - receive-session admission;
    - backend start decisions.

- [ ] R036: Refactor `libs/st2110core/include/st2110/delivery/audio/audio_frame.hpp`
  - file responsibility:
    - owning/non-owning project-local audio storage/view contract only.
  - after refactor file must answer only for:
    - local audio buffer layout and view shape.
  - file must not contain:
    - SDP/audio signaling semantics;
    - RTP packet interpretation;
    - backend support logic.

- [ ] R037: Refactor `libs/st2110core/include/st2110/delivery/audio/audio_frame_conversion.hpp`
  - file responsibility:
    - audio delivery-format conversion boundary only.
  - after refactor file must answer only for:
    - explicit conversion branches into current `AudioBuffer` / `AudioFrameView`-compatible representations.
  - move/refactor from:
    - any PCM16/PCM24/other local conversion logic currently buried in assembler/backend paths.
  - file must not contain:
    - RTP parsing;
    - receiver baseline validation;
    - MTL session projection.

---

## Track R4 — Receive session contracts block refactor

> Это блок рефакторинга responsibility block `receive session contracts`.
>
> Ответственность блока:
> - public backend/session-facing contracts;
> - session/selection/bootstrap composition boundaries;
> - typed cross-consistency at contract boundaries.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - делать raw parsing;
> - делать runtime packet processing;
> - знать platform socket syscalls или MTL API internals.

- [ ] R038: Refactor `libs/st2110core/include/st2110/contracts/backend/backend.hpp`
  - file responsibility:
    - backend-facing lifecycle/state/stats/sink interfaces only.
  - after refactor file must answer only for:
    - public backend contracts.
  - file must not contain:
    - socket-specific operational config;
    - MTL device/session details;
    - media parsing/runtime helpers.

- [ ] R039: Refactor `libs/st2110core/include/st2110/contracts/backend/backend_factory.hpp`
  - file responsibility:
    - abstract backend-factory contract and public backend-kind/media-kind-facing descriptor shapes only.
  - after refactor file must answer only for:
    - factory contract and descriptor vocabulary.
  - file must not contain:
    - compiled-registry composition;
    - support matrix logic;
    - platform-specific branching.

- [ ] R040: Refactor `libs/st2110core/include/st2110/contracts/backend/backend_factory_registry.hpp`
  - file responsibility:
    - declaration-only public registry API only.
  - after refactor file must answer only for:
    - public registry query/selection API shape.
  - move/refactor from:
    - declaration fragments currently mixed into `backend_factory.hpp` or old registry implementation.
  - file must not contain:
    - concrete compiled factory list;
    - build/platform logic.

- [ ] R041: Refactor `libs/st2110core/include/st2110/contracts/video/rx_video_session_config.hpp`
  - file responsibility:
    - typed backend-facing/manual video session config only.
  - after refactor file must answer only for:
    - session config shape consumed by backend/bootstrap layers.
  - move/refactor from:
    - the video runtime/session-config portion of `libs/st2110core/include/st2110/rx_config.hpp`.
  - file must not contain:
    - raw SDP parsing;
    - packet/depacketize behavior;
    - delivery conversion behavior.

- [ ] R042: Refactor `libs/st2110core/include/st2110/contracts/video/video_receiver_bootstrap.hpp`
  - file responsibility:
    - explicit composition boundary from typed video signaling + receiver policy inputs into backend-facing/bootstrap carriers only.
  - after refactor file must answer only for:
    - bootstrap assembly and cross-consistency at this contract boundary.
  - move/refactor from:
    - bootstrap/projection logic currently spread across `libs/st2110core/include/st2110/signaling_structs.hpp`;
    - `libs/st2110core/include/st2110/video_signaling.hpp`;
    - `libs/st2110core/include/st2110/video_receiver_timing.hpp`;
    - `libs/st2110core/include/st2110/video_receiver_timing_signaling.hpp`.
  - file must not contain:
    - raw SDP parsing;
    - concrete packet/timestamp/runtime execution.

- [ ] R043: Refactor `libs/st2110core/include/st2110/contracts/video/video_backend_selection.hpp`
  - file responsibility:
    - typed backend selection/support decision contract for video only.
  - after refactor file must answer only for:
    - backend-kind-aware video backend selection and associated typed result shape.
  - move/refactor from:
    - selection/support shape currently mixed across `backend_factory.hpp` and `libs/st2110core/include/st2110/video_backend_support.hpp`.
  - file must not contain:
    - concrete compiled registry;
    - socket/MTL implementation bodies.

- [ ] R044: Refactor `libs/st2110core/include/st2110/contracts/audio/rx_audio_session_config.hpp`
  - file responsibility:
    - typed backend-facing/manual audio session config only.
  - after refactor file must answer only for:
    - session config shape consumed by backend/bootstrap layers.
  - move/refactor from:
    - the audio runtime/session-config portion of `libs/st2110core/include/st2110/rx_config.hpp`.
  - file must not contain:
    - RTP packet parsing;
    - audio assembly logic;
    - storage conversion logic.

- [ ] R045: Refactor `libs/st2110core/include/st2110/contracts/audio/audio_signaling_rx_config.hpp`
  - file responsibility:
    - typed projection boundary from audio signaling to runtime audio session config only.
  - after refactor file must answer only for:
    - audio signaling -> typed runtime/session config composition.
  - file must not contain:
    - raw SDP parsing;
    - audio frame assembly;
    - backend runtime code.

- [ ] R046: Refactor `libs/st2110core/include/st2110/contracts/audio/audio_receiver_bootstrap.hpp`
  - file responsibility:
    - explicit composition boundary from typed audio signaling + receiver policy inputs into backend-facing/bootstrap carriers only.
  - after refactor file must answer only for:
    - audio bootstrap assembly and cross-consistency.
  - file must not contain:
    - raw SDP parsing;
    - audio reorder/assembler/timestamp runtime logic.

- [ ] R047: Refactor/delete `libs/st2110core/include/st2110/rx_config.hpp`
  - file responsibility:
    - temporary compatibility shim only while video/audio session config is split.
  - move/refactor out of this file:
    - video session config -> `contracts/video/rx_video_session_config.hpp`;
    - audio session config -> `contracts/audio/rx_audio_session_config.hpp`.
  - after refactor file must not remain as a mixed video/audio/runtime dump.
  - if after introducing the split session-config files there are no remaining declarations uniquely owned by `libs/st2110core/include/st2110/rx_config.hpp` and no production includes still require it, delete `libs/st2110core/include/st2110/rx_config.hpp` in this same task.
  - do not keep `rx_config.hpp` as a compatibility wrapper once the split contracts are in place.

- [ ] R048: Refactor/delete `libs/st2110core/include/st2110/video_backend_support.hpp`
  - file responsibility:
    - temporary compatibility shim only while video backend selection/support contract is isolated.
  - move/refactor out of this file:
    - backend-selection vocabulary -> `contracts/video/video_backend_selection.hpp`;
    - backend-specific concrete support logic -> exact backend-local files.
  - after refactor file must not be a cross-block support/validation bucket.
  - if after this redistribution no declarations/helpers remain that are still uniquely owned by this file and no production code still includes it for real content, delete `libs/st2110core/include/st2110/video_backend_support.hpp` in this same task.
  - do not leave this file as a legacy cross-block support helper once contract-level and backend-local responsibilities are separated.

- [ ] R049: Refactor/delete `libs/st2110core/include/st2110/video_receiver_timing.hpp`
  - file responsibility:
    - temporary compatibility shim only while receiver timing contract placement is stabilized.
  - move/refactor out of this file:
    - contract/bootstrap-facing timing capability/config composition -> `contracts/video/video_receiver_bootstrap.hpp`;
    - runtime playout timing behavior -> `receive/video/video_playout_timing.hpp`.
  - after refactor file must not mix:
    - bootstrap contract;
    - runtime playout behavior;
    - signaling consistency logic.
  - if after this split no declarations/helpers remain that are still uniquely owned by this file and no production code still includes it for real content, delete `libs/st2110core/include/st2110/video_receiver_timing.hpp` in this same task.
  - do not keep this file as an extra timing bucket once bootstrap-facing and runtime-facing timing responsibilities are fully separated.

- [ ] R050: Refactor/delete `libs/st2110core/include/st2110/video_receiver_timing_signaling.hpp`
  - file responsibility:
    - temporary compatibility shim only while receiver-timing/signaling consistency logic is moved.
  - move/refactor out of this file:
    - typed bootstrap/signaling consistency logic -> `contracts/video/video_receiver_bootstrap.hpp`.
  - after refactor file must not remain as a side-channel validator detached from the bootstrap contract.
  - if after this move no declarations/helpers remain that are still uniquely owned by this file and no production code still includes it for real content, delete `libs/st2110core/include/st2110/video_receiver_timing_signaling.hpp` in this same task.
  - do not keep this file as a parallel validation path once the consistency logic is owned by the bootstrap contract boundary.

---

## Track R5 — Common receive processing block refactor

> Это блок рефакторинга responsibility block `common receive processing`.
>
> Ответственность блока:
> - backend-agnostic receive processing;
> - packet admission after generic parsing;
> - reorder/assembly/reconstruction/timestamp/playout boundaries;
> - common receive semantics and common receive config carriers.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - знать socket syscalls;
> - знать MTL handles/API;
> - знать OBS handoff;
> - сужать standard model до текущего backend support.

### Shared receive processing

- [ ] R051: Refactor `libs/st2110core/include/st2110/receive/shared/packet_admission.hpp`
  - file responsibility:
    - stream-specific packet admission after successful raw parsing only.
  - after refactor file must answer only for:
    - payload-type and similar admission decisions at receive-processing layer.
  - file must not contain:
    - RTP parsing;
    - backend loop code;
    - depacketizer mutation.

- [ ] R052: Refactor `libs/st2110core/include/st2110/receive/shared/reorder_buffer.hpp`
  - file responsibility:
    - abstract/common reorder-buffer contract only.
  - after refactor file must answer only for:
    - generic reorder-buffer API shape.
  - file must not contain:
    - media-specific packet storage;
    - socket runtime behavior.

- [ ] R053: Refactor `libs/st2110core/include/st2110/receive/shared/fixed_reorder_buffer.hpp`
  - file responsibility:
    - common fixed-window reorder implementation only.
  - after refactor file must answer only for:
    - fixed reorder behavior by sequence number.
  - file must not contain:
    - jitter-buffer behavior;
    - backend receive-loop logic;
    - video/audio-specific decode decisions.

- [ ] R054: Refactor `libs/st2110core/include/st2110/receive/shared/receive_reorder_tolerance_policy.hpp`
  - file responsibility:
    - modeled common reorder flush/tolerance policy only.
  - after refactor file must answer only for:
    - explicit gap/flush/wait tolerance policy shape.
  - move/refactor from:
    - hidden stall/flush behavior currently implicit in backend receive paths.
  - file must not contain:
    - concrete socket thread logic.

### Video receive processing

- [ ] R055: Refactor `libs/st2110core/include/st2110/receive/video/video_receive_description.hpp`
  - file responsibility:
    - common typed description of what video receive session should accept only.
  - after refactor file must answer only for:
    - receive/session capability model separate from delivery/storage compatibility.
  - move/refactor from:
    - the receive/session capability part of `libs/st2110core/include/st2110/video_receive_capability.hpp`.
  - file must not contain:
    - `PixelFormat`/`VideoFrameView` narrowing;
    - backend-local support matrix;
    - MTL projection details.

- [ ] R056: Refactor `libs/st2110core/include/st2110/receive/video/video_timestamp_mapping.hpp`
  - file responsibility:
    - common video RTP timestamp -> internal timestamp mapping only.
  - after refactor file must answer only for:
    - mapping semantics and invariants.
  - file must not contain:
    - backend loop anchoring defaults;
    - playout delay scheduling;
    - sink delivery behavior.

- [ ] R057: Refactor `libs/st2110core/include/st2110/receive/video/video_playout_timing.hpp`
  - file responsibility:
    - common receiver-side video playout/release timing boundary only.
  - after refactor file must answer only for:
    - playout/release timing decisions above reconstructed units.
  - file must not contain:
    - RTP parsing;
    - depacketizer byte placement;
    - OBS-specific timestamp handoff.

- [ ] R058: Refactor `libs/st2110core/include/st2110/receive/video/video_reorder_policy.hpp`
  - file responsibility:
    - video-specific reorder/runtime tolerance policy only.
  - after refactor file must answer only for:
    - video receive reorder policy choices above common reorder primitives.
  - file must not contain:
    - socket receive-loop code;
    - assembly mutation logic.

- [ ] R059: Refactor `libs/st2110core/include/st2110/receive/video/frame_write_coverage.hpp`
  - file responsibility:
    - local tracking of written video-unit/frame coverage only.
  - after refactor file must answer only for:
    - write-coverage accounting.
  - file must not contain:
    - packet parsing;
    - scan-mode policy selection;
    - backend delivery logic.

- [ ] R060: Refactor `libs/st2110core/include/st2110/receive/video/frame_assembler.hpp`
  - file responsibility:
    - byte-oriented frame/unit assembly storage mutation only.
  - after refactor file must answer only for:
    - bounded writes into local assembled storage.
  - file must not contain:
    - ST2110 packet semantics;
    - scan-mode grouping;
    - backend runtime concerns.

- [ ] R061: Refactor `libs/st2110core/include/st2110/receive/video/video_receive_semantics.hpp`
  - file responsibility:
    - modeled receive semantics and completion/grouping policy only.
  - after refactor file must answer only for:
    - unit/grouping/completion semantics by modeled axes.
  - file must not contain:
    - byte writes;
    - socket/MTL runtime code.

- [ ] R062: Refactor `libs/st2110core/include/st2110/receive/video/video_segment_constraints.hpp`
  - file responsibility:
    - format/mode-aware segment constraints only.
  - after refactor file must answer only for:
    - segment-level structural/runtime constraints that belong above raw parsing.
  - file must not contain:
    - frame write mutation;
    - backend support policy.

- [ ] R063: Refactor `libs/st2110core/include/st2110/receive/video/video_segment_placement.hpp`
  - file responsibility:
    - explicit mapping from packet segment semantics to local write operations only.
  - after refactor file must answer only for:
    - placement logic by modeled axes.
  - explicit requirement:
    - this file is a valid place for per-format/per-mode concrete logic branches;
    - temporary `Unsupported` is acceptable only inside exact branch-local logic whose body is still missing.
  - file must not contain:
    - raw parser code;
    - backend runtime code.

- [ ] R064: Refactor `libs/st2110core/include/st2110/receive/video/video_packet_padding.hpp`
  - file responsibility:
    - padding semantics validation/handling only.
  - after refactor file must answer only for:
    - mode-aware/packing-aware trailing padding behavior.
  - file must not contain:
    - segment placement;
    - backend delivery logic.

- [ ] R065: Refactor `libs/st2110core/include/st2110/receive/video/depacketizer.hpp`
  - file responsibility:
    - packet-to-video-unit assembly orchestration only.
  - after refactor file must answer only for:
    - consuming parsed/admitted packets and producing assembled units.
  - file must not contain:
    - raw RTP parsing;
    - socket receive loop;
    - `VideoFrameView`/OBS delivery concerns.

- [ ] R066: Refactor `libs/st2110core/include/st2110/receive/video/video_unit_reconstructor.hpp`
  - file responsibility:
    - transform assembled generic video units into reconstructed project-local frames only.
  - after refactor file must answer only for:
    - reconstruction layer above depacketizer.
  - file must not contain:
    - packet parsing;
    - socket/MTL runtime;
    - OBS handoff.

- [ ] R067: Refactor `libs/st2110core/include/st2110/receive/video/video_receive_pipeline.hpp`
  - file responsibility:
    - common composition of video receive-processing layers only.
  - after refactor file must answer only for:
    - depacketizer + reconstructor + common timing/release composition.
  - file must not contain:
    - UDP receive loop;
    - socket port lifecycle;
    - MTL frame API handling.

### Audio receive processing

- [ ] R068: Refactor `libs/st2110core/include/st2110/receive/audio/audio_packet.hpp`
  - file responsibility:
    - common typed audio RTP packet interpretation after generic RTP parsing only.
  - after refactor file must answer only for:
    - audio RTP packet model and packet-local interpretation.
  - file must not contain:
    - socket receive loop;
    - audio reorder runtime;
    - audio buffer storage conversion policy beyond packet-local decode needs.

- [ ] R069: Refactor `libs/st2110core/include/st2110/receive/audio/audio_reorder_buffer.hpp`
  - file responsibility:
    - audio-specific reorder storage/behavior only.
  - after refactor file must answer only for:
    - audio RTP reorder behavior.
  - file must not contain:
    - jitter-buffer/planned playout release;
    - socket runtime code;
    - audio frame delivery.

- [ ] R070: Refactor `libs/st2110core/include/st2110/receive/audio/audio_frame_assembler.hpp`
  - file responsibility:
    - audio packet -> project-local audio block assembly only.
  - after refactor file must answer only for:
    - assembling current audio block representation from admitted/reordered audio packets.
  - file must not contain:
    - raw SDP/runtime config projection;
    - backend thread logic;
    - OBS handoff.

- [ ] R071: Refactor `libs/st2110core/include/st2110/receive/audio/audio_timestamp_mapping.hpp`
  - file responsibility:
    - common audio RTP timestamp -> internal timestamp mapping and audio playout-timing boundary only.
  - after refactor file must answer only for:
    - timestamp mapping and playout timing semantics.
  - file must not contain:
    - socket receive loop;
    - MTL frame handling;
    - audio output API coupling.

- [ ] R072: Refactor `libs/st2110core/include/st2110/receive/audio/audio_stats.hpp`
  - file responsibility:
    - common audio receive stats carriers/helpers only.
  - after refactor file must answer only for:
    - common audio receive-processing observability.
  - file must not contain:
    - socket runtime counters;
    - MTL device/session counters;
    - OBS runtime counters.

---

## Track R6 — Socket platform adapters block refactor

> Это блок рефакторинга responsibility block `socket platform adapters`.
>
> Ответственность блока:
> - OS/runtime transport details only;
> - no media logic beyond transport contract needs.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - знать SDP/media standards logic beyond transport contract inputs;
> - знать depacketizer/assembler behavior;
> - знать OBS or MTL concerns.

- [ ] R073: Refactor `libs/st2110core/include/st2110/backends/socket/platform/socket_runtime.hpp`
  - file responsibility:
    - OS-neutral socket transport types/contracts only.
  - after refactor file must answer only for:
    - socket address family;
    - bind endpoint;
    - multicast membership carrier;
    - port open/receive/close contract.
  - file must not contain:
    - packet parsing;
    - backend lifecycle/state;
    - video/audio pipeline composition.

- [ ] R074: Refactor `libs/st2110core/include/st2110/backends/socket/platform/socket_stub_rx_port.hpp`
  - file responsibility:
    - temporary contract-conforming stub transport implementation only.
  - after refactor file must answer only for:
    - explicit stub/test transport behavior.
  - file must not contain:
    - hidden backend policy assembly;
    - production runtime defaults.

- [ ] R075: Refactor `libs/st2110core/include/st2110/backends/socket/platform/linux_socket_rx_port.hpp`
  - file responsibility:
    - Linux socket receive-port implementation only.
  - after refactor file must answer only for:
    - Linux open/bind/multicast join/receive/close through the OS-neutral socket contract.
  - file must not contain:
    - packet parsing;
    - media kind branching;
    - sink delivery.

- [ ] R076: Refactor `libs/st2110core/include/st2110/backends/socket/platform/windows_socket_rx_port.hpp`
  - file responsibility:
    - Windows socket receive-port implementation placeholder/contract only.
  - after refactor file must answer only for:
    - Windows transport adaptation boundary and nothing else.
  - file must not contain:
    - platform-independent backend policy;
    - media parsing;
    - OBS/plugin logic.

---

## Track R7 — Socket backend block refactor

> Это блок рефакторинга responsibility block `socket backend`.
>
> Ответственность блока:
> - consume lower/common contracts;
> - own socket-backend runtime composition only;
> - not redefine media model or transport contracts.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - собирать hidden defaults;
> - заново валидировать internal enums;
> - знать sibling block internals instead of public boundaries.

- [ ] R077: Refactor `libs/st2110core/include/st2110/backends/socket/socket_rx_single_media_backend_base.hpp`
  - file responsibility:
    - generic socket single-media backend runtime base only.
  - after refactor file must answer only for:
    - socket port lifecycle;
    - receive thread;
    - generic datagram accounting;
    - generic RTCP/datagram classification helpers.
  - file must not contain:
    - video/audio operational config assembly;
    - media-specific parser/depacketizer policy construction.

- [ ] R078: Refactor `libs/st2110core/src/backends/socket/socket_rx_single_media_backend_base.cpp`
  - file responsibility:
    - implementation of the media-agnostic runtime base only.
  - after refactor file must answer only for:
    - thread/port/datagram loop mechanics shared by socket media backends.
  - file must not contain:
    - video packet pipeline code;
    - audio packet pipeline code;
    - backend selection/build logic.

- [ ] R079: Refactor `libs/st2110core/include/st2110/backends/socket/socket_rx_video_backend.hpp`
  - file responsibility:
    - concrete socket video backend composition only.
  - after refactor file must answer only for:
    - consuming prebuilt operational video config;
    - wiring parsed/admitted datagrams into the common video receive pipeline;
    - localized socket-video runtime state.
  - file must not:
    - rebuild hidden open/parse/timestamp defaults;
    - own standard video capability truth;
    - own delivery-conversion truth.

- [ ] R080: Refactor `libs/st2110core/include/st2110/backends/socket/socket_rx_audio_backend.hpp`
  - file responsibility:
    - concrete socket audio backend composition only.
  - after refactor file must answer only for:
    - consuming prebuilt operational audio config;
    - wiring parsed/admitted datagrams into common audio receive processing;
    - localized socket-audio runtime state.
  - file must not:
    - rebuild hidden packet/reorder/assembler/timestamp defaults;
    - collapse common audio capability to current Level-A path outside explicit support branches.

---

## Track R8 — MTL backend block refactor

> Это блок рефакторинга responsibility block `MTL backend`.
>
> Ответственность блока:
> - wrap relevant MTL APIs through backend-local logic;
> - consume the common model instead of replacing it;
> - keep MTL device/session projection and frame mapping localized.
>
> После рефакторинга блок должен отвечать **только** за это.
> При выборе `Mtl` backend пользователь должен иметь путь ко всему in-scope ST2110 behavior, которое реально expressible/implementable через выбранный MTL API surface.
> Если проект не может дотянуться до MTL capability из-за узкой project model / projection target / delivery model — это incompleteness of project, а не acceptable narrowing.

- [ ] R081: Refactor `libs/st2110core/include/st2110/backends/mtl/mtl_rx_backend_factory.hpp`
  - file responsibility:
    - MTL backend factory declarations only.
  - after refactor file must answer only for:
    - factory contract/descriptor for MTL backends.
  - file must not contain:
    - device/session projection;
    - backend runtime logic;
    - fake unavailable-path policy except if an explicit temporary test fixture is still needed elsewhere.

- [ ] R082: Refactor `libs/st2110core/src/backends/mtl/mtl_rx_backend_factory.cpp`
  - file responsibility:
    - MTL backend factory implementation only.
  - after refactor file must answer only for:
    - creating concrete MTL backends and exposing truthful descriptors.
  - file must not contain:
    - support matrix logic that belongs in `mtl_video_support.hpp`;
    - session projection logic.

- [ ] R083: Refactor `libs/st2110core/include/st2110/backends/mtl/mtl_video_support.hpp`
  - file responsibility:
    - MTL-video-specific support matrix / support-branch dispatch only.
  - after refactor file must answer only for:
    - deciding which common video branches currently have concrete MTL logic.
  - move/refactor from:
    - the support-matrix/support-helper part of `mtl_rx_video_backend.hpp/.cpp`.
  - explicit rule:
    - this file must not use `Unsupported` because “projection target is too narrow”;
    - it may report `Unsupported` only from exact concrete logic branches whose bodies are not implemented yet.

- [ ] R084: Refactor `libs/st2110core/include/st2110/backends/mtl/mtl_video_projection.hpp`
  - file responsibility:
    - typed projection from common video config + MTL runtime config into MTL ST20P structures only.
  - after refactor file must answer only for:
    - complete typed projection into `st20p_rx_ops`-facing data.
  - move/refactor from:
    - the projection/helper portion of `mtl_rx_video_backend.hpp/.cpp`.
  - explicit rule:
    - if a recognized value cannot be represented here, the model is incomplete and must be completed;
    - this file must not hide incompleteness behind helper-level `Unsupported`.

- [ ] R085: Refactor `libs/st2110core/include/st2110/backends/mtl/mtl_rx_video_backend.hpp`
  - file responsibility:
    - concrete MTL video backend declaration only.
  - after refactor file must answer only for:
    - backend-local state/resource ownership surface and public backend contract conformance.
  - file must not contain:
    - full support matrix bodies;
    - projection helper bodies;
    - common media-model truth.

- [ ] R086: Refactor `libs/st2110core/src/backends/mtl/mtl_rx_video_backend.cpp`
  - file responsibility:
    - concrete MTL video backend implementation only.
  - after refactor file must answer only for:
    - MTL device/session lifecycle;
    - receive-loop / blocking frame get-put flow;
    - localized frame mapping and stats collection by calling lower explicit boundaries.
  - file must consume:
    - `mtl_video_support.hpp`;
    - `mtl_video_projection.hpp`;
    - common delivery/timestamp boundaries.
  - file must not:
    - re-own the support matrix;
    - hide conversion/delivery limits as session-start failure;
    - narrow MTL below relevant in-scope MTL capability because Socket or current handoff is narrower.

---

## Track R9 — OBS plugin composition block refactor

> Это блок рефакторинга responsibility block `OBS plugin composition`.
>
> Ответственность блока:
> - top composition of OBS/plugin-facing behavior only;
> - OBS source/module/property/runtime/handoff wiring through public backend contracts.
>
> После рефакторинга блок должен отвечать **только** за это.
> Блок не должен:
> - знать packet parsing, depacketizing, socket syscalls, MTL session internals;
> - превращаться в место backend/media logic duplication.

### OBS plugin public include surface

- [ ] R087: Refactor `plugins/obs_st2110/include/obs_st2110/plugin_api.hpp`
  - file responsibility:
    - small plugin-wide public include/API surface only.
  - after refactor file must answer only for:
    - plugin-level declarations that truly need cross-file visibility in OBS layer.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not expose:
    - backend internals;
    - socket/MTL handles;
    - receive-pipeline types.

- [ ] R088: Refactor `plugins/obs_st2110/include/obs_st2110/source_config.hpp`
  - file responsibility:
    - typed OBS source configuration model only.
  - after refactor file must answer only for:
    - source settings/state shape derived from OBS properties and persisted settings.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - backend start/stop code;
    - OBS source runtime/thread ownership.

- [ ] R089: Refactor `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp`
  - file responsibility:
    - typed OBS source runtime state surface only.
  - after refactor file must answer only for:
    - runtime state owned by one source instance.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - source property building;
    - backend-wiring logic bodies;
    - raw media handoff helpers.

### OBS plugin implementation files

- [ ] R090: Refactor `plugins/obs_st2110/src/plugin_entry.cpp`
  - file responsibility:
    - OBS module entrypoints and plugin-global registration orchestration only.
  - after refactor file must answer only for:
    - `obs_module_*` entrypoints and module-level load/unload wiring.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly using the DistroAV-inspired module split.
  - file must not contain:
    - source property logic;
    - per-source runtime behavior;
    - backend-specific media logic.

- [ ] R091: Refactor `plugins/obs_st2110/src/source_registration.cpp`
  - file responsibility:
    - `obs_source_info` descriptor creation/registration only.
  - after refactor file must answer only for:
    - source descriptor and callback table wiring.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - backend start/stop bodies;
    - media handoff implementation.

- [ ] R092: Refactor `plugins/obs_st2110/src/source_settings_ui.cpp`
  - file responsibility:
    - OBS source properties/defaults/update UI logic only.
  - after refactor file must answer only for:
    - property list/defaults/visibility/dependency handling.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - socket/MTL runtime behavior;
    - frame/audio handoff logic.

- [ ] R093: Refactor `plugins/obs_st2110/src/source_runtime.cpp`
  - file responsibility:
    - one-source runtime lifecycle orchestration only.
  - after refactor file must answer only for:
    - start/stop/reconfigure/join/reset behavior for one OBS source instance.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - backend operational config construction beyond calling the dedicated wiring boundary;
    - media handoff conversion bodies.

- [ ] R094: Refactor `plugins/obs_st2110/src/backend_wiring.cpp`
  - file responsibility:
    - plugin-to-backend operational wiring only.
  - after refactor file must answer only for:
    - mapping OBS source config/runtime selection into public backend/bootstrap contracts.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - source properties UI;
    - socket/MTL internal implementation details;
    - OBS handoff bodies.

- [ ] R095: Refactor `plugins/obs_st2110/src/obs_video_handoff.cpp`
  - file responsibility:
    - localized video handoff from project delivery contract into OBS only.
  - after refactor file must answer only for:
    - `VideoFrameView` / project-local video handoff -> `obs_source_output_video(...)`.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - backend receive loop;
    - packet parsing/depacketizer logic.

- [ ] R096: Refactor `plugins/obs_st2110/src/obs_audio_handoff.cpp`
  - file responsibility:
    - localized audio handoff from project delivery contract into OBS only.
  - after refactor file must answer only for:
    - `AudioFrameView` / project-local audio handoff -> `obs_source_output_audio(...)`.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly.
  - file must not contain:
    - backend receive loop;
    - RTP/audio assembly logic.

---

## Track R10 — Support / build / entrypoints block refactor

> Это блок рефакторинга responsibility block `support/build and entrypoints`.
>
> Ответственность блока:
> - build wiring;
> - compiled entrypoint composition;
> - repository-local support shims/scripts only.
>
> После рефакторинга блок должен отвечать **только** за это.
> Этот блок intentionally идет последним:
> - он должен фиксировать уже стабилизированные file/block boundaries;
> - он не должен диктовать архитектуру нижних блоков.

- [ ] R097: Refactor `libs/st2110core/src/backend_factory_registry.cpp`
  - file responsibility:
    - compiled backend-factory set composition and registry implementation only.
  - after refactor file must answer only for:
    - wiring the actually compiled factory set into the public registry API.
  - move/refactor from:
    - current mixed registry implementation in this file.
  - explicit cleanup:
    - keep only compiled-factory composition;
    - move selection/support semantics to contract/backend-selection files and backend-local support files.
  - file must not contain:
    - backend support policy logic;
    - platform-specific application branching;
    - app/OBS logic.

- [ ] R098: Refactor/delete `libs/st2110core/src/stub.cpp`
  - file responsibility:
    - temporary build/link compatibility translation unit only, if still required.
  - after refactor file must answer only for:
    - nothing, if repository no longer needs a stub TU.
  - if after the preceding refactoring tasks the build no longer needs this translation unit for any real linkage/build role, delete `libs/st2110core/src/stub.cpp` in this same task.
  - file must not remain:
    - as a hidden placeholder for missing real architecture;
    - as a sink for mixed temporary logic;
    - as an inert always-built compatibility TU with no remaining responsibility.

- [ ] R099: Refactor `libs/st2110core/CMakeLists.txt`
  - file responsibility:
    - build wiring for `st2110core` only.
  - after refactor file must answer only for:
    - source membership;
    - include paths;
    - target link dependencies;
    - platform/MTL build capability localization.
  - file must not encode:
    - architecture decisions that belong in code;
    - vendoring/superbuild logic for external heavy dependencies.

- [ ] R100: Refactor `plugins/obs_st2110/CMakeLists.txt`
  - file responsibility:
    - OBS plugin target build wiring only.
  - after refactor file must answer only for:
    - plugin module target structure and dependencies.
  - move/refactor from:
    - no current production logic exists yet; create the boundary explicitly when the plugin target is added.
  - file must not contain:
    - runtime/backend logic;
    - UI/business logic.

- [ ] R101: Refactor `scripts/build_and_test.sh`
  - file responsibility:
    - developer convenience build/test orchestration only.
  - after refactor file must answer only for:
    - local build/test flow for this repository.
  - file must not become:
    - a clean-machine installer;
    - a hidden source of product/runtime defaults.

---
## Exit condition for Phase R

- every listed file belongs clearly to exactly one responsibility block;
- no leftover mixed “bucket” headers remain;
- no generic enum-validation theater remains over closed internal enums;
- `Unsupported` remains only as missing concrete logic inside exact concrete logic branches;
- common modeled axes are no longer narrowed by Socket MVP limits or current delivery limits;
- socket/MTL/OBS blocks consume lower/common contracts instead of importing sibling internals;
- extending support for a new mode/format/backend path is mostly reduced to filling the already-existing concrete logic branch in the correct file.
- all legacy compatibility files from the previous architecture are either:
  - fully redistributed and deleted;
  - or still present only when they retain one explicit remaining responsibility documented by the corresponding refactor task;
- no empty compatibility headers / forwarding shims / inert stub translation units remain in the production tree without explicit justified responsibility.

> Уточнение после `Phase R — Responsibility-block refactoring`:
> - задачи этого трека выполняются уже поверх refactored responsibility blocks;
> - они должны использовать новые block boundaries и новые файлы из `backends/mtl`, `contracts/*`, `receive/*`, `delivery/*`, `plugins/obs_st2110/*`;
> - они не должны возвращать проект к legacy bucket-файлам или reintroduce mixed-responsibility helpers;
> - `Unsupported` в remaining MTL tasks допустим только как отсутствие concrete logic inside exact MTL-local branch that is supposed to implement a recognized modeled value;
> - inability to represent a recognized value in a projection target or delivery target must be treated as project incompleteness and fixed by completing the corresponding model/boundary.

### C3. MTL video RX
- [x] 130: Add `ST2110_WITH_MTL` build option + localized MTL dependency/build guard
  - keep the existing public backend model unchanged:
    - `RxBackendKind::Mtl` stays in the public backend-kind axis;
    - existing backend lifecycle/state/stats/factory contracts stay unchanged;
  - when `ST2110_WITH_MTL=OFF`, the project must build cleanly without MTL headers/libs and without compiling MTL backend code;
  - when `ST2110_WITH_MTL=ON`, MTL dependency wiring must stay localized to build/factory/runtime code rather than leaking into app/bootstrap code;
  - keep temporary build/runtime unavailability localized through factory/build selection, not by removing `mtl` from public parsing/selection.
- [x] 130A: Replace `ST2110_WITH_MTL` with a platform-derived MTL backend build boundary
  - remove `ST2110_WITH_MTL` as a user-facing CMake option;
  - introduce an internal build capability such as `ST2110_HAS_MTL_BACKEND`;
  - derive `ST2110_HAS_MTL_BACKEND` from the target platform:
    - Linux => MTL backend is built and required;
    - Windows / unsupported platforms => MTL backend is not built;
  - keep `RxBackendKind::Mtl` in the public backend-kind model even when the backend is not built on a platform;
  - keep the future Windows port limited to the project socket backend unless MTL Windows support is explicitly re-evaluated later;
  - do not treat MTL as an optional Linux product feature;
  - do not leak MTL platform/build decisions into parser, signaling, app, or OBS source code.
- [x] 130B: Rewire `libs/st2110core/CMakeLists.txt` to consume installed MTL through `pkg-config`
  - make Linux `st2110core` builds require an already installed MTL package;
  - discover MTL through `pkg-config` package `mtl`;
  - link MTL through an imported pkg-config target, for example `PkgConfig::ST2110_MTL`;
  - remove manual `ST2110_MTL_INCLUDE_DIRS` / `ST2110_MTL_LIBRARIES` as the primary integration path;
  - do not vendor or superbuild DPDK / Media Transport Library from this project CMake;
  - keep dependency installation/building as responsibility of a future external setup/install script;
  - ensure CMake configure fails early on Linux when MTL is required but not installed;
  - ensure non-MTL platforms do not require MTL headers, MTL libraries, DPDK, or pkg-config `mtl`.
- [x] 130C: Remove the unavailable MTL factory build path from normal backend architecture
  - stop compiling `src/mtl_rx_backend_factory_unavailable.cpp`;
  - delete `src/mtl_rx_backend_factory_unavailable.cpp` if it has no remaining repository role;
  - do not register unavailable MTL factories on platforms where MTL is not built;
  - on unsupported platforms, represent MTL absence by absence from the compiled factory registry and by `rx_backend_kind_built(RxBackendKind::Mtl) == false`;
  - keep unavailable/null factory behavior only as an explicit test fixture if tests still need to exercise unavailable backend selection;
  - avoid showing an unavailable MTL backend as a normal user-selectable plugin backend on Windows / non-MTL platforms.
- [x] 130D: Rework `backend_factory_registry.cpp` around the actual compiled backend factory set
  - include and instantiate MTL factories only when `ST2110_HAS_MTL_BACKEND` is enabled;
  - keep socket video/audio factories always available for supported project builds;
  - make the built-in factory array size match the compiled factory set instead of hardcoding a four-factory list on every platform;
  - update `rx_backend_kind_built(...)` to use `ST2110_HAS_MTL_BACKEND` rather than `ST2110_WITH_MTL`;
  - keep backend selection backend-kind-aware and media-aware without platform-specific branching in app/bootstrap/OBS code;
  - ensure Linux builds expose both socket and MTL factories;
  - ensure Windows / non-MTL builds expose only socket factories.
- [x] 130E: Align MTL factory descriptors with real compiled/runtime availability
  - in the real MTL factory implementation, do not use `available = false` as a placeholder once the corresponding backend runtime is implemented;
  - keep `MtlRxVideoBackendFactory` compiled only on MTL-capable builds;
  - make video MTL factory availability reflect the implemented video runtime state after task `132`;
  - keep audio MTL factory availability false or absent until `MtlRxAudioBackend` is implemented through tasks `140–142`;
  - avoid returning `nullptr` from a factory whose descriptor says it is available;
  - keep descriptor capability flags accurate:
    - video MTL factory => video receive only;
    - audio MTL factory => audio receive only after audio backend implementation.
- [x] 130F: Update `scripts/build_and_test.sh` for the new local Linux MTL-required development path
  - remove any `ST2110_WITH_MTL` argument or expectation from the local test script;
  - keep the script as a developer convenience only, not as final plugin packaging logic;
  - set or preserve `PKG_CONFIG_PATH` entries needed for local `/usr/local` MTL/DPDK installs;
  - fail early with a clear message if `pkg-config --exists mtl` fails on Linux;
  - configure the project with ordinary CMake/Ninja commands and no MTL feature toggle;
  - build and run existing tests through CTest;
  - keep future Windows test/build scripting separate from this Linux MTL-required script.
- [x] 130G: Document the new MTL build/dependency policy in MTL project context docs
  - update `docs/mtl_runtime_context.md` to state that:
    - Linux builds consume an externally installed MTL;
    - DPDK/MTL installation is outside project CMake;
    - future installer/setup scripts may build/install DPDK and MTL before invoking this project build;
    - Linux plugin builds are expected to include both socket and MTL backends;
    - Windows builds are socket-only unless MTL support is explicitly re-evaluated later.
  - update `docs/mtl_context_index.md` if needed so MTL build/dependency policy is discoverable from the MTL context entry point;
  - keep this documentation as MTL-specific context, not as a new global spec/deviation note;
  - do not change ST 2110 signaling/runtime architecture rules while documenting this build policy.
- [x] 131: Implement `MtlRxVideoBackend` skeleton
  - reuse existing backend lifecycle/state/stats/factory contracts;
  - do not introduce a parallel backend API;
  - keep MTL device/session ownership explicit inside the backend;
  - keep MTL-specific lifecycle, MTL device/runtime projection, frame mapping, and stats boundaries localized in the backend/runtime layer;
  - do not encode project-local video format, scan-mode, packing-mode, or `VideoFrameView` limitations as MTL backend capability limitations;
  - leave the common video capability-model expansion, backend-support validation, ST20P projection, runtime start/stop, frame delivery, and stats behavior to the follow-up MTL tasks below.
- [x] 131A: Expand the common ST 2110 video receive capability model beyond current Socket MVP limits
  - extend existing common video enums / structs / helpers so the project can represent the standard and MTL-known video receive space independently of whether the Socket backend has implemented each branch;
  - keep the capability model common to Socket and MTL rather than introducing an MTL-only media model for formats/modes;
  - cover common video receive axes such as:
    - scan mode: progressive, interlaced, PsF;
    - packing mode: GPM, BPM, and future/recognized packing branches where modeled;
    - signaled sampling variants already represented by the SDP/signaling model;
    - bit depths including integer and floating-point depth representation already modeled at signaling level;
    - transport/payload format needed for backend projection;
    - output/storage/handoff format as a separate axis from signaled sampling and MTL transport format;
    - frame-rate / RTP clock / timestamp policy axes already modeled elsewhere;
    - one-stream and redundant-stream/topology hints where they affect backend construction.
  - do not collapse common capability into `PixelFormat::UYVY`, `VideoScanMode::Progressive`, or `VideoPackingMode::Gpm`;
  - do not make Socket implementation limits define which formats/modes are structurally recognized by the project;
  - completion condition:
    - common video receive config/validation can represent standard/MTL-known video modes before backend-specific implementation support is checked.
- [x] 131B: Split common video structural validation from backend implementation-support validation
  - keep structural validation responsible for determining whether a video receive description is well-formed, standards-aware, and internally consistent;
  - add a separate backend-support validation boundary that answers whether a specific backend currently implements that already-recognized mode;
  - ensure failure semantics are explicit:
    - malformed or contradictory stream/config values => `InvalidValue`;
    - recognized but not implemented by the selected backend => `Unsupported`;
    - runtime/system/MTL/socket failure => backend/runtime error, not config invalidity.
  - Socket must reject not-yet-implemented common video modes through Socket implementation-support validation, not by treating them as unknown or invalid;
  - MTL must accept/project common video modes that MTL can implement, even when Socket cannot;
  - completion condition:
    - the same structurally valid video config can pass common validation, fail Socket support validation, and pass MTL support/projection validation where MTL supports it.
- [x] 131C: Add a common video backend support matrix/policy for Socket and MTL
  - introduce explicit backend-specific support policy helpers for common video receive capabilities;
  - define Socket support in terms of actual project Socket implementation status, not in terms of whether a format/mode is standard-valid;
  - define MTL support in terms of whether:
    - the common model already represents the branch;
    - the MTL-facing typed boundary already represents the branch cleanly;
    - and the concrete MTL-local logic branch for that branch is implemented.
  - keep Socket and MTL policies consuming the same common video capability/config model;
  - avoid separate duplicated validation logic that can diverge between backends;
  - make unsupported branches named and testable:
    - Socket unsupported because the exact Socket concrete logic branch is not implemented yet;
    - MTL unsupported only when the exact MTL-local concrete logic branch is not implemented yet after the common model and MTL-facing typed boundary already represent the branch.
  - inability to represent a recognized value in an MTL projection target or project delivery target is not a valid support-policy outcome and must be treated as project incompleteness.
  - completion condition:
    - backend selection checks common video capability through a shared pipeline and diverges only at backend-local implementation policy / concrete logic branch availability.
- [ ] 131D: Complete the explicit MTL video projection boundary into `st20p_rx_ops`
  - implement the projection in the refactored MTL block through:
    - `libs/st2110core/include/st2110/backends/mtl/mtl_video_projection.hpp`;
    - `libs/st2110core/src/backends/mtl/mtl_rx_video_backend.cpp` where the backend consumes that projection boundary.
  - add a named projection boundary from the common video receive/session model plus MTL device/runtime config into MTL `st20p_rx_ops`;
  - consume the common model from the refactored boundaries, including:
    - `contracts/video/rx_video_session_config.hpp`;
    - `receive/video/video_receive_description.hpp`;
    - relevant timing/delivery boundaries where needed.
  - keep MTL device/runtime fields backend-specific:
    - MTL device ports;
    - session ports `P` / `R`;
    - PMD selection;
    - network protocol;
    - queue counts;
    - MTL flags;
    - lcores / socket / NUMA policy where consumed.
  - project common media axes into MTL ST20P fields through explicit per-axis/per-branch logic:
    - payload type;
    - width / height / frame rate;
    - scan/interlace-related fields;
    - ST20 transport format;
    - ST frame output format;
    - packing/session mode;
    - frame buffer count;
    - receive flags and session policies.
  - projection target must be complete for all in-scope branches the project routes into MTL.
  - do not infer MTL transport/output format only from project-local `PixelFormat`.
  - do not silently coerce unknown or unsupported projection values to defaults.
  - generic projection helpers must not return `Unsupported` merely because the helper does not know how to continue.
  - if some recognized branch still lacks implementation, `Unsupported` is allowed only from the exact branch-local concrete logic that is supposed to populate that part of the MTL projection.
  - if a recognized value cannot be represented in the MTL-facing typed target, treat that as project incompleteness and complete the target model/boundary.
  - completion condition:
    - MTL video start can consume the same common video config validated for Socket/MTL and obtain fully populated `st20p_rx_ops` through the explicit MTL projection boundary.
- [ ] 131E: Keep Socket video backend on the same common video validation path
  - rewire or preserve Socket video operational assembly so it consumes the common video receive capability/config model before Socket-specific runtime construction;
  - ensure Socket limitations are expressed by Socket support policy:
    - not implemented in Socket depacketizer/reconstructor;
    - not implemented in Socket storage/handoff;
    - not implemented in Socket timing/playout;
    - not implemented in Socket packing/scan branch.
  - do not use common structural validation to reject modes merely because Socket lacks implementation;
  - keep existing Socket MVP behavior unchanged for currently implemented modes;
  - completion condition:
    - Socket and MTL run through the same common video structural validation, and Socket-only failures for broader modes are localized as implementation-support failures.
- [ ] 131F: Split project video delivery/handoff compatibility from receive-session capability
  - implement/tighten this split through the refactored boundaries:
    - `libs/st2110core/include/st2110/contracts/video/rx_video_session_config.hpp`;
    - `libs/st2110core/include/st2110/receive/video/video_receive_description.hpp`;
    - `libs/st2110core/include/st2110/delivery/video/video_handoff_format.hpp`;
    - `libs/st2110core/include/st2110/delivery/video/video_frame_conversion.hpp`;
    - `libs/st2110core/include/st2110/delivery/video/video_frame.hpp`.
  - introduce or tighten a common video delivery/handoff support boundary separate from backend receive-session support;
  - this boundary must decide whether an already-received frame/unit can currently be exposed through:
    - `VideoFrameView`;
    - a conversion helper;
    - OBS handoff;
    - a future native frame contract.
  - do not reject MTL session creation or common receive/session config validation only because a particular output frame format cannot yet be represented by the current project-local delivery/storage layer.
  - ensure the signaling/bootstrap -> session-config path preserves/builds structurally complete recognized receive/session capability for known signaling modes without requiring immediate `VideoFrameHandoffFormat -> PixelFormat` collapse.
  - current project handoff/storage limits must fail only at the explicit delivery/handoff/conversion boundary.
  - Socket may still fail earlier if the exact Socket concrete logic branch for depacketizer/storage/handoff is not implemented for a recognized mode.
  - MTL may receive broader MTL-supported modes and then handle project delivery limitations through localized conversion/drop/report policy.
  - completion condition:
    - receive-session capability and project delivery/handoff capability are independent boundaries, and current project-local delivery limits do not narrow the common receive model.
- [ ] 131G: Add regression coverage for common video capability validation across Socket and MTL
  - add tests proving common structural validation accepts recognized video modes beyond current Socket MVP implementation;
  - add tests proving Socket rejects not-yet-implemented recognized modes through Socket support policy, not common structural validation;
  - add tests proving MTL support/projection accepts corresponding MTL-supported modes where projection exists;
  - cover at least:
    - progressive and interlaced/PsF recognized branches;
    - GPM and BPM recognized branches;
    - non-UYVY output/storage branches;
    - 10-bit / 12-bit / 16-bit-related branches where represented;
    - 4:4:4 / 4:2:2 / 4:2:0 recognized branches where represented;
    - one-stream and redundant/topology branches where modeled.
  - completion condition:
    - tests fail if recognized common video modes are incorrectly rejected as invalid or if MTL support is narrowed by Socket implementation limits.
- [ ] 132: Implement full MTL video start/stop using ST20P RX frame API
  - consume the common video receive config after common structural validation and MTL backend-support validation;
  - consume MTL-specific device/runtime config only for MTL device/session construction;
  - use `mtl_init` / `mtl_uninit` for device lifecycle;
  - use `st20p_rx_create` / `st20p_rx_free` for ST20P session lifecycle;
  - project ST20P RX operations through the common-to-MTL projection boundary from task `131D`;
  - use blocking `st20p_rx_get_frame` / `st20p_rx_put_frame` receive flow where configured;
  - use the MTL wake mechanism on stop so blocked receive can terminate cleanly;
  - support MTL-projected video session axes through explicit projection / policy branches rather than hidden project-local defaults;
  - keep start-failure cleanup, retry-after-failure behavior, and stop/shutdown semantics explicit;
  - do not reject MTL session creation only because the current project `VideoFrameView` mapping cannot deliver a particular MTL output format;
  - completion condition:
    - MTL video start/stop constructs and tears down the MTL device/session path from common validated video config plus MTL runtime config, without hardcoded Socket MVP format/mode restrictions.
- [ ] 133: Map MTL video frames through the common delivery boundary and deliver to sink where representable
  - consume frames from `st20p_rx_get_frame(...)` and return them with `st20p_rx_put_frame(...)`;
  - deliver any sink-facing view synchronously within the valid lifetime of the MTL frame;
  - map MTL frame metadata through a dedicated boundary:
    - frame format;
    - plane pointers;
    - linesizes;
    - width / height;
    - interlaced / second-field metadata;
    - frame status;
    - packet counters;
    - RTP timestamp and MTL timestamp metadata.
  - keep `rtp_timestamp` -> `TimestampNs` mapping on the existing timestamp-mapping boundary;
  - use the common delivery/handoff boundary from task `131F` for deciding:
    - direct `VideoFrameView` delivery;
    - conversion before delivery;
    - future/native frame delivery;
    - counted/reportable delivery-unsupported/drop behavior.
  - do not treat broader MTL-supported receive-session values as session-start failures merely because current project delivery is narrower;
  - completion condition:
    - MTL video frame delivery capability is separated from MTL receive-session capability, and unsupported project handoff cases are localized, observable, and non-corrupting.
- [ ] 134: Add meaningful backend-local MTL video stats
  - populate delivery counters and MTL session/device stats that are actually available/meaningful;
  - include frame-level metadata that MTL exposes where useful:
    - delivered frames;
    - delivery-unsupported frames;
    - incomplete/status-marked frames;
    - packet totals / received packet counters where available;
    - receive timestamps / timing metadata where consumed;
    - MTL port/device counters where available.
  - do not force socket packet-parse / reorder / depacketizer stats concepts onto the MTL video backend;
  - keep unavailable MTL counters explicit rather than inventing synthetic values;
  - keep stats collection backend-local and snapshot-oriented;
  - completion condition:
    - MTL video stats describe actual MTL runtime/session/frame behavior and remain valid across broader common video capability branches.

### C4. MTL audio RX
- [ ] 140: Expand the common ST 2110 audio receive capability model beyond current Socket Level-A limits
  - extend existing common audio enums / structs / helpers so the project can represent the standard and MTL-known ST30/ST31 receive space independently of whether the Socket backend has implemented each branch;
  - keep the capability model common to Socket and MTL rather than introducing an MTL-only media model for audio formats/modes;
  - cover common audio receive axes such as:
    - RTP payload type;
    - PCM format / bit depth;
    - ST31/AM824 or other non-current audio payload branches as distinct modeled branches where relevant;
    - sampling rate;
    - packet time;
    - samples per packet as a derived value;
    - channel count;
    - channel order / channel mapping;
    - frame/block duration policy;
    - one-stream and redundant-stream/topology hints where they affect backend construction.
  - do not collapse common audio capability into `48 kHz`, `1 ms`, `1..8 channels`, linear PCM, or current `InterleavedS32` delivery limits;
  - do not make Socket implementation limits define which audio formats/modes are structurally recognized by the project;
  - completion condition:
    - common audio receive config/validation can represent recognized ST30/ST31/MTL-known audio modes before backend-specific implementation support is checked.
- [ ] 140A: Split common audio structural validation from backend implementation-support validation
  - keep structural validation responsible for determining whether an audio receive description is well-formed, standards-aware, and internally consistent;
  - add a separate backend-support validation boundary that answers whether a specific backend currently implements that already-recognized mode;
  - ensure failure semantics are explicit:
    - malformed or contradictory stream/config values => `InvalidValue`;
    - recognized but not implemented by the selected backend => `Unsupported`;
    - runtime/system/MTL/socket failure => backend/runtime error, not config invalidity.
  - Socket must reject not-yet-implemented common audio modes through Socket implementation-support validation, not by treating them as unknown or invalid;
  - MTL must accept/project common audio modes that MTL can implement, even when Socket cannot;
  - completion condition:
    - the same structurally valid audio config can pass common validation, fail Socket support validation, and pass MTL support/projection validation where MTL supports it.
- [ ] 140B: Add a common audio backend support matrix/policy for Socket and MTL
  - introduce explicit backend-specific support policy helpers for common audio receive capabilities;
  - define Socket support in terms of actual project Socket implementation status, not in terms of whether an audio format/mode is standard-valid;
  - define MTL support in terms of whether:
    - the common audio model already represents the branch;
    - the MTL-facing typed boundary already represents the branch cleanly;
    - and the exact MTL-local concrete logic branch is implemented.
  - keep Socket and MTL policies consuming the same common audio capability/config model;
  - avoid separate duplicated validation logic that can diverge between backends;
  - make unsupported branches named and testable:
    - Socket unsupported because the exact Socket parser/reorder/assembler/storage concrete logic branch is not implemented yet;
    - MTL unsupported only when the exact MTL-local concrete logic branch is not implemented yet after the common model and MTL-facing typed boundary already represent the branch.
  - inability to represent a recognized value in an MTL projection target or project delivery target is not a valid support-policy outcome and must be treated as project incompleteness.
  - completion condition:
    - backend selection checks common audio capability through a shared pipeline and diverges only at backend-local implementation policy / concrete logic branch availability.
- [ ] 140C: Complete the explicit MTL audio projection boundary into `st30p_rx_ops`
  - implement the projection in the refactored MTL/audio path through the appropriate MTL backend-local files and boundaries;
  - add a named projection boundary from the common audio receive/session model plus MTL device/runtime config into MTL `st30p_rx_ops`;
  - consume the common model from the refactored boundaries, including:
    - `contracts/audio/rx_audio_session_config.hpp`;
    - the common audio receive capability/config model introduced by tasks `140` / `140A`;
    - delivery/conversion boundaries where needed.
  - keep MTL device/runtime fields backend-specific:
    - MTL device ports;
    - session ports `P` / `R`;
    - PMD selection;
    - network protocol;
    - queue counts;
    - MTL flags;
    - lcores / socket / NUMA policy where consumed.
  - project common media axes into MTL ST30P fields through explicit per-axis/per-branch logic:
    - payload type;
    - `st30_fmt`;
    - `st30_sampling`;
    - `st30_ptime`;
    - channel count;
    - frame buffer count;
    - frame buffer size/duration policy;
    - receive flags and session policies.
  - use MTL helper APIs or named derived-value helpers for packet/frame sizes where available.
  - do not hardcode `48` samples, `48 kHz`, `1 ms`, stereo, or PCM24 as backend defaults except through named explicit external adapter/default policy.
  - do not silently map ST31/AM824 to linear PCM.
  - projection target must be complete for all in-scope branches the project routes into MTL audio.
  - generic projection helpers must not return `Unsupported` merely because the helper does not know how to continue.
  - if some recognized branch still lacks implementation, `Unsupported` is allowed only from the exact branch-local concrete logic that is supposed to populate that part of the MTL projection.
  - if a recognized value cannot be represented in the MTL-facing typed target, treat that as project incompleteness and complete the target model/boundary.
  - completion condition:
    - MTL audio start can consume the same common audio config validated for Socket/MTL and obtain fully populated `st30p_rx_ops` through the explicit MTL projection boundary.
- [ ] 140D: Keep Socket audio backend on the same common audio validation path
  - rewire or preserve Socket audio operational assembly so it consumes the common audio receive capability/config model before Socket-specific runtime construction;
  - ensure Socket limitations are expressed by Socket support policy:
    - not implemented in Socket parser/reorder/assembler;
    - not implemented in Socket storage/handoff;
    - not implemented in Socket timing/playout;
    - not implemented in Socket wire-format branch.
  - do not use common structural validation to reject modes merely because Socket lacks implementation;
  - keep existing Socket MVP behavior unchanged for currently implemented modes;
  - completion condition:
    - Socket and MTL run through the same common audio structural validation, and Socket-only failures for broader modes are localized as implementation-support failures.
- [ ] 140E: Split project audio delivery/conversion compatibility from receive-session capability
  - introduce or tighten a common audio delivery/conversion support boundary separate from backend receive-session support;
  - implement/tighten this split through the refactored boundaries:
    - `libs/st2110core/include/st2110/contracts/audio/rx_audio_session_config.hpp`;
    - the common audio receive capability/config model introduced by tasks `140` / `140A`;
    - `libs/st2110core/include/st2110/delivery/audio/audio_frame.hpp`;
    - `libs/st2110core/include/st2110/delivery/audio/audio_frame_conversion.hpp`.
  - this boundary must decide whether an already-received audio frame/block can currently be exposed through:
    - `AudioBuffer`;
    - `AudioFrameView`;
    - a PCM conversion helper;
    - OBS handoff;
    - a future native audio contract.
  - do not reject MTL session creation or common receive/session config validation only because a particular MTL audio frame format cannot yet be represented by the current project-local audio delivery layer.
  - Socket may still fail earlier if the exact Socket parser/assembler/storage concrete logic branch for a recognized mode is not implemented.
  - MTL may receive broader MTL-supported modes and then handle project delivery/conversion limitations through localized conversion/drop/report policy.
  - completion condition:
    - receive-session capability and project audio delivery/conversion capability are independent boundaries, and current project-local delivery limits do not narrow the common audio receive model.
- [ ] 140F: Add regression coverage for common audio capability validation across Socket and MTL
  - add tests proving common structural validation accepts recognized audio modes beyond current Socket Level-A implementation;
  - add tests proving Socket rejects not-yet-implemented recognized modes through Socket support policy, not common structural validation;
  - add tests proving MTL support/projection accepts corresponding MTL-supported modes where projection exists;
  - cover at least:
    - PCM16 and PCM24;
    - at least one additional modeled MTL audio format branch;
    - multiple sampling-rate branches;
    - multiple packet-time branches;
    - multiple channel counts;
    - one-stream and redundant/topology branches where modeled.
  - add tests that verify project `InterleavedS32` conversion limitations are reported only by the conversion/delivery boundary;
  - completion condition:
    - tests fail if recognized common audio modes are incorrectly rejected as invalid or if MTL support is narrowed by Socket implementation limits.
- [ ] 141: Implement `MtlRxAudioBackend` skeleton + focused smoke test
  - reuse existing backend lifecycle/state/stats/factory contracts;
  - do not introduce a parallel audio backend API;
  - keep MTL device/session ownership explicit inside the backend;
  - make the skeleton consume the common audio receive config after common structural validation and MTL support validation;
  - keep MTL device/runtime projection, session projection, frame mapping, conversion, and stats as explicit backend-local boundaries;
  - do not encode current project audio storage/conversion limits as MTL session capability limits;
  - add focused smoke coverage proving:
    - backend lifecycle/state behavior is explicit;
    - MTL-owned resource handles remain backend-local;
    - broader common audio capability branches are not collapsed into the Socket Level-A adapter path before MTL support/projection validation.
- [ ] 142: Implement full audio start/stop and frame/block delivery using ST30P RX
  - consume the common audio receive config after common structural validation and MTL backend-support validation;
  - consume MTL-specific device/runtime config only for MTL device/session construction;
  - use `mtl_init` / `mtl_uninit` for device lifecycle;
  - use `st30p_rx_create` / `st30p_rx_free` for ST30P session lifecycle;
  - use blocking `st30p_rx_get_frame` / `st30p_rx_put_frame` receive flow where configured;
  - use the MTL wake mechanism on stop so blocked receive can terminate cleanly;
  - project ST30P RX operations through the common-to-MTL projection boundary from task `140C`;
  - map supported MTL PCM frame data into the current `AudioBuffer` / `AudioFrameView` contract through explicit conversion helpers;
  - keep the current `InterleavedS32` storage boundary explicit and localize any PCM8/PCM16/PCM24 -> S32 conversion there;
  - keep ST31/AM824 and any other non-linear-PCM branch distinct unless an explicit decode boundary is implemented;
  - handle project delivery/conversion unsupported cases through localized report/drop/conversion policy, not through MTL session-start failure;
  - completion condition:
    - MTL audio start/stop constructs and tears down the MTL device/session path from common validated audio config plus MTL runtime config, while project audio delivery limitations remain localized to conversion/handoff.
- [ ] 143: Add meaningful backend-local MTL audio stats
  - populate delivery counters and MTL session/device stats that are actually available/meaningful;
  - include frame-level metadata that MTL exposes where useful:
    - delivered audio frames/blocks;
    - delivery-unsupported frames/blocks;
    - conversion failures;
    - packet totals / received packet counters where available;
    - receive timestamps / timing metadata where consumed;
    - MTL port/device counters where available.
  - do not force socket/video packet-parse, reorder, depacketizer, or assembler stats concepts onto the MTL audio backend;
  - keep unavailable counters explicit rather than synthetic;
  - keep stats valid across broader common audio format, sampling, packet-time, channel-count, and topology branches;
  - completion condition:
    - MTL audio stats describe actual MTL runtime/session/frame behavior and do not depend on Socket Level-A-only assumptions.

---

## Track Jitter Buffer — full receiver playout / de-jitter buffering

> Цель трека:
> - добавить полноценный receiver-side jitter buffer, а не расширять reorder buffer до псевдо-jitter-buffer;
> - сделать архитектуру сразу пригодной и для fixed, и для dynamic/adaptive playout delay;
> - встроить jitter buffering в runtime video/audio backends как основной release layer перед sink/output;
> - не захардкодить алгоритм, а выделить explicit policy / timing / stats / adaptation boundaries.

- [ ] JB001: Ввести явную архитектурную boundary для jitter buffer как отдельного playout/release слоя
  - отделить:
    - packet reorder layer;
    - media-unit reconstruction layer;
    - playout / de-jitter release layer.
  - jitter buffer не должен быть спрятан внутри:
    - `FixedWindowReorderBuffer`;
    - `AudioFixedWindowReorderBuffer`;
    - `Depacketizer`;
    - `AudioFrameAssembler`.
  - явно смоделировать оси:
    - media kind (`Video` / `Audio`);
    - jitter-buffer mode (`Fixed` / `Adaptive`);
    - playout clock source / timing source;
    - startup policy;
    - late-unit policy;
    - underrun policy;
    - shrink/grow adaptation policy;
    - reset/discontinuity policy.
  - текущий `video_playout_timing.hpp` использовать как уже существующую timing boundary для video, а не обходить ad hoc логикой.
  - expected behavior:
    - reorder продолжает отвечать только за packet sequence order;
    - jitter buffer отвечает за release time / playout delay / adaptive hold.
  - tests:
    - architecture regression tests на boundary placement;
    - tests, подтверждающие, что reorder и jitter buffer не смешаны в один helper/class.

- [ ] JB002: Ввести общий receiver time-source / scheduler boundary для release-by-time
  - добавить явную абстракцию времени для runtime release decisions:
    - monotonic local clock;
    - future PTP-aligned / reference-clock-aware mode;
    - deterministic fake clock for tests.
  - ввести scheduler boundary для:
    - next-release deadline computation;
    - wait/wake strategy;
    - stop/reset/drain behavior;
    - no busy-spin / no sink-coupled timing logic.
  - success path:
    - backend/runtime может спросить “что можно release сейчас” без ручной логики sleep в sink.
  - failure / invariants:
    - invalid clock config -> explicit `InvalidValue`;
    - scheduler stop/reset не оставляет hanging delayed units.
  - tests:
    - fake clock deadline tests;
    - reset/stop/drain tests;
    - no-duplicate release tests.

- [ ] JB003: Смоделировать общий jitter-buffer config/policy surface без hardcoded MVP-ограничений
  - добавить explicit config model для jitter buffer:
    - mode (`Fixed` / `Adaptive`);
    - initial target delay;
    - minimum delay;
    - maximum delay;
    - startup prefill policy;
    - max startup wait;
    - grow aggressiveness;
    - shrink aggressiveness;
    - hysteresis / hold-down interval;
    - late tolerance / cutoff policy;
    - underflow handling;
    - discontinuity / restart handling.
  - derived values должны вычисляться helper’ами, а не literals.
  - config surface должна быть общей по архитектуре, но с media-specific validation branches там, где это реально нужно.
  - expected behavior:
    - один и тот же runtime path может работать и в fixed, и в adaptive mode через explicit policy switch.
  - tests:
    - config validation;
    - invalid min/max/initial relations;
    - explicit default-policy tests.

- [ ] JB004: Реализовать общий adaptive controller для target playout delay
  - добавить adaptive algorithm boundary, которая принимает наблюдения runtime и выдает новый target delay.
  - входы adaptive controller:
    - packet/media-unit arrival-to-playout margin;
    - late arrivals;
    - release underruns;
    - queue starvation events;
    - queue fullness / headroom;
    - timestamp discontinuities / stream restarts.
  - algorithm requirements:
    - fast grow on stress / late events;
    - slow shrink on stable periods;
    - hysteresis, чтобы не дрожать около порога;
    - explicit min/max clamp;
    - no unbounded latency growth;
    - no silent reset of learned state.
  - adaptation decision must stay localized in one controller boundary, а не размазываться по backend/runtime loops.
  - tests:
    - grow under burst jitter;
    - slow shrink after stabilization;
    - clamp at min/max;
    - restart/discontinuity reset behavior;
    - non-oscillation regression tests.

- [ ] JB005: Добавить общую runtime telemetry/input model для adaptive jitter estimation
  - ввести normalized measurement snapshots/windowing для jitter adaptation:
    - per-second window;
    - rolling window;
    - EWMA-like aggregates;
    - peak/avg/min margin tracking.
  - для audio отдельно завести telemetry path, совместимый с TS-DF / ADV-style measurement reporting;
  - для video завести release-margin / completion-before-deadline telemetry, не pretending that ST 2110 defines one mandatory adaptive algorithm.
  - expected behavior:
    - controller получает structured metrics, а не raw scattered counters from several classes.
  - tests:
    - measurement window rollover;
    - empty/stable/burst windows;
    - no-loss of adaptation inputs across resets.

- [ ] JB006: Реализовать video jitter buffer как post-reconstruction playout queue
  - добавить video-specific playout buffer поверх reconstructed video units.
  - queue element должен содержать как минимум:
    - unit/frame identity;
    - RTP timestamp;
    - mapped media timestamp;
    - scheduled reconstruction/playout timestamp;
    - completeness/partial flags;
    - release/drop state.
  - интегрировать с existing `video_playout_timing.hpp`, а не дублировать timing math elsewhere.
  - video jitter buffer должен:
    - принимать reconstructed units от `VideoReceivePipeline`;
    - удерживать complete units до scheduled release time;
    - уметь работать и в `Fixed`, и в `Adaptive` mode;
    - явно обрабатывать late-completed units;
    - сохранять future extensibility for `Progressive | Interlaced | PsF`.
  - policy decisions должны быть явными для:
    - partial unit arrival before deadline;
    - completion after deadline;
    - release of incomplete unit when policy allows;
    - drop when policy forbids.
  - tests:
    - complete frame held then released on deadline;
    - late frame dropped/handled by explicit policy;
    - timestamp-wrap / long-run continuity;
    - progressive behavior unchanged except delayed release;
    - future-mode architecture regression for Interlaced/PsF branches.

- [ ] JB007: Ввести audio playout timing boundary и реализовать audio jitter buffer как post-assembly release queue
  - добавить explicit `audio_playout_timing.hpp` / equivalent boundary, аналогичную video playout timing, но audio-specific.
  - queue element должен содержать:
    - block identity;
    - RTP timestamp;
    - mapped media timestamp;
    - scheduled playout timestamp;
    - completeness/continuity state.
  - audio jitter buffer должен:
    - принимать complete assembled audio blocks;
    - удерживать их до playout timestamp;
    - работать в fixed/adaptive mode;
    - иметь explicit continuity/discontinuity handling;
    - учитывать `sampling_rate_hz`, `packet_time_us`, `samples_per_packet` как modeled/derived inputs.
  - policy decisions:
    - late audio block;
    - underrun gap;
    - timestamp jump;
    - stream restart.
  - tests:
    - steady-state playout cadence;
    - late packet/block handling;
    - adaptive growth under jitter;
    - continuity after restart;
    - no hidden dependence on channel order/storage format.

- [ ] JB008: Подключить video jitter buffer в socket video runtime как основной release path
  - изменить `SocketRxVideoBackend` так, чтобы path стал:
    - parse;
    - packet admission;
    - reorder;
    - depacketize / reconstruct;
    - jitter buffer enqueue;
    - timed release to sink.
  - убрать immediate-after-reconstruction delivery semantics из runtime path.
  - release scheduling must stay inside backend/runtime layer, not in sink.
  - stats snapshot должен включать jitter-buffer state/metrics.
  - failure behavior:
    - invalid jitter-buffer config -> explicit start failure;
    - reset/stop очищают queue/controller state.
  - tests:
    - backend integration tests with fake clock;
    - stop/reset while delayed frames exist;
    - no direct sink delivery before scheduled release.

- [ ] JB009: Подключить audio jitter buffer в socket audio runtime как основной release path
  - изменить `SocketRxAudioBackend` так, чтобы path стал:
    - parse;
    - payload-type admission;
    - reorder;
    - audio frame assemble;
    - jitter buffer enqueue;
    - timed release to sink/output.
  - current immediate delivery of complete audio block must be removed from main runtime path.
  - explicit handling for:
    - late block;
    - missing block / underrun;
    - restart / discontinuity.
  - tests:
    - backend integration tests with fake clock;
    - underflow/loss/jitter scenarios;
    - no direct sink delivery before scheduled playout.

- [ ] JB010: Протянуть jitter-buffer config через session/bootstrap/operational boundaries
  - добавить jitter-buffer config в:
    - manual video/audio session-config-driven operational composition;
    - video receiver bootstrap composition;
    - audio receiver bootstrap composition;
    - backend operational configs.
  - использовать refactored contract/bootstrapping boundaries, а не legacy side-channel config.
  - не вводить отдельный ad hoc config path вне current session/bootstrap/runtime architecture.
  - timing-related defaults должны быть explicit at config-construction layer.
  - where signaling-derived timing inputs matter, they must be projected through existing timing/bootstrap boundaries, not bypassed.
  - tests:
    - projection/consistency tests;
    - invalid config rejected before runtime start;
    - fixed/adaptive mode preserved through projection.

- [ ] JB011: Расширить stats/observability model для jitter-buffer runtime и adaptation diagnostics
  - добавить в stats explicit jitter-buffer telemetry:
    - current target delay;
    - current effective delay;
    - queue depth / buffered units;
    - startup prefill state;
    - late-unit drops;
    - underruns;
    - adaptation grow/shrink events;
    - max/min/avg playout margin;
    - audio ADV/TS-DF-style window metrics where applicable.
  - stats должны быть доступны через existing backend stats snapshot boundary.
  - no implicit logging-only observability.
  - tests:
    - zero/default stats;
    - grow/shrink event accounting;
    - reset clears runtime stats correctly;
    - backend snapshot includes jitter metrics.

- [ ] JB012: Добавить полноценное unit/integration test coverage для adaptive jitter buffer
  - покрыть отдельно:
    - common scheduler/time-source boundary;
    - config validation;
    - adaptive controller logic;
    - video playout queue;
    - audio playout queue;
    - backend runtime integration.
  - обязательные сценарии:
    - stable low-jitter stream;
    - short burst jitter;
    - sustained jitter causing target-delay growth;
    - stabilization causing shrink;
    - packet loss + reorder;
    - late-after-deadline arrival;
    - timestamp discontinuity;
    - restart/reset;
    - long-running wraparound continuity.
  - использовать fake clock / deterministic runtime harness, а не flaky sleep-based tests.

- [ ] JB013: Добавить end-to-end/manual-test readiness и обновить MVP exit dependencies
  - после внедрения jitter buffer обновить:
    - manual test procedure;
    - known limitations;
    - Track F dependencies for `198–202`.
  - явно документировать:
    - supported jitter-buffer modes;
    - current adaptive algorithm boundaries;
    - known support limits, если какие-то останутся локализованными.
  - acceptance result:
    - end-to-end video/audio demos больше не зависят только от “лабораторной” low-jitter сети;
    - runtime timing/release policy становится explicit and observable part of MVP/next-phase behavior.
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

### E0. Plugin skeleton
- [ ] 180: Add `plugins/obs_st2110/` module target
  - build the plugin from the refactored OBS plugin block layout:
    - `plugins/obs_st2110/include/obs_st2110/*`;
    - `plugins/obs_st2110/src/*`.
  - build plugin as OBS-loadable `.so`;
  - link required `libobs`;
  - add `obs-frontend-api` linkage if frontend callbacks / Tools menu integration are used;
  - add Qt linkage only where plugin-global dialog/UI is required;
  - install/copy artifact into local OBS plugins directory.
- [ ] 181: Implement ST 2110 OBS input-source skeleton through the refactored OBS plugin boundaries
  - use the explicit OBS-plugin files/boundaries introduced by `Phase R`, including:
    - `plugins/obs_st2110/src/plugin_entry.cpp`;
    - `plugins/obs_st2110/src/source_registration.cpp`;
    - `plugins/obs_st2110/include/obs_st2110/source_config.hpp`;
    - `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp`.
  - register source from the central module entrypoint;
  - define `obs_source_info` with:
    - `get_name`
    - `get_properties`
    - `get_defaults`
    - `create`
    - `update`
    - `destroy`
    - `show`
    - `hide`
    - `activate`
    - `deactivate`
    - `get_width`
    - `get_height`
  - define source-local runtime/state struct and nested source-config struct through the dedicated OBS plugin composition boundaries.
- [ ] 182: Implement minimal source properties/defaults/update flow for ST 2110 receive source
  - implement this in the refactored OBS plugin UI boundary, primarily through:
    - `plugins/obs_st2110/src/source_settings_ui.cpp`;
    - `plugins/obs_st2110/include/obs_st2110/source_config.hpp`.
  - cover:
    - media kind;
    - backend selector;
    - source address / port / payload type;
    - video params;
    - audio params;
    - explicit no-signal / fallback policy;
    - property visibility/dependency logic where media/backend choice changes applicable fields.
- [ ] 183: Implement explicit no-network / no-signal fallback behavior:
  - startup behavior when no packets are available
  - timeout-based behavior when input disappears
  - black video / clear video / hold-last-frame policy as explicitly modeled behavior
  - silence / no-audio-output policy for audio path

### E1. Connect backends to OBS
- [ ] 190: Wire socket backend into the OBS source runtime boundary
  - implement this through the refactored OBS composition boundaries, primarily:
    - `plugins/obs_st2110/src/backend_wiring.cpp`;
    - `plugins/obs_st2110/src/source_runtime.cpp`.
  - project source properties into socket-backend runtime/operational config;
  - start/stop/reset socket receive path from source lifecycle;
  - keep socket-specific logic out of module/frontend/UI layer.
- [ ] 191: Wire MTL backend into the same OBS source runtime boundary where available
  - implement this through the refactored OBS composition boundaries, primarily:
    - `plugins/obs_st2110/src/backend_wiring.cpp`;
    - `plugins/obs_st2110/src/source_runtime.cpp`.
  - project source properties into MTL runtime/operational config;
  - use the same source contract as socket backend;
  - report unavailable/unsupported MTL cases cleanly without reshaping generic OBS source API.
- [ ] 192: Implement source-owned background runtime for receive/start/stop/reconfigure
  - implement this in the dedicated OBS source-runtime boundary:
    - `plugins/obs_st2110/src/source_runtime.cpp`;
    - `plugins/obs_st2110/include/obs_st2110/source_runtime.hpp`.
  - provide:
    - dedicated runtime/thread ownership per source instance;
    - explicit start / stop / join / wake / reset behavior;
    - safe cleanup after partial start failure;
    - lifecycle driven by `create/update/show/hide/activate/deactivate`.
- [ ] 193: Implement backend-to-OBS media handoff boundary
  - implement this through the dedicated handoff files:
    - `plugins/obs_st2110/src/obs_video_handoff.cpp`;
    - `plugins/obs_st2110/src/obs_audio_handoff.cpp`.
  - choose and localize direct handoff vs explicit bounded queue policy;
  - hand off video through `obs_source_output_video(...)`;
  - hand off audio through `obs_source_output_audio(...)`;
  - keep backend-owned frame/block lifetime valid only through the handoff boundary.
- [ ] 194: Implement localized RTP/OBS timestamp mapping policy:
  - video timestamp mapping grounded in ST 2110 system timing rules
  - audio timestamp mapping grounded in ST 2110 / ST 2110-30 rules
  - synthetic and live paths handled explicitly
  - no blind reuse of DistroAV timestamp semantics
- [ ] 195: Verify source lifecycle stability and repeated reconfiguration:
  - repeated create/destroy
  - repeated show/hide
  - repeated activate/deactivate
  - repeated property update/reconfigure
  - repeated backend switch
  - clean stop/join/unload with no stale runtime state

### E2. Environment
- [ ] 169: Create Linux setup/build/install script for Ubuntu 24.04 OBS plugin environment
  - create a repository script for clean Ubuntu 24.04 VM setup, for example:
    - `scripts/setup_ubuntu_obs_plugin_env.sh`;
    - or another clearly named Linux-only setup script.
  - script responsibilities:
    - install compiler/build tooling:
      - compiler toolchain;
      - CMake;
      - Ninja;
      - pkg-config;
      - Git;
    - install OBS runtime/development dependencies:
      - OBS;
      - `libobs` development headers/packages where available;
      - OBS frontend API headers if used by the plugin;
      - Qt development packages only if plugin-global dialog/UI requires Qt;
    - install DPDK/MTL build dependencies needed by the chosen MTL reference version;
    - clone/download/build/install DPDK as required by MTL;
    - clone/download/build/install Media Transport Library;
    - configure persistent or repeatable MTL runtime prerequisites where practical:
      - pkg-config path visibility;
      - dynamic linker visibility;
      - hugepages setup;
      - clear reboot/restart notes where the system cannot apply changes immediately.
    - configure/build the project through CMake/Ninja;
    - install/copy the OBS plugin artifact into the local OBS plugin directory once the plugin target exists;
    - print final verification commands / paths for:
      - `pkg-config --modversion mtl`;
      - plugin artifact location;
      - OBS plugin install location;
      - OBS log location.
  - keep this script Linux-only;
  - do not make this script responsible for Windows support;
  - do not merge this script with `scripts/build_and_test.sh`:
    - `build_and_test.sh` remains a local developer test runner;
    - the setup script is the clean-machine dependency/build/install orchestration path.
  - keep project CMake responsible only for building this repository and finding installed dependencies;
  - do not make CMake clone, patch, build, or install DPDK/MTL;
  - make the script idempotent where practical:
    - tolerate already-installed apt packages;
    - tolerate existing dependency source directories when versions match;
    - fail clearly when an existing dependency checkout is on an unexpected version/branch;
    - avoid silently overwriting local work.
  - support at least a documented default local layout, for example:
    - dependency source root;
    - project build directory;
    - OBS plugin install directory.
  - add a dry-run/help mode or clear usage output if practical.
- [ ] 170: Create Ubuntu 24.04 OBS plugin development environment:
  - compiler/CMake toolchain
  - OBS development packages / `libobs`
  - frontend API headers if used
  - Qt development packages if plugin-global dialog/frontend UI is used
- [ ] 171: Install OBS and verify:
  - OBS starts successfully
  - local plugin `.so` can be discovered/loaded from the OBS plugin directory
  - plugin log messages are visible
- [ ] 172: Decide local code sync / build / install / run loop for OBS plugin work:
  - preferred repo checkout location
  - build directory strategy
  - local install path into OBS plugins dir
  - fastest repeatable rebuild/restart workflow

---

## Track E3 — Pre-MVP spec cleanup before readiness

> Назначение этого track:
> - собрать уже найденные standards/SMPTE cleanup issues в один pre-MVP-exit gate;
> - выполнять эти задачи перед `Track F — MVP exit / readiness for testing`, либо явно документировать оставшиеся ограничения в `202`.

- [x] 196A: Tighten ST 2110-20 `SSN` cross-field validation
  - enforce the standard relationship between:
    - `SSN`
    - `colorimetry=ALPHA`
    - `TCS=ST2115LOGS3`
  - implemented behavior:
    - normal non-`ALPHA` / non-`ST2115LOGS3` streams accept `SSN=ST2110-20:2017`;
    - normal non-`ALPHA` / non-`ST2115LOGS3` streams reject `SSN=ST2110-20:2022`;
    - streams with `colorimetry=ALPHA` or `TCS=ST2115LOGS3` require `SSN=ST2110-20:2022`;
    - structurally unknown future `SSN` values remain representable through `Other`;
    - runtime `PixelFormat`, frame storage, depacketizer, and pipeline projection shape remain unchanged.
- [x] 196B: Tighten `RANGE` modeling and validation
  - model `FULLPROTECT` explicitly as a known `VideoRange` value instead of routing it through `Other`;
  - implemented ST 2110-20 range constraints:
    - with `colorimetry=BT2100`, only `NARROW` / `FULL` are accepted;
    - outside BT2100 context, `NARROW` / `FULLPROTECT` / `FULL` are accepted;
    - absent `RANGE` keeps the existing optional / unspecified signaling-level behavior;
    - unknown future range tokens remain representable through `Other + raw_token`;
  - runtime frame storage, `PixelFormat` projection shape, and depacketizer behavior remain unchanged.
- [x] 196C: Add SDP `PAR` media-description modeling
  - parse optional `PAR=<w>:<h>` from video `a=fmtp`;
  - represent PAR as a signaling/media-description property, not as runtime frame storage;
  - implemented signaling-level default:
    - absent `PAR` -> `1:1`;
  - implemented validation:
    - both parts must be positive integers;
    - malformed forms are rejected;
    - canonical/minimal ratio form is applied in the raw fmtp parsing path;
  - runtime `VideoFrame`, `PixelFormat`, depacketizer, placement, and runtime projection behavior remain unchanged.
- [x] 196D: Enforce ST 2110-20 `width` / `height` SDP limits
  - validate signaled video dimensions as `1..32767` in the signaling/media-description boundary;
  - keep lower-level runtime/frame validation separate where it already exists;
  - current UYVY-specific even-width runtime constraint remains unchanged and localized in runtime projection/format validation;
  - covered by focused tests for:
    - width/height `1` accepted structurally;
    - width/height `32767` accepted structurally;
    - width/height `0` rejected;
    - width/height `32768` rejected;
    - current UYVY runtime projection constraints still remaining localized.
- [x] 196E: Tighten SDP `exactframerate` canonical parsing
  - keep `exactframerate` parsing local to `video_sdp_fmtp.hpp`;
  - implemented behavior:
    - integer frame rates are accepted only as a single decimal integer, not `N/1`;
    - rational frame rates are accepted only in the smallest numerator/denominator representation;
    - numerator and denominator must be positive;
    - malformed forms reject as `InvalidValue`;
  - timestamp mapping and runtime cadence behavior remain unchanged;
  - covered by focused tests for:
    - `25` accepted;
    - `25/1` rejected;
    - `30000/1001` accepted;
    - reducible rational such as `60000/2002` rejected;
    - zero numerator/denominator rejected;
    - existing valid SDP examples remaining accepted.
- [x] 196F: Require media-level `mediaclk` for final ST 2110 video SDP ingestion
  - keep raw SDP parsing scope-aware and non-destructive;
  - final `VideoStreamSignaling` ingestion now requires a selected media-level `a=mediaclk`, as required by ST 2110-10;
  - session-level `mediaclk` may remain preserved in the raw model, but does not by itself make final ST 2110 video ingestion standards-clean;
  - media-level value continues to override a session-level value where both are present;
  - timing interpretation remains localized in SDP timing/ingestion boundaries and is not moved into runtime pipeline or depacketizer.
- [x] 196G: Tighten required `ts-refclk` / reference-clock validation
  - final ST 2110 video SDP ingestion now requires `ts-refclk`;
  - known reference-clock forms are validated more strictly:
    - `ptp=IEEE1588-2008:<gmid>[:domain]`;
    - `ptp=IEEE1588-2008:traceable`;
    - `localmac=<EUI-48 MAC>`;
  - unknown/open-ended reference-clock forms remain preservable only through the intentional `Other` model path;
  - malformed known `ptp=` / `localmac=` forms no longer pass silently;
  - no PTP runtime behavior was added in this task;
  - covered by focused tests for:
    - missing `ts-refclk` rejected by final video SDP ingestion;
    - valid PTP GMID accepted;
    - valid PTP traceable form accepted;
    - malformed PTP GMID/domain rejected;
    - valid localmac accepted;
    - malformed localmac rejected.
- [x] 196H: Finalize `MAXUDP` value policy against ST 2110-10 limits
  - reuse the existing `MAXUDP -> VideoStreamSignaling::max_udp_datagram_bytes -> PacketParsePolicy` path.
  - validate values against current project policy for:
    - Standard UDP Size Limit;
    - Extended UDP Size Limit;
    - absent `MAXUDP` defaulting to Standard UDP Size Limit.
  - keep pure wire parsing separate from policy validation.
  - prefer minimal changes to existing `.hpp` files:
    - `packet_parse.hpp`
    - `video_signaling.hpp`
    - `video_sdp_fmtp.hpp` unchanged because malformed numeric values are already rejected in parsing
    - related tests only
  - do not introduce a parallel runtime config mechanism.
  - add focused tests for:
    - absent `MAXUDP` uses Standard UDP Size Limit;
    - valid standard-sized `MAXUDP`;
    - valid extended-sized `MAXUDP`;
    - non-boundary / above-Extended `MAXUDP` values rejected by current project policy;
    - packet parse policy receives the final effective limit.
- [x] 196I: Tighten raw SDP `a=source-filter` grammar validation
  - keep source-filter as raw transport metadata, outside `VideoStreamSignaling`.
  - validate known structural fields more strictly:
    - accepted filter mode tokens;
    - required nettype / addrtype / destination address;
    - at least one source address where required;
    - malformed source-list forms rejected.
  - preserve the original raw value and parsed fields.
  - prefer minimal changes to existing `.hpp` files:
    - `video_sdp_media_section.hpp`
    - related tests only
  - do not wire source-filter into socket/backend behavior in this task.
  - add focused tests for:
    - valid session-level source-filter;
    - valid media-level source-filter;
    - invalid filter mode rejected;
    - missing destination/source fields rejected;
    - parsed source-filter remains scope-aware;
    - backend/runtime behavior remains untouched.
- [x] 196J: Add runtime RTP payload-type admission boundary
  - add an explicit video packet admission / validation helper that compares `PacketView::rtp.payload_type` with the configured or signaled expected payload type.
  - place this boundary before reorder/depacketizer use in the eventual receive path.
  - keep raw RTP parsing separate from stream-specific admission policy:
    - RTP parser should parse PT;
    - admission policy should decide whether this packet belongs to the selected stream.
  - wrong-PT packets should be rejected/dropped/accounted for locally without mutating depacketizer state.
  - prefer minimal changes to existing `.hpp` files where possible:
    - new small header `packet_admission.hpp`
    - `rx_config.hpp` / `video_signaling.hpp` unchanged
    - related tests only
  - add focused tests for:
    - matching dynamic payload type accepted;
    - mismatching payload type rejected/dropped;
    - payload type validation remains separate from generic RTP parsing;
    - depacketizer is not entered for wrong-PT packets.
- [x] 196K: Tighten raw SDP `m=video` media-line validation for ST 2110 video
  - keep this in the raw SDP media-section / final ingestion boundary.
  - validate at least:
    - selected video payload type is in the dynamic RTP payload type range `96..127`;
    - `m=video` line has a structurally valid port token;
    - protocol token is an expected RTP profile for the current project scope, initially `RTP/AVP` unless a later explicit branch supports more.
  - preserve raw media-line text for future transport/bootstrap use.
  - do not mix socket bind/join behavior into this task.
  - prefer minimal changes to:
    - `video_sdp_media_section.hpp`
    - `video_sdp_ingestion.hpp` unchanged because final ingestion already passes through the raw media-section selection boundary
    - related tests only
  - add focused tests for:
    - valid `m=video 50000 RTP/AVP 112`;
    - non-dynamic PT rejected for ST 2110 raw video;
    - malformed port rejected;
    - unexpected protocol rejected;
    - existing valid SDP ingestion remains unchanged.
- [x] 196L: Add RTCP tolerance / UDP datagram classification boundary
  - define a local classifier before media packet parsing / pipeline use:
    - RTP media packet candidate;
    - RTCP packet candidate;
    - malformed / unsupported datagram.
  - RTCP packets should be tolerated:
    - ignored or counted separately;
    - not fed to `PacketView::from_udp_datagram()`;
    - not counted as malformed ST 2110-20 media packets.
  - keep actual RTCP semantic interpretation out of MVP unless later needed.
  - prefer minimal changes:
    - no new helper/header was required because the local classification boundary is already localized in the shared socket receive runtime base
    - parser stats unchanged
    - related tests only
  - add focused tests for:
    - valid RTP media packet classified as media;
    - common RTCP packet types classified as RTCP/tolerated;
    - malformed short datagram rejected;
    - RTCP does not reach depacketizer.
- [x] 196M: Enforce cross-packet SRD row/offset monotonicity inside assembly units
  - extend the mode-aware receive semantics / depacketizer state with a localized packet-order validation boundary.
  - enforce for current `Progressive + GPM` MVP path:
    - SRD Row Number must not go backwards within the current frame;
    - within the same row, SRD Offset must strictly increase across successive segments/packets;
    - overlapping or regressing segments are rejected before writing.
  - keep future `Interlaced` / `PsF` behavior behind the existing mode-aware branches.
  - do not move this into generic low-level `st2110_20.hpp` parsing, because row/offset continuity is assembly-unit/mode dependent.
  - prefer minimal changes:
    - `video_receive_semantics.hpp`
    - `depacketizer.hpp`
    - related tests only
  - add focused tests for:
    - valid multi-packet same-row fragmentation;
    - later packet with lower row rejected;
    - later packet with same row and lower/equal offset rejected;
    - row advance accepted;
    - rejection does not emit or corrupt a frame.
- [x] 196N: Make depacketizer packet segment writes atomic
  - refactor `Depacketizer::write_packet_segments()` so one packet is fully validated/mapped before any segment is written into `FrameAssembler`.
  - collect all `VideoFrameWriteOp`s first.
  - only after all placement/bounds checks succeed, apply writes.
  - ensure invalid second/third SRD segment cannot partially mutate the current assembly unit.
  - keep `FrameAssembler` byte-oriented and format-agnostic.
  - prefer minimal changes:
    - `depacketizer.hpp`
    - `video_segment_placement.hpp` unchanged
    - related tests only
  - add focused tests for:
    - packet with all valid segments writes successfully;
    - packet with invalid later segment writes none of its segments;
    - current unit remains recoverable/drop-policy-consistent after rejected packet;
    - behavior remains unchanged for valid progressive packets.
- [x] 196O: Require or explicitly default `SSN` in standards-aware video signaling validation
  - close the gap where SDP parsing requires `SSN`, but manually constructed `VideoStreamSignaling` can validate with `media.signal_standard == nullopt`.
  - choose one explicit policy:
    - require `signal_standard` for standards-clean `VideoStreamSignaling`;
  - keep this separate from `196A`, which handles the `ST2110-20:2017` vs `ST2110-20:2022` cross-field rule.
  - prefer minimal changes:
    - `video_signaling.hpp`
    - related tests only
  - add focused tests for:
    - missing `SSN` rejected by standards-clean signaling validation;
    - SDP-derived signaling still passes when `SSN` is present;
    - final runtime/bootstrap/timing projection helpers reject missing `SSN` before downstream projection logic.
- [x] 196P: Correct ST 2110-21 `TP` / `TROFF` / `CMAX` sender timing validation
  - re-checked `validate_video_sender_signaling(...)` and final SDP ingestion behavior against ST 2110-21;
  - final video SDP ingestion no longer silently treats absent `TP` as `2110TPN`;
  - `TP` is now required for ST 2110-21-conforming video RTP SDP ingestion;
  - `TROFF` and `CMAX` are now validated according to ST 2110-21 optional-parameter semantics and local modeled policy, rather than being rejected for `Narrow` / `NarrowLinear` solely by sender class;
  - numeric constraints are now explicit:
    - `TROFF`, when present, must be a positive integer microsecond value;
    - `CMAX`, when present, must be valid for the local modeled policy;
  - stricter sender/conformance policy remains separate from normal receiver ingestion;
  - changes stayed localized to:
    - `video_signaling.hpp`
    - `video_sdp_ingestion.hpp`
    - `video_sdp_fmtp.hpp`
    - related tests
  - focused tests now cover:
    - missing `TP` rejected by final ST 2110 video SDP ingestion;
    - `TP=2110TPN`, `TP=2110TPNL`, `TP=2110TPW` accepted;
    - `TROFF` accepted where standard-valid;
    - `TROFF=0` rejected;
    - `CMAX` handling matches the corrected sender timing policy;
    - receiver capability rejection still remains in `video_receiver_timing_signaling.hpp`.
- [x] 196Q: Tighten raw SDP `c=` connection-data structural validation
  - kept `c=` as raw transport metadata outside `VideoStreamSignaling`;
  - known SDP/ST 2110-relevant structure is now validated more explicitly:
    - `nettype`, currently `IN`;
    - `addrtype`, currently `IP4` and `IP6`;
    - non-empty base connection address;
    - slash parameters only in forms where they are structurally meaningful;
    - malformed TTL / address-count forms rejected;
  - no socket join / multicast source filtering was implemented here;
  - changes stayed localized to:
    - `video_sdp_media_section.hpp`
    - related tests only
  - focused tests now cover:
    - valid unicast `c=IN IP4 192.0.2.10`;
    - valid multicast `c=IN IP4 239.1.1.1/32`;
    - malformed `nettype` / `addrtype` rejected;
    - malformed slash-parameter forms rejected;
    - existing session/media `c=` preservation remains unchanged.
### 197A. Reset socket RX operational-start architecture onto explicit operational configs
- [x] 197A1: Define common socket RX operational transport/policy model
  - ввести явную общую абстракцию для socket operational input, без media-specific payload/runtime деталей;
  - рекомендованная форма:
    - `SocketRxOperationalCommonConfig`
      - `SocketRxOpenConfig open_config`
      - `PacketParsePolicy packet_parse_policy`
  - при необходимости общего expected-payload-type admission для socket runtime решить явно:
    - либо хранить в media-specific config;
    - либо вынести в common config только если это действительно одинаковая runtime-boundary для audio/video;
  - добавить strict validation helper для common config;
  - не использовать `union` для `RxVideoConfig` / `RxAudioConfig`;
  - если нужен единый media-erased carrier, использовать `std::variant`, а не raw union.
- [x] 197A2: Define explicit media-specific socket operational configs
  - ввести:
    - `SocketRxVideoOperationalConfig`
    - `SocketRxAudioOperationalConfig`
  - оба должны содержать `SocketRxOperationalCommonConfig` как вложенную common-часть, а не дублировать transport/policy fields плоско;
  - `SocketRxVideoOperationalConfig` должен явно содержать:
    - `RxVideoConfig rx_config`
    - `VideoReceivePipelineConfig receive_pipeline_config`
    - `VideoRtpTimestampMapperConfig timestamp_mapper_config`
    - при необходимости уже сейчас — explicit timing/playout operational field, но только если runtime реально его потребляет;
  - `SocketRxAudioOperationalConfig` должен явно содержать:
    - `RxAudioConfig rx_config`
    - `AudioRtpPacketPolicy audio_packet_policy`
    - `AudioFrameAssemblerConfig frame_assembler_config`
    - `AudioReorderBufferConfig reorder_buffer_config`
    - `AudioRtpTimestampMapperConfig timestamp_mapper_config`
    - `ParsedAudioChannelOrder channel_order` как уже смоделированную ось signaling/bootstrap path;
  - добавить strict validation helpers:
    - `validate_socket_rx_video_operational_config(...)`
    - `validate_socket_rx_audio_operational_config(...)`
  - validation должна проверять:
    - common transport/policy part;
    - media-specific config validity;
    - cross-consistency между `rx_config` и derived runtime components;
    - отсутствие hidden recomputation inside backend.
- [x] 197A3: Introduce explicit socket-specific backend start interfaces instead of legacy manual media start
  - архитектурно отделить generic backend lifecycle от socket-specific operational start boundary;
  - не оставлять concrete socket backend с двумя `start_video(...)` / `start_audio(...)`;
  - recommended direction:
    - ввести socket-specific interfaces:
      - `ISocketRxVideoBackend`
      - `ISocketRxAudioBackend`
    - concrete socket backends должны принимать только operational config:
      - `start_video(const SocketRxVideoOperationalConfig&, IVideoFrameSink&)`
      - `start_audio(const SocketRxAudioOperationalConfig&, IAudioFrameSink&)`
  - manual `RxVideoConfig` / `RxAudioConfig` start path не должен оставаться методом concrete backend;
  - generic `IRxBackend` lifecycle/state/stats boundary оставить separate от socket operational input boundary;
  - если потребуется refactor generic media backend interfaces, делать это явно и последовательно, а не оставлять hybrid API shape.
- [x] 197A4: Move all manual/bootstrap-to-operational assembly out of backend classes into explicit adapter layer
  - backend classes не должны строить:
    - `SocketRxOpenConfig`
    - `PacketParsePolicy`
    - receive pipeline config
    - audio packet/reorder/assembler config
    - timestamp mapper config
  - ввести explicit adapter/projection layer вне backend internals:
    - `socket_rx_video_operational_config_from_video_receiver_bootstrap(...)`
    - `socket_rx_video_operational_config_from_rx_video_config(...)`
    - `socket_rx_audio_operational_config_from_audio_receiver_bootstrap(...)`
    - `socket_rx_audio_operational_config_from_rx_audio_config(...)`
  - manual path может существовать только как explicit external adapter layer, а не как backend-local “legacy start” policy;
  - signaling/bootstrap path должен оставаться richer primary source там, где он уже смоделирован:
    - video bootstrap already carries packet parse policy + pipeline config + timing config;
    - audio bootstrap must be extended as needed to stop collapsing richer signaling/runtime axes before backend start. :contentReference[oaicite:2]{index=2}
- [x] 197A5: Rewire `SocketRxVideoBackend` to operational-only runtime start
  - удалить backend-local builders/defaults полностью;
  - удалить manual `start_video(const RxVideoConfig&, ...)` from concrete socket backend API;
  - runtime start должен использовать только prebuilt `SocketRxVideoOperationalConfig`;
  - `process_received_datagram(...)` must enforce `packet_parse_policy` explicitly before packet parsing;
  - backend должен только:
    - validate operational config;
    - create/open socket port;
    - allocate receive buffer from explicit policy;
    - construct runtime objects from explicit operational config;
    - run receive loop and deliver frames;
  - hidden `PartialFramePolicy::Drop`, hidden empty `PacketParsePolicy`, hidden hardcoded timestamp-mapper anchor/config не допускаются.
- [x] 197A6: Rewire `SocketRxAudioBackend` to the same operational-only architecture
  - удалить backend-local builders/defaults:
    - `build_packet_parse_policy(...)`
    - `build_open_config(...)`
    - `build_audio_packet_policy(...)`
    - `build_audio_frame_assembler_config(...)`
    - `build_audio_reorder_buffer_config(...)`
    - `build_audio_timestamp_mapper_config(...)`
  - удалить manual `start_audio(const RxAudioConfig&, ...)` from concrete socket backend API;
  - runtime start должен использовать только prebuilt `SocketRxAudioOperationalConfig`;
  - `process_received_datagram(...)` must enforce `packet_parse_policy` explicitly before payload-type/audio-packet parsing;
  - backend должен only consume explicit operational audio runtime components and not reconstruct hidden defaults locally;
  - current audio runtime support limits, if still present, must remain explicit in adapter/validation boundaries, not as unnamed backend constants.
- [x] 197A7: Keep `SocketRxSingleMediaBackendBase` generic and remove media-specific policy assembly pressure from it
  - base class не должен знать про video/audio operational config internals;
  - base class should remain responsible only for:
    - socket port lifecycle;
    - receive thread;
    - generic datagram accounting/stats;
    - receive-buffer ownership;
    - generic RTCP-like tolerance helpers;
  - если обнаружится реально shared logic between video/audio operational runtime consumption, выносить только media-agnostic helpers:
    - common-config validation/use;
    - packet-policy buffer sizing/admission helpers;
    - not media-specific assembly/projection;
  - base class must not become a hidden policy-construction layer.
- [x] 197A8: Extend bootstrap models where needed so richer already-modeled axes reach socket backends without collapse
  - video path:
    - проверить, достаточно ли `VideoReceiverBootstrapConfig` для final socket operational projection;
    - если timing/playout/runtime-consumed fields still remain outside operational path, explicitize them through named boundary rather than ad hoc backend defaults;
  - audio path:
    - расширить `AudioReceiverBootstrapConfig` до полного operational projection source where needed;
    - не терять `channel_order` and other already-modeled signaling/runtime axes before backend start;
  - manual config path must remain explicit and visibly poorer than bootstrap path only where this is truly intended, not because the backend silently rebuilds missing defaults.
- [x] 197A9: Add focused regression/architecture tests for the new start boundary
  - video tests:
    - operational config accepted when fully consistent;
    - mismatched `open_config` vs `rx_config` rejected;
    - mismatched receive-pipeline config vs `rx_config` rejected;
    - packet-size policy enforced in receive path;
    - no manual `RxVideoConfig` start method on concrete socket backend;
  - audio tests:
    - operational config accepted when fully consistent;
    - mismatched audio packet/reorder/assembler/timestamp config vs `rx_config` rejected;
    - packet-size policy enforced in receive path;
    - no manual `RxAudioConfig` start method on concrete socket backend;
  - architecture tests:
    - socket backend no longer owns adapter/default-building role;
    - base class remains media-agnostic;
    - bootstrap-to-operational adapters preserve already-modeled axes instead of rebuilding hidden defaults.
- [x] 197B: Replace the hardcoded video reorder window literal with an explicit named runtime/support boundary
  - тип расхождения:
    - с правилами/архитектурой.
  - конкретное расхождение:
    - current video socket backend создает reorder buffer через literal `FixedWindowReorderBuffer(32)`.
    - сам `FixedWindowReorderBuffer` уже умеет принимать explicit `window_size`, но video runtime boundary не моделирует источник этого значения и тем самым кодирует временное решение magic number’ом.
    - это расходится с project rules:
      - temporary support limits must be expressed through explicit modeled/validation/support boundaries;
      - important helper/policy boundaries must not remain implicit.
  - как исправлять по сути:
    - вынести reorder window в explicit named policy/config:
      - runtime receive policy;
      - bootstrap config;
      - or localized default helper with a named constant + validation boundary.
    - backend должен получать это значение явно, а не зашивать literal в construction path.
- [x] 197C: Make initial RTP-to-`TimestampNs` anchoring explicit in socket RX runtime
  - тип расхождения:
    - со стандартным timing смыслом + с правилами/архитектурой.
  - конкретное расхождение:
    - current video и audio socket backends создают RTP timestamp mappers с:
      - `anchor_rtp_timestamp = 0`;
      - `anchor_timestamp_ns = 0`.
    - из-за этого первый реально наблюденный ненулевой RTP timestamp сразу мапится в ненулевой `TimestampNs`, хотя сама политика “что считается internal origin” в runtime никак явно не задана.
    - это проблемно по двум причинам:
      - ST 2110 timing model трактует RTP timestamps как time-of-sampling / epoch-related timing quantity, а не как произвольный hidden local rebasing artifact; RP 2110-25 также формулирует RTP-time measurements через epoch-related `RTPTimestamp_encoded`;
      - project rules требуют explicit modeled timing/policy boundary вместо скрытого default anchoring.
  - как исправлять по сути:
    - явно ввести policy/boundary для initial anchoring:
      - “first observed RTP timestamp becomes local zero”;
      - или “preserve RTP-derived absolute relation from bootstrap reference”;
      - или другой named mode.
    - backend/bootstrap должен явно выбирать этот режим, а mapper не должен получать hidden `0/0` anchor без объясненной политики.
- [x] 197D: Tighten raw audio SDP `m=audio` validation to the same standards-aware transport boundary already used for video
  - тип расхождения:
    - со стандартом + с правилами.
  - конкретное расхождение:
    - current raw audio SDP parser `parse_audio_m_line_payload_types(...)` проверяет по сути только:
      - что line starts with `m=audio`;
      - что есть минимум 4 whitespace-separated tokens;
      - что payload type численно в диапазоне `0..127`.
    - parser не валидирует:
      - сам `port` token;
      - `proto` token;
      - dynamic RTP payload-type range as an explicit ST 2110 boundary.
    - для ST 2110 это слишком слабо:
      - ST 2110-10 требует RTP streams with dynamic payload types in `96..127`, unless fixed designation exists;
      - ST 2110-30 требует SDP-based signaling for PCM audio streams under AES67/ST 2110 constraints;
      - project rules требуют strict parsing and explicit validation boundaries.
  - как исправлять по сути:
    - сделать для audio такой же explicit raw media-line validation boundary, как уже сделано для video:
      - validate `port`;
      - validate supported `proto` through explicit local policy;
      - validate PT against current ST 2110 audio policy explicitly, а не только against `<=127`.
    - malformed `m=audio` must fail early at raw SDP boundary, not later by accident.
- [x] 197E: Replace attribute-name-only audio clock-signaling checks with real parsing/validation of `ts-refclk` and media-level `mediaclk`
  - тип расхождения:
    - со стандартом + с правилами.
  - конкретное расхождение:
    - current final audio SDP ingestion checks required timing signaling only by attribute-name presence in preserved unknown-attribute lists:
      - `ts-refclk` считается “present”, если где-то сохранился attribute with that name;
      - `mediaclk` считается “present”, если media-level unknown attribute с таким именем найден.
    - при этом malformed known forms могут пройти финальную проверку просто потому, что имя атрибута присутствует.
    - это расходится:
      - со ST 2110-10 clock signaling model, где `ts-refclk` и `mediaclk` — это не просто names, а structured signaling forms;
      - со strict-parse project rules.
  - как исправлять по сути:
    - добавить dedicated audio timing/reference-clock parsing boundary:
      - parse known `ts-refclk` forms structurally;
      - parse/validate media-level `mediaclk` structurally;
      - distinguish malformed known forms from unknown future/open forms explicitly.
    - final audio ingestion should depend on parsed/validated timing objects, not on raw attribute-name presence.
- [x] 197F: Separate structural audio signaling validity from the current Level A receiver-support boundary
  - тип расхождения:
    - со стандартом + с правилами/архитектурой.
  - конкретное расхождение:
    - current `validate_audio_stream_signaling(...)` validates `AudioStreamSignaling` directly against `audio_level_a_receiver_baseline()`.
    - meaning:
      - modeled signaling validity is currently collapsed into “fits current Level A-oriented receiver baseline”;
      - structurally valid ST 2110-30 streams outside current baseline are rejected too early at signaling/model boundary.
    - это архитектурно узко и стандартно неудачно, потому что ST 2110-30 explicitly models multiple conformance levels, while current signaling validator hardcodes one local receiver-support range as if it were the structural signaling truth.
  - как исправлять по сути:
    - разделить два слоя:
      - structural audio signaling validation:
        - valid PCM signaling shape;
        - valid bit depth;
        - valid channel-order syntax/model;
        - valid numeric/signaling structure;
      - receiver/runtime support validation:
        - current Level A-oriented baseline;
        - current supported rates/ptime/channel counts.
    - raw SDP → signaling adapter должен строить structurally valid signaling object отдельно от later “is supported by current receiver policy” checks.
- [x] 197G: Add explicit reorder flush/tolerance policy to the socket receive path
  - тип расхождения:
    - с правилами/архитектурой.
  - конкретное расхождение:
    - current socket video/audio receive paths still only drain strict `pop_next()` order and do not apply any explicit gap-flush/tolerance policy.
    - при этом concrete reorder buffers уже expose explicit flush helpers, но socket runtime boundary их никак не моделирует и не использует.
    - из-за этого first-gap behavior сейчас остается hidden stall policy, а не explicit receiver/runtime policy.
  - как исправлять по сути:
    - поднять gap handling на explicit runtime boundary:
      - named receive tolerance policy;
      - explicit flush mode/helper usage;
      - configurable/localized first-gap behavior.
    - socket backend должен явно решать:
      - ждать missing packet;
      - flush gap once;
      - or use another named policy.
    - concrete buffer helpers должны оставаться implementation detail, а runtime behavior — стать explicit и проверяемым на backend level.
- [x] 197H: Apply operational packet-size / `MAXUDP` policy in the socket receive parse path
  - тип расхождения:
    - со стандартом + с правилами/архитектурой.
  - конкретное расхождение:
    - current socket video/audio backends still build `PacketParsePolicy` and size receive buffers from it, but actual live media parsing bypasses the integrated packet-policy path:
      - video receive path parses via direct staged packet parsing;
      - audio receive path parses via direct audio RTP packet parsing.
    - то есть existing packet-size / `MAXUDP` policy boundary сейчас operationally не применяется в реальном receive parse path.
    - это расходится:
      - со ST 2110-10 UDP datagram size semantics;
      - с project rules, потому что already-modeled policy boundary остается construction artifact instead of real runtime behavior.
  - как исправлять по сути:
    - сделать так, чтобы live receive parse path использовал existing packet-policy boundary перед packet admission / deeper parsing;
    - packet-size / `MAXUDP` handling should be enforced by the real receive path, not only by:
      - receive buffer sizing;
      - standalone helper availability;
      - signaling/bootstrap construction.
    - нельзя оставлять separate “policy exists” and “operational parser ignores it” behavior.
---

## Track F — MVP exit / readiness for testing
- [ ] 198: End-to-end video MVP demo on Linux: receive and display/save progressive ST2110-20 GPM stream
- [ ] 199: End-to-end audio MVP demo on Linux: receive and play/save audio stream
- [ ] 200: End-to-end OBS demo with selectable backend and basic UI
- [ ] 201: Document manual test procedure for MVP
- [ ] 202: Document known limitations still allowed at MVP exit
  - if some runtime branches remain intentionally `Unsupported` behind already-modeled standard axes, document them explicitly as localized MVP limitations
  - if signaling-driven bootstrap is still partial, document the remaining manual-config scaffolding explicitly

---

# Phase 2 — Medium

## Track G — Video formats / audio formats / extensibility
- [ ] 210: Add at least one more video pixel format beyond UYVY
- [ ] 211: Audit format-specific code paths so new formats require localized additions only
- [ ] 212: Add additional audio format/profile support if needed
- [ ] 213: Add shared format capability description/query API
- [ ] 214: Expand standards-aware video signaling / media-property coverage through the already-modeled video signaling representation
  - continue expanding support for signaled video/media-property variants without changing the core signaling/runtime contracts introduced in MVP
  - keep parsing, validation, and projection extensions localized to existing model/adapter boundaries
  - after `069D8`, future work should mostly add implementation behavior for already-explicit enum branches rather than routing known standard values through `Other`
- [ ] 214A: Expand video SDP `a=` attribute coverage through the already-modeled raw SDP media-section boundary
  - add support for additional video-relevant `a=` attributes by filling existing per-attribute/per-PT parsing branches inside the raw SDP/media-section ingestion architecture introduced in MVP
  - do not reshape `VideoStreamSignaling`, raw SDP media-section model, or the final ingestion pipeline only to add new `a=` attribute coverage
  - keep new attribute parsing localized as:
    - new raw parsed fields where needed
    - new per-attribute parser branches
    - new mapping/validation branches where applicable
  - add focused tests for each newly supported `a=` attribute and for coexistence with already-supported attributes
- [ ] 214B: Expand remaining video SDP `a=` / fmtp media-parameter coverage through existing parser branches
  - add support for additional ST 2110-relevant SDP attributes and fmtp media type parameters by filling existing raw media-section, per-attribute parser, and mapping branches
  - do not reshape the SDP ingestion pipeline, `VideoStreamSignaling`, or runtime bootstrap contracts only to add new SDP coverage
  - keep each newly supported field localized to:
    - raw parsed representation
    - parser branch
    - mapping branch
    - structural validation branch, where applicable
  - add focused tests for each newly supported SDP field and for coexistence with already-supported fields
- [ ] 214C: Integrate parsed SDP transport metadata into receiver bootstrap boundaries
  - consume the raw SDP session/media transport boundary introduced in `069D10` / `069D12` / `069D16`
  - derive backend-facing transport hints from parsed SDP where appropriate:
    - media/session `c=`
    - parsed multicast address / TTL / address-count fields
    - scoped `source-filter`
  - keep manual/local transport fields explicit and overrideable for tests/scaffolding
  - keep pure SDP parsing separate from backend/runtime socket or MTL implementation
  - future implementation should fill existing raw-SDP / bootstrap adapter branches rather than changing SDP parser shape
  - add focused tests for:
    - `c=` to transport hint projection
    - source-filter to multicast-source hint projection
    - manual override behavior
    - single-stream bootstrap behavior
- [ ] 214D: Add redundant RTP stream modeling and selection policy through existing SDP redundancy boundary
  - consume duplicate media-section candidate records introduced in `069D13`
  - model `a=group:DUP` / duplicate RTP stream relationships through raw SDP `mid` linkage
  - define where primary/secondary stream selection and future seamless/redundant receive behavior will live
  - do not implement full SMPTE ST 2022-7 switching in this task unless explicitly chosen later
  - keep backend and receive-pipeline public contracts stable
  - add focused architecture tests proving:
    - redundant-stream support plugs into existing SDP/bootstrap boundaries
    - duplicate candidates are selected/ignored through a local policy point
    - parser shape does not need to change for future redundant receive behavior
- [ ] 215: Expand audio signaling / channel-order / channel-mapping support through the already-modeled audio signaling boundary
  - add implementation for additional channel-order / channel-mapping cases without changing the core audio signaling/runtime contracts introduced in MVP
  - keep reordering/adaptation localized to the pre-defined boundaries

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
- [ ] 229: Implement BPM runtime receive behavior through the already-modeled packing-mode branches
  - fill BPM-specific depacketize / padding / validation / runtime-policy logic through existing packing-mode dispatch/boundaries
  - keep signaling model, runtime config shape, and pipeline contracts unchanged
  - add focused tests
- [ ] 229A: Implement fuller ST 2110-21 receiver timing / tolerance behavior through the already-defined timing/capability/playout boundaries
  - fill buffering / tolerance / release behavior inside the boundaries introduced in MVP
  - keep parser/reorder/depacketizer contracts unchanged
  - add focused tests
- [ ] 229B: Add stricter optional SDP sender/conformance validation policy as a separate mode
  - keep normal receiver-side SDP ingestion permissive for optional standard parameters
  - add a localized stricter validation helper/policy only for conformance checking or sender-profile validation
  - do not make strict conformance policy the default receiver ingestion behavior
  - avoid changing existing SDP parser shape
  - add focused tests proving:
    - normal receiver ingestion accepts absent optional parameters
    - strict policy can reject missing optional-but-required-by-local-policy parameters
    - receiver timing capability checks remain in `video_receiver_timing_signaling.hpp`

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
- [ ] 267A: Harden SDP ingestion interoperability and corner-case handling through the existing raw media-section / per-attribute parsing architecture
  - harden handling of:
    - duplicate `a=` attributes
    - attribute ordering variations
    - session-level vs media-level inheritance / override behavior
    - unknown-attribute coexistence
    - partial/legacy SDP variants
    - strict-vs-tolerant parsing policy boundaries
    - malformed-but-recoverable SDP forms where policy explicitly allows recovery
  - keep robustness improvements localized to existing raw media-section, per-attribute parser, and mapping/validation branches
  - avoid reshaping signaling model or bootstrap contracts for interoperability fixes
  - add regression tests for real-world SDP edge cases and captured samples

---

# Phase 6 — Windows port (optional, own backend only)

## Track M — Optional Windows support
- [ ] 300: Decide whether Windows port is worth doing after Linux result is stable
- [ ] 301: Fill Windows implementation behind the already-defined socket transport boundary
  - do not introduce the socket OS/platform abstraction впервые in the Windows phase
  - reuse the socket transport boundary introduced during Linux socket backend work
  - keep Windows support limited to the project’s own socket backend
- [ ] 302: Implement Winsock unicast receive path behind the existing socket transport boundary
- [ ] 303: Implement Winsock multicast join/leave behind the existing socket transport boundary
- [ ] 304: Build & run dump tool(s) on Windows
- [ ] 305: Evaluate whether OBS Windows plugin integration is worth the effort
- [ ] 306: Do not port MTL backend; Linux-only by design