# ST2110-OBS-PLUGIN — Architecture rules

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **MAY**, **Actually read**, **Fully grounded**, **Relevant standards set**, **Primary repository**, **MTL reference repository**, **Pinned MTL reference branch**, **DistroAV reference repository**, and **Pinned DistroAV reference branch** inherit the meanings defined in `plan_rules.md`.
- **Pipeline stage** = one stage in the project’s media flow whose inputs, outputs, and responsibility are explicit.
- **External ingress** = any boundary where raw or externally supplied data enters the project, including SDP text, backend/platform values, settings, file metadata, or network packets.
- **Internal typed model** = project-owned typed representation produced after external ingress parsing/projection.
- **Structural validation** = validation performed at ingress or at immediate model-construction boundaries to ensure the typed model is well-formed.
- **Projection boundary** = a typed translation boundary from one already-recognized typed model into another typed model or backend API structure.
- **Conversion boundary** = a typed conversion boundary from one already-built media representation into another project-facing media representation.
- **Support boundary** = an explicit dispatch point that routes a recognized modeled value into the concrete logic branch responsible for it.
- **Concrete logic branch** = the exact branch/function/object that implements behavior for one recognized value or combination of modeled-axis values.
- **Temporary limit** = a current localized support restriction that is explicit and not masquerading as permanent architecture.
- **Sender responsibility** = behavior that the project assumes has already been made standard-conformant by the sender or by upstream control/signaling logic.
- **Pipeline-guaranteed invariant** = a property that a downstream stage may trust because an upstream stage or dispatch contract already guarantees it.

## 1. Project goal

The project MUST be shaped around a correct end-to-end media pipeline.

Current long-term goal:
- receive ST 2110 video and audio over the network;
- reconstruct them correctly;
- keep the design open for synchronized audio+video playout;
- deliver the resulting media into OBS;
- later extend toward send capability without having to redesign the whole architecture.

The architecture MUST therefore avoid locking the project into:
- video-only forever;
- socket-only forever;
- receive-only forever;
- one payload/format only;
- one playout strategy only.

## 2. Canonical receive pipeline

The receive-side architecture MUST be described and evaluated as a sequence of explicit stages.

### 2.1 Current canonical stage order

1. orchestrator / OBS-facing composition obtains raw signaling;
2. raw SDP is parsed by ingress parsers into media models and `ParsedSdpStreamSet`;
3. orchestrator projects parsed media into receive bootstrap objects;
4. orchestrator selects the local receive policy / interface;
5. orchestrator constructs `ReceiveStartRequest`;
6. orchestrator projects request + settings into backend-specific start config;
7. orchestrator constructs the selected backend;
8. orchestrator starts the backend;
9. backend/runtime receives packets or frames and executes media delivery;
10. reconstructed media is delivered to sink / OBS.

Every architectural discussion SHOULD identify which stage owns the behavior being discussed.

### 2.2 One stage = one responsibility

Each pipeline stage MUST own one primary responsibility.

Examples:
- ingress parses and validates raw signaling;
- orchestrator dispatches between already-recognized typed choices;
- backend config projection maps typed project state into backend-specific runtime state;
- socket packet processing performs socket-specific receive/reorder/depacketize/reconstruct work;
- MTL backend wraps MTL runtime and frame/block retrieval;
- delivery/sink stages hand reconstructed media to OBS-facing consumers.

A stage MUST NOT re-own responsibilities that belong to earlier stages just because it is convenient.

## 3. Sender responsibility vs project responsibility

The project MUST clearly distinguish:
- what the sender/control plane is responsible for;
- what the project ingress is responsible for;
- what downstream typed pipeline stages may trust.

### 3.1 Project trust model for signaling

The project assumes that upstream signaling is standard-conformant in the ways that belong to sender/control-plane responsibility.

Therefore the project SHOULD NOT add defensive logic whose only purpose is to second-guess sender-side semantic coordination that the project architecture already assumes.

Examples of checks that SHOULD NOT become routine downstream architecture merely for paranoia:
- re-checking that duplicate media sections are semantically coordinated after the typed model already assumes standard-conformant duplicate signaling;
- re-checking media-kind dispatch results after the orchestrator already chose the media-specific path;
- re-checking backend-kind dispatch results after settings already chose the backend-specific path.

### 3.2 What the project still MUST validate

The previous rule does NOT remove ingress validation.

At external ingress the project MUST still validate what is necessary to safely construct typed models, including for example:
- syntactic parseability;
- numeric parseability;
- explicit bounded numeric constraints;
- token recognition where the model requires recognition;
- required fields for the typed model being built;
- structural validation of the resulting model.

Malformed or contradictory external input MUST fail explicitly at ingress.

### 3.3 Trust after typed construction

Once a value has been accepted into a trusted internal typed model:
- downstream stages SHOULD rely on pipeline-guaranteed invariants;
- repetitive self-validation of internally assembled typed objects MUST NOT become routine architecture;
- failures in internally produced typed values are implementation bugs, not a reason to normalize broad downstream validators.

## 4. Modeling rules

### 4.1 Common model before backend-specific projection

For a given media type, the project MUST have one common project model before backend-specific projection.

Socket limits MUST NOT narrow that common model.
MTL limits MUST NOT narrow that common model.
OBS handoff limits MUST NOT narrow that common model.

### 4.2 Known axes MUST stay modeled

Known standard/runtime/backend parameters MUST remain explicit modeled axes or derived values where relevant.

This includes, at minimum:
- media kind;
- backend kind;
- receive topology, including duplicate-stream topology when in scope;
- video scan mode;
- video packing / payload-format-related distinctions;
- RTP payload type admission;
- timing / timestamp / playout policy;
- packet-size / `MAXUDP` policy;
- audio sampling rate;
- audio packet time;
- audio channel count and channel order;
- backend-local runtime/session/device projection parameters;
- delivery capability vs receive capability.

### 4.3 Derived values MUST be derived

Values that are already implied by modeled inputs MUST be derived, not hardcoded.

Examples:
- audio `samples_per_packet` from sampling rate and packet time;
- backend-session parameters from the already-built typed request/config;
- duplicate-stream handling choices from typed receive-topology state.

## 5. InvalidValue and Unsupported

### 5.1 `InvalidValue`

`InvalidValue` MUST be used for malformed external input, contradictory ingress data, or explicit violation of a required ingress constraint.

### 5.2 `Unsupported`

`Unsupported` MUST mean:
- the value is recognized by the common project model;
- the architectural branch for that value exists;
- execution reached the concrete logic branch responsible for it;
- but the concrete logic for that branch is not implemented yet.

`Unsupported` MUST NOT be used to hide:
- incomplete common modeling;
- incomplete projection targets;
- incomplete conversion targets;
- helper uncertainty;
- broad downstream distrust of already-typed project state.

## 6. Orchestrator rules

The orchestrator is a composition and dispatch layer.

It MAY:
- choose the media-specific path;
- choose the backend-specific path from settings;
- translate between typed pipeline stages;
- assemble backend construction inputs.

It MUST NOT:
- own packet parsing;
- own media reconstruction;
- duplicate backend runtime logic;
- redefine common media truth.

If the orchestrator has already chosen a media-specific or backend-specific path, downstream callees MUST be allowed to rely on that contract instead of re-checking the same choice again.

## 7. Socket backend rules

The project `Socket` backend is the project’s own RTP/ST 2110 receive implementation.

It owns the packet-driven media path.

### 7.1 Common vs media-specific split

Common receive/runtime behavior SHOULD live in common socket backend layers such as the single-media base where applicable.

Media-specific socket backends SHOULD keep primarily media-specific behavior only.

If behavior is identical across socket video and socket audio single-media runtime, it SHOULD be moved into the common socket single-media base rather than duplicated.

### 7.2 Packet path ownership

The socket receive path MAY include project-local packet handling such as:
- packet parsing;
- reorder buffering;
- buffer-drain policy;
- depacketization;
- reconstruction;
- timestamp mapping;
- media delivery.

Those behaviors belong to the socket path, not to the common media model and not to the MTL path.

### 7.3 Duplicate stream

Duplicate-stream support MUST be modeled explicitly as receive topology.

It MUST NOT be treated as two unrelated happy-path streams glued together informally.

If duplicate stream support is incomplete, the incompleteness MUST remain localized in the concrete logic branch that handles duplicate topology.

## 8. MTL backend rules

The project `Mtl` backend MUST be a wrapper around MTL APIs.

It MUST NOT re-implement the project socket packet-processing stack on top of MTL.

In particular, the MTL backend MUST NOT route MTL-received media through project-local:
- reorder buffers;
- packet depacketizers;
- socket packet parsers;
- socket frame reconstructors.

The MTL path SHOULD do only what is actually needed around MTL:
- project-to-MTL config/session projection;
- MTL lifecycle ownership;
- frame/block retrieval from MTL;
- localized project mapping/conversion to project-facing frame/audio contracts;
- sink delivery;
- backend-local stats.

## 9. Delivery, sink, and playout boundaries

The project MUST distinguish between:
- network/session receive correctness;
- reconstruction correctness;
- playout/synchronization correctness;
- OBS handoff correctness.

A receive path being valid does not automatically mean that the current delivery/handoff boundary is wide enough for every recognized mode.

That gap MUST be represented as delivery/handoff/conversion scope, not as proof that the receive/session model is invalid.

## 10. Jitter-buffer placement rule

A future jitter buffer SHOULD be treated as a playout/synchronization boundary, not as packet-ingress logic.

Preferred architectural direction:
- packet-level recovery/reorder stays in the packet path where appropriate;
- jitter/pll/playout smoothing lives after media reconstruction, on the final delivery side;
- if audio and video are both present, the architecture SHOULD remain open to one shared synchronization/playout boundary rather than two permanently unrelated playout paths.

The architecture also MUST degrade cleanly when only one media type is present.

## 11. OBS and DistroAV rules

DistroAV is a reference source for:
- OBS plugin/module structure;
- OBS source/input lifecycle;
- frontend/config/UI patterns;
- NDI-related plugin-side reference behavior when explicitly needed.

DistroAV is NOT the project’s media-model truth.
DistroAV is NOT the project’s ST 2110 standards truth.

Use DistroAV-derived compact context first, and original pinned DistroAV files only when the compact docs are insufficient.

## 12. Future send direction

The receive-side architecture MUST NOT hardcode assumptions that make later send support structurally impossible or unnecessarily invasive.

Future send work is expected to include at least:
- media information extraction from files or other sources;
- SDP formation from typed media state;
- signaling of generated SDP through discovery/control integration;
- packetization and network send.

Therefore the project SHOULD preserve common typed modeling and pipeline boundaries that can later support both receive and send directions.

## 13. Standards-facing policy

When standards are relevant, the architecture MUST reflect the actual responsibility split implied by the standards set in scope.

Examples of standards-facing consequences that matter in this project:
- ST 2110-10 owns system timing, common RTP/SDP requirements, and duplicate-stream signaling;
- ST 2110-20 owns uncompressed video RTP payload and SDP metadata;
- ST 2110-21 owns sender traffic-shaping/timing models and is informative for receiver-side timing context, but does not define the project’s practical receiver implementation for jitter/playout;
- ST 2110-30 owns PCM audio constraints via AES67-compatible behavior;
- RP 2110-25 is about measurement/reporting practice, not a substitute for pipeline architecture.

Assistant MUST NOT turn a temporary MVP simplification into permanent architecture when the standards-aware model already shows the wider axis.

## 14. Temporary limits

Temporary limits are allowed only when they are:
- explicit;
- localized;
- attached to the correct concrete logic branch;
- not presented as the permanent shape of the architecture.

Temporary limits MUST NOT be encoded as:
- silent narrowing of the common model;
- helper-level `Unsupported` hiding missing architecture;
- magic literals where a modeled axis or derived value should exist;
- defensive re-validation of pipeline-guaranteed invariants.