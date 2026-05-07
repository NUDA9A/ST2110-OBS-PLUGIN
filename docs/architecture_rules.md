# ST2110-OBS-PLUGIN — Architecture rules

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **MAY**, **Actually read**, **Fully grounded**, **MTL task**, **Primary repository**, **MTL reference repository**, and **Pinned MTL reference branch** inherit the meanings defined in `plan_rules.md`.
- **Architecture-aware step** = a mode step that designs, checks, validates, accepts, or reviews production architecture or standards-facing behavior.
- **Responsibility block** = a named architectural block with one primary responsibility, an explicit public boundary, and one-way dependencies on lower blocks only.
- **Modeled axis** = an explicit type / enum / field / variant / policy / helper / boundary representing a known varying parameter of the system.
- **Standard-defined axis** = a modeled axis whose value space is defined by the relevant standards or by an external API the project must integrate with.
- **External ingress** = a boundary where raw or externally supplied data enters the project, such as wire data, SDP text, media-file metadata, persisted settings, user-entered settings, deserialized state, or external library/API values.
- **Internal typed model** = a typed project representation produced after external ingress parsing/projection.
- **Logic branch** = an explicit function / method / object body / `switch` case / dispatch target that is responsible for executing the concrete behavior for one recognized value or one recognized combination of modeled-axis values.
- **Support boundary** = an explicit dispatch point that routes a recognized modeled value into its corresponding logic branch.
- **Projection boundary** = an explicit typed translation boundary that maps one already-recognized typed representation into another already-complete typed representation or into a backend/platform API structure without redefining or narrowing the source model.
- **Conversion boundary** = an explicit typed conversion boundary that maps already-received/constructed media from one already-complete project/backend representation into another already-complete project-facing representation.
- **Derived value** = a value that MUST be computed from other inputs instead of being hardcoded when the upstream inputs already model the needed information.
- **Temporary limit** = a current MVP/phase restriction that is allowed only when modeled explicitly and kept localized.

## 1. Ingress validation and internal trust model

- Parsing MUST be strict.
- Malformed wire data MUST return an explicit error.
- Silent coercion MUST NOT be used.
- External ingress MUST validate raw or externally supplied data before creating or accepting an internal typed model.
- External ingress includes:
    - wire data;
    - SDP/text parsing;
    - user-entered settings;
    - persisted/deserialized settings or state;
    - media-file metadata and format descriptions;
    - backend/platform/FFI values that originate outside trusted project code.
- `InvalidValue` MUST mean:
    - malformed external input;
    - contradictory external input;
    - or violation of an explicit required constraint at an ingress boundary.
- `Unsupported` MUST mean:
  - the value is recognized by the internal typed model;
  - the corresponding logic branch exists architecturally;
  - and execution has reached the concrete logic branch responsible for that value;
  - but the body of that concrete logic is not implemented yet.
- `Unsupported` MUST NOT be used as:
  - a substitute for incomplete target modeling;
  - a substitute for incomplete projection modeling;
  - a substitute for incomplete conversion modeling;
  - a substitute for helper uncertainty;
  - a substitute for enum/domain validation;
  - or a substitute for current project architecture being too narrow to express a recognized value.
- Projection boundaries MUST translate between already-complete typed models.
- Conversion boundaries MUST translate between already-complete typed models.
- Projection/conversion helpers MUST dispatch into logic branches when behavior differs by modeled axis.
- If a recognized value cannot be represented at a projection/conversion target, that is an architectural/modeling incompleteness, not a routine `Unsupported` case.
- `Unsupported` is allowed only from the concrete logic branch that is supposed to perform the behavior for the recognized value.
- After a value has been converted into a closed internal typed model by trusted project code, generic enum-validation helpers MUST NOT be used as routine architecture.
- Standalone validators such as `validate_enum_xxx(...)` over closed internal enums MUST NOT exist unless that exact boundary still consumes untrusted/deserialized/FFI values that may be out of domain.
- Invalid internal typed values produced only by project code are implementation bugs and MUST be fixed at the source that produced them, not normalized into routine runtime validation.
- Internally built configs/objects MUST NOT be broadly revalidated only to prove self-consistency when they are assembled entirely from trusted typed values.
- Fallbacks/defaults MUST be explicit at the call site or config-construction layer.
- Fallbacks/defaults MUST NOT be hidden inside parsers, enum validators, or unrelated helpers.
- `is_valid()` MUST be only a wrapper over the file’s chosen validation entry point and MUST NOT contain divergent logic.

## 2. Responsibility-block architecture

The repository architecture MUST remain organized as explicit responsibility blocks.

Current top-level blocks are:

- support/build and entrypoints;
- foundation;
- standard media model;
- external ingress;
- receive session contracts;
- delivery and conversion;
- common receive processing;
- socket platform adapters;
- socket backend;
- MTL backend;
- OBS plugin composition.

Required rules:

- Every production file MUST belong to exactly one primary responsibility block.
- A production file MUST NOT combine responsibilities from different blocks unless it is an explicit temporary compatibility file that is named as such and scheduled for removal.
- Lower blocks MUST NOT depend on higher blocks.
- Foundation MUST NOT know media, backend, platform, or OBS details.
- Standard media model MUST NOT know project delivery/storage limits, backend-specific support policy, socket runtime details, MTL runtime details, or OBS plugin details.
- External ingress MUST translate raw external input into internal typed models and MUST NOT own backend runtime logic.
- Receive session contracts MUST define backend-facing/session-facing contracts and MUST NOT own packet parsing, platform socket code, or MTL runtime code.
- Delivery/conversion MUST define project-local storage/handoff/conversion boundaries and MUST NOT redefine standards models.
- Common receive processing MUST implement backend-agnostic receive/runtime logic and MUST NOT depend on platform socket code or OBS plugin code.
- Socket platform adapters MUST implement OS/runtime transport details only and MUST NOT know media standards logic beyond what is required by the transport contract.
- Socket backend MUST consume lower-block contracts and processing logic; it MUST NOT redefine the standard media model.
- MTL backend MUST consume lower-block contracts and processing logic; it MUST NOT replace the common media model with an MTL-private truth.
- OBS plugin composition MUST remain the top composition layer and MUST NOT know internal backend/platform implementation details beyond public contracts.
- Responsibility blocks MUST be weakly coupled.
- A block MUST interact with other blocks only through explicit public contracts/boundaries.
- A block MUST NOT depend on the internal types, helper functions, constants, or implementation details of a sibling block.
- Cross-block knowledge MUST flow through lower/common contracts, not through direct sibling-to-sibling implementation coupling.
- If two blocks need the same concept, that concept MUST live in the appropriate lower/common block rather than be duplicated or imported from one sibling into another.

## 3. Standard-defined axes, modeled coverage, and localized missing logic

Assistant MUST keep explicit modeled axes / boundaries / derived values where relevant.

The following are mandatory examples and are NOT exhaustive:

- media kind;
- backend kind;
- all standard-defined video media-description axes;
- all standard-defined audio media-description axes;
- `VideoScanMode`;
- packing mode;
- transport payload format;
- RTP payload type admission;
- completion semantics;
- clock / timestamp / timing / playout policy;
- packet size / `MAXUDP` policy;
- SDP/raw signaling mapping boundaries;
- audio sampling rate / packet time / `samples_per_packet` derivation;
- audio channel count / order / mapping;
- receive topology;
- backend-local runtime/device/session projection boundaries;
- receive-session capability versus project delivery / handoff / conversion capability.

Any known variable standard / signaling / runtime / backend parameter MUST be treated as a modeled axis or derived value unless clearly proven otherwise.

### 3.1 Standard-defined axes are authoritative

- The relevant standards and required external APIs define which axes exist.
- The project’s typed model MUST be able to express the full value space of those standard-defined axes that are in scope for the project.
- The current implementation status of Socket, MTL, delivery contracts, or OBS integration MUST NOT shrink the project’s common typed model.
- If a standard allows open-ended values/tokens, the model SHOULD preserve them explicitly through raw-token / `Other`-style representation rather than erasing them.

### 3.2 Common media model across backends

For a given media type, the recognized receive/session description MUST be common across backends.

This means:

- common signaling/runtime/media modeling MUST describe what the project can represent from the standard;
- backend-specific code MUST consume that common model rather than define a separate private truth for what is recognized;
- Socket implementation limits MUST NOT narrow the common model;
- MTL-specific device/runtime/session configuration MAY be backend-local, but it MUST NOT replace the common media model.

### 3.3 Project responsibility is full modeling plus concrete-logic incompleteness only

- The project MUST model all in-scope standard-defined axes completely.
- Every typed target representation used by the architecture MUST also be complete enough for the in-scope branches that the project intends to route into it.
- The project MAY temporarily lack the body of some concrete logic branches.
- Such temporary incompleteness MUST appear only as missing concrete logic inside an explicit logic branch.
- A branch MUST NOT become “non-existent” in the architecture merely because its logic is not implemented yet.
- A projection/helper/conversion layer MAY dispatch into concrete logic branches.
- Only those concrete logic branches MAY temporarily return `Unsupported`.
- A generic projection/helper/conversion layer MUST NOT return `Unsupported` merely because the helper itself does not know how to continue.
- If the architecture cannot express a recognized value at the next typed boundary, that is project incompleteness and MUST be fixed by completing the model/boundary rather than normalized as helper-level `Unsupported`.
- Extending support SHOULD mostly mean:
  - adding the missing concrete logic;
  - adding tests;
  - not redesigning the whole architecture.

### 3.4 Receive capability versus delivery capability

The ability to receive or construct a backend/session path MUST remain separate from the ability to expose the result through current project delivery/handoff contracts.

Examples include:

- current `VideoFrameView` / `PixelFormat` limitations;
- current `AudioBuffer` / `AudioFrameView` / storage-layout limitations;
- current OBS handoff limitations;
- current conversion-helper availability.

A backend/session path MUST NOT be rejected early only because the current project handoff contract is narrower, unless that delivery/handoff/conversion boundary is itself the active branch being checked.

### 3.5 Backend-specific limits and concrete-logic-local `Unsupported`

If a format, mode, or combination is represented by the common model but the selected backend/runtime/delivery/storage path does not implement it yet, failure MUST occur only at the concrete logic branch that is responsible for that exact behavior.

In particular:

- Socket gaps MUST be represented as missing Socket logic, not as proof that the common model is invalid or unknown.
- MTL backend support MUST be evaluated against the same common model and then carried through explicit MTL-local logic branches that wrap the relevant MTL API path.
- Delivery/storage/handoff gaps MUST remain missing delivery/storage/handoff logic, not be reported as generic invalidity.
- A generic mapper/projection/helper MUST NOT become the place where recognized values are rejected with `Unsupported` merely because the actual logic branch has not been written yet.
- A default `switch` / `if` fallthrough to `Unsupported` is acceptable only when that fallthrough is itself the concrete logic branch for the recognized value at that exact layer.
- Preferred structure is:
  - dispatch by modeled axis;
  - one concrete logic function/object per branch;
  - `Unsupported` only from the concrete logic that is supposed to implement that branch.

### 3.6 MTL backend completeness relative to MTL ST 2110 capability

- The project `Mtl` backend MUST be architected as a wrapper around the relevant MTL ST 2110 receive APIs, not as an independent narrowing truth for media/session capability.
- For in-scope ST 2110 behavior that is expressible and implementable through the chosen MTL API surface, the project’s common model and MTL backend path MUST be able to express, route, and execute that behavior.
- Selecting the project `Mtl` backend MUST mean that the user can reach the relevant in-scope ST 2110 capability that MTL itself can realize through the chosen MTL API surface.
- If MTL can express/implement an in-scope ST 2110 mode but the project cannot reach it because the project model, project delivery model, or project projection target is too narrow, that is project incompleteness and MUST NOT be normalized as acceptable narrowing.
- Missing support in the MTL backend MUST therefore mean only:
  - the common model already represents the branch;
  - the MTL-facing path already has the right typed boundary;
  - and the remaining gap is only the missing concrete branch logic that wraps the corresponding MTL capability.
- The project MUST NOT intentionally keep the MTL backend narrower than the relevant in-scope MTL ST 2110 capability merely because another backend or current project handoff contract is narrower.

## 4. MVP limitations and anti-patterns

MVP limitations are allowed only as explicit:

- modeled axes;
- named concrete logic branches;
- named policies / helpers / constants;
- localized `Unsupported` / `InvalidValue` branches;
- follow-up-backed temporary limits.

MVP limitations MUST NOT be encoded as:

- magic constants;
- unnamed literals;
- ad hoc branches without named boundaries;
- fixed values that should be derived from signaled/runtime inputs;
- omission of already-known architectural axes;
- global narrowing of the common model to current backend behavior;
- intentional narrowing of the MTL path below relevant in-scope MTL ST 2110 capability because of unrelated project limits;
- generic enum-validation theater over closed internal enums;
- duplicated truth represented in multiple intermediate configs that are then kept consistent only through repetitive validators;
- helper-level `Unsupported` used as a substitute for incomplete target modeling or incomplete concrete logic;
- partial projection/conversion targets that cannot represent recognized in-scope values.

Required discipline:

- one branch = one responsibility;
- one source of truth = one place;
- one modeled axis value = one explicit concrete logic path;
- if a branch is not implemented, `Unsupported` MUST appear exactly in that concrete logic path;
- if a value came from raw external input, validate it at ingress;
- if a value was created by trusted project code inside the typed model, do not paper over design bugs with routine enum validators;
- if a projection/conversion target cannot represent a recognized value, complete that model/boundary instead of reporting helper-level `Unsupported`;
- if a `switch`/dispatch exists for a modeled axis, the per-case bodies or per-case callees are the places where temporary missing logic may exist.

Example:
- audio `samples_per_packet` MUST be derived from `sampling_rate_hz` and `packet_time_us`, not hardcoded as `48`.

If an axis / boundary / dispatch already exists architecturally, fuller support SHOULD fill the existing branch rather than redesign the architecture.

## 5. Standards and deviations

Assistant MUST treat every relevant task or review as potentially standards-relevant.

Assistant MUST NOT:
- assume a scope is too small to require standards consideration;
- lock in an avoidable standards deviation;
- turn an MVP limit into architecture;
- replace a known modeled axis / support boundary / validation boundary / derived value with a hardcoded assumption.

Avoidable standards deviations MUST NOT be accumulated intentionally.

If a standards deviation can be fixed now without unreasonable scope growth, it SHOULD be fixed now.

Otherwise it MUST be:
- explicitly named;
- localized;
- tied to an existing or new follow-up task where applicable.

## 6. Deviation hygiene

Assistant MUST NOT add a new deviation item when:
- the limitation is already known;
- it is already covered by an existing task in `plan.md`;
- the current change does not worsen it or change its nature.

In that case Assistant MUST reference the existing follow-up task and MUST NOT duplicate the deviation.

A new deviation SHOULD be added only when the mismatch is genuinely new:
- not already reflected in `plan.md`;
- not already covered by backlog;
- or newly introduced by the current change.

## 7. Phase goals

- **MVP** = minimal viable ST 2110 video/audio receive on Linux, two backends, basic OBS integration, manual E2E readiness.
- **Medium** = broader format coverage, robustness, edge-cases, UX/observability, testing readiness.
- **Plugin** = stable and usable OBS plugin behavior.
- **Tests** = systematic regression and coverage.
- **Hardening** = performance, recovery, correctness polish.
- **Windows** = optional port of own socket backend without MTL.

## 8. Build, dependency, and platform policy

Project build and dependency responsibilities MUST remain separated:

- project CMake builds this repository;
- project CMake may discover and link already installed external dependencies;
- project CMake MUST NOT clone, patch, build, or install DPDK, MTL, OBS, or other heavy system/runtime dependencies.

External dependency setup belongs to explicit setup/install scripts, not to production code and not to project CMake internals.

For Linux MVP/plugin builds:

- Linux is the primary MVP target;
- the Linux plugin build is expected to include both the project socket backend and the MTL backend;
- MTL is a required installed dependency for Linux MTL-capable builds;
- MTL discovery should be localized to build/dependency wiring, preferably through the installed `pkg-config` package `mtl`;
- DPDK/MTL installation, hugepages setup, dynamic linker visibility, and pkg-config path setup belong to the Linux setup/install script.

For Windows:

- Windows support is optional and limited to the project’s own socket backend unless MTL support is explicitly re-evaluated later;
- Windows builds MUST NOT require MTL, DPDK, or MTL headers/libraries;
- Windows plugin/backend selection MUST NOT expose MTL as a normal selectable backend when MTL is not compiled.

Backend concepts MUST remain distinct:

- the project `Socket` backend is the project’s own socket-based RTP/ST 2110 receive implementation;
- the project `Mtl` backend is the backend that consumes Media Transport Library APIs;
- MTL internal data-path choices such as DPDK PMD, MTL kernel socket, or AF_XDP are MTL runtime/configuration modes and MUST NOT be conflated with the project `Socket` backend.

Setup scripts MUST remain orchestration layers:

- a Linux setup/build/install script may install packages, build/install DPDK, build/install MTL, configure runtime prerequisites, build this repository, and install the OBS plugin artifact;
- local developer scripts such as `scripts/build_and_test.sh` remain convenience test runners and MUST NOT become full clean-machine installers;
- production/runtime code MUST NOT depend on assumptions that only hold because of an interactive local setup step unless that assumption is represented through explicit build/runtime validation.