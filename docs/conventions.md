# ST2110-OBS-PLUGIN — Conventions

## 0. Definitions

- Project copy of this file = authoritative runtime copy.
- If this file conflicts with `plan_rules.md`, `plan_rules.md` wins.
- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Code check**, **Already implemented**, and **Copy-ready** inherit the meanings defined in `plan_rules.md`.

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

- Format-specific constraints SHOULD stay localized in helpers or explicit switches.
- Adding a new format SHOULD usually require:
  - extending enum values;
  - adding validation / dispatch cases;
  - adding tests;
  - not rewriting existing validation flow.

## 3. API shaping

- Public and architectural headers MUST make modeled axes and support boundaries explicit.
- Important helper boundaries MUST NOT exist only in prose.
- If a helper is needed for a clean boundary, it SHOULD exist as a named function / method / policy / adapter.
- Temporary support limits MUST be expressed through explicit modeled / validation / support boundaries.

## 4. Assistant output rules

If a task changes production API / headers / helper boundaries or adds a production file, Assistant MUST provide copy-ready API skeletons.

Rules:
- new production file → full file;
- existing production `.hpp` → only new / changed declaration blocks;
- full existing header replacement → only if explicitly requested;
- unchanged declarations MUST NOT be resent;
- existing inline bodies MUST NOT be resent;
- declarations intended for user implementation SHOULD end with `;`;
- required helper declarations MUST NOT be omitted;
- tests MUST be sent in full;
- implementation bodies MUST stay absent by default unless explicitly requested.

For every new or semantically changed method/helper, Assistant MUST describe:
- input / validation expectations;
- success result / state changes / side effects;
- failure behavior / returned errors;
- invariants after the call.

## 5. Planning and status hygiene

Default `plan.md` workflow:
- completed task → `[x]` where declared;
- no default move to `## Done`;
- no duplicate completion marking.

Before proposing status updates, Assistant MUST infer the actual workflow from the current file state and recent user instructions.

## 6. Context discipline

Before proposing the next task, before accepting an implementation, and before saying that rules/work/context were studied, Assistant MUST:
- actually read all required project control `.md` files;
- actually read relevant code;
- actually read all uploaded ST 2110 / RP 2110 PDFs.

Assistant MUST NOT rely only on:
- memory;
- earlier snippets;
- earlier turns;
- backlog text;
- maps;
- unchanged-file assumptions.

Before proposing the next task, Assistant MUST also perform a code check to determine whether the task is already implemented.

If the task is already implemented:
- Assistant MUST NOT propose redundant implementation;
- Assistant MUST explicitly say that no additional production/test code is needed.

If required `.md`, relevant code, or uploaded standards PDF was not actually read for the current step, Assistant MUST NOT claim that context was fully checked.

## 7. Deviations hygiene

Assistant MUST NOT add a new `Spec notes / deviations` item when:
- the limitation is already known;
- it is already covered by an existing task in `plan.md`;
- the current task does not worsen it or change its nature.

In that case Assistant MUST reference the existing follow-up task and MUST NOT duplicate the deviation.

A new deviation SHOULD be added only when the mismatch is genuinely new:
- not already reflected in `plan.md`;
- not already covered by backlog;
- or newly introduced by the current change.