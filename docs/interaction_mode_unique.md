# ST2110-OBS-PLUGIN — Interaction mode: УНИКАЛЬНЫЙ

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully grounded**, **Copy-ready**, **MTL task**, **Primary repository**, and **Pinned MTL reference branch** inherit the meanings defined in `plan_rules.md`.

## 1. General mode rules

In mode `УНИКАЛЬНЫЙ` Assistant acts according to the explicit user request instead of the staged workflows of other modes.

This mode relaxes only the predefined workflow structure.
It does NOT relax:
- source authority rules;
- honesty about what was actually read;
- missing-material behavior;
- compact-context-first and pinned-branch requirements for MTL tasks and DistroAV tasks;
- architecture rules when the user asks for architecture-aware or standards-aware work.

## 2. Mandatory retained rules

Even in this mode Assistant MUST:

- follow `plan_rules.md`;
- follow `conventions.md`;
- follow `architecture_rules.md` whenever the request touches architecture, standards, validation, extensibility, runtime behavior, or acceptance;
- use authoritative sources in the required order;
- read the actual files required by the specific request;
- request missing required material before pretending the context is complete;
- for MTL tasks, first read:
  - `docs/mtl_context_index.md`;
  - `docs/mtl_task_context_map.md`;
  - the task-specific compact MTL context docs selected by that map;
- and then read the relevant original pinned MTL reference files only if the compact MTL context docs are insufficient for the current request;
- not read original MTL reference files for non-MTL requests;
- for DistroAV tasks, first read:
  - `docs/distroav_context_index.md`;
  - the relevant compact DistroAV context docs;
- and then read the relevant original pinned DistroAV reference files only if the compact DistroAV context docs are insufficient for the current request;
- not read original DistroAV reference files for non-DistroAV requests.

## 3. Output behavior

Assistant MAY choose the most suitable response shape for the user's request.

However:
- repository-facing updates MUST remain copy-ready when the user expects repository-ready output;
- full-file vs partial-block behavior MUST be stated clearly;
- Assistant MUST NOT hide whether the response is partial due to missing material.

## 4. Mode change

The user MAY switch out of `УНИКАЛЬНЫЙ` to any explicit structured mode at any time.