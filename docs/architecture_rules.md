# ST2110-OBS-PLUGIN — Architecture rules

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **MAY**, **Actually read**, **Fully grounded**, **MTL task**, **Primary repository**, **MTL reference repository**, and **Pinned MTL reference branch** inherit the meanings defined in `plan_rules.md`.
- **Architecture-aware step** = a mode step that designs, checks, validates, accepts, or reviews production architecture or standards-facing behavior.
- **Modeled axis** = an explicit enum / config field / type / policy / helper / boundary representing a known varying parameter of the system.
- **Support boundary** = explicit place where “recognized but not supported yet” is modeled and enforced.
- **Validation boundary** = explicit place where structural or semantic constraints are checked before use.
- **Derived value** = value that MUST be computed from other inputs instead of being hardcoded when the upstream inputs already model the needed information.
- **Temporary limit** = current MVP/phase restriction that is allowed only when modeled explicitly and kept localized.

## 1. Validation

- Parsing MUST be strict.
- Malformed wire data MUST return an explicit error.
- Silent coercion MUST NOT be used.
- Configs MUST be explicitly validated before use/start.
- Fallbacks/defaults MUST be explicit at the call site or config-construction layer.
- Fallbacks/defaults MUST NOT be hidden inside parsers or validators.
- `Unsupported` MUST mean “recognized but not supported yet”.
- `InvalidValue` MUST mean “violates required constraints”.
- `is_valid()` MUST be only a wrapper over `validate_*() == Error::Ok` and MUST NOT contain divergent logic.

## 2. Extensibility

Code MUST remain extensible.

Typical future support SHOULD require:
- adding enum/value coverage;
- adding switch / adapter / mapper branches;
- adding validation / projection / support cases;
- adding tests;
- not rewriting the pipeline.

Format-specific, backend-specific, and standards-specific constraints SHOULD stay localized in helpers, adapters, validators, or explicit switches.

Important helper boundaries MUST NOT exist only in prose.
If a helper is needed for a clean boundary, it SHOULD exist as a named function / method / policy / adapter.

## 3. Explicit modeled axes and boundaries

Assistant MUST keep explicit modeled axes / boundaries / derived values where relevant.

The following are mandatory examples and are NOT exhaustive:

- media kind;
- backend kind;
- pixel/storage format;
- `VideoScanMode`;
- packing mode;
- RTP payload type admission;
- completion semantics;
- clock / timestamp / timing / playout policy;
- packet size / `MAXUDP` policy;
- SDP/raw signaling mapping boundaries;
- video media-description properties;
- audio sampling rate / packet time / `samples_per_packet` derivation;
- audio conformance level / current support boundary;
- audio channel count / order / mapping;
- receiver capability / support policy.

Any known variable standard / signaling / runtime / backend parameter MUST be treated as a modeled axis or derived value unless clearly proven otherwise.

## 4. MVP limitations

MVP limitations are allowed only as explicit:

- modeled axes;
- support / validation boundaries;
- named policies / helpers / constants;
- localized `Unsupported` / `InvalidValue` branches;
- follow-up-backed temporary limits.

MVP limitations MUST NOT be encoded as:

- magic constants;
- unnamed literals;
- ad hoc branches without named boundaries;
- fixed values that should be derived from signaled/runtime inputs;
- omission of already-known architectural axes.

Example:
- audio `samples_per_packet` MUST be derived from `sampling_rate_hz` and `packet_time_us`, not hardcoded as `48`.

If an axis / boundary / dispatch already exists architecturally, fuller support SHOULD fill existing branches rather than redesign the architecture.

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