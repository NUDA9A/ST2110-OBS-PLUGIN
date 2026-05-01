# ST2110-OBS-PLUGIN — Conventions

> Этот файл дополняет `plan_rules.md`.  
> При конфликте приоритет у `plan_rules.md`.  
> Версия этого файла в Project является authoritative runtime copy.

## 1. Validation and parsing

- Parse strictly: malformed wire data must yield an explicit error, not silent coercion.
- Configs must be explicitly validated before use/start.
- Fallbacks/defaults must be explicit at the call site or config construction layer, not hidden in parsers/validators.
- `Unsupported` = input is recognizable but not supported by this implementation yet.
- `InvalidValue` = input/config violates required constraints.
- `is_valid()` is only a wrapper around `validate_*() == Error::Ok`; it must not contain divergent logic.

## 2. Extensibility

- Format-specific constraints should stay localized in helpers or explicit format switches.
- Adding a new format should usually require:
  - extending enum values;
  - adding a validation / dispatch case;
  - adding tests;
  - not rewriting the existing validation flow.

## 3. API and header shaping

- Public and architectural headers must make modeled axes and support boundaries explicit.
- Helper boundaries affecting validation, dispatch, lifecycle, runtime support, or derived values must not remain implicit only in prose.
- If a helper is needed for a clean architectural boundary, it should exist as a named function / method / policy / adapter.
- Temporary support limits must be expressed through explicit modeled / validation / support boundaries, not by silently omitting already-known API surface.

## 4. Assistant output format for API-bearing tasks

If a task changes production API / headers / helper boundaries or adds a production file, the assistant must provide copy-ready API skeletons.

Rules:
- new production file → provide the whole file;
- existing production `.hpp` → provide only new / changed declaration blocks;
- do not resend unchanged declarations or existing inline bodies;
- full replacement of an existing production header is allowed only if the user explicitly asks for it;
- declarations should end with `;` so the user can replace `;` with `{ ... }`;
- do not omit helper declarations that are part of the intended boundary;
- tests are always sent in full copy-ready form;
- implementation bodies stay absent by default unless the user explicitly asks for full implementation code.

For every declared or semantically changed method/helper, the assistant must describe:
- input and validation expectations;
- success result / state changes / side effects;
- failure behavior / returned errors;
- invariants after the call.

## 5. Planning and status hygiene

Default `plan.md` workflow:
- completed tasks are marked `[x]` where they are declared;
- tasks are not moved to `## Done` by default;
- the same task must not be duplicated both in place and in `## Done` unless the user explicitly wants that workflow.

Before proposing `plan.md` updates, the assistant must first determine the actual workflow from the file state and the user’s recent instructions.

## 6. Context discipline for task proposal and acceptance

Before proposing the next task, before accepting an implementation, and before saying that rules/work/context were studied, the assistant must actually read the current versions of:
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md` when tests / coverage / test selection matter;
- Project copies of `plan_rules.md` and `conventions.md`;
- relevant production/test code;
- all ST 2110 / RP 2110 PDFs uploaded in the Project.

The assistant must not rely only on memory, earlier snippets, earlier turns, backlog text, maps, or assumptions about unchanged content.

Before proposing the next task, the assistant must also check the actual code and any newer local code from chat to determine whether the task is already implemented.

If the task is already implemented:
- the assistant must not propose redundant implementation;
- the assistant must explicitly say that no additional production/test code is needed for that task.

If any required `.md`, relevant code, or standards PDF was not actually read for the current step, the assistant must not claim that the context was fully checked.

## 7. Deviations hygiene

Do not add a new `Spec notes / deviations` item if the missing behavior:
- is already known;
- is already covered by an existing task in `plan.md`;
- is not worsened or changed in nature by the current task.

In that case:
- reference the existing follow-up task;
- do not duplicate the deviation.

Add a new deviation only when the mismatch is genuinely new:
- not reflected in `plan.md`;
- not covered by an existing backlog item;
- or introduced as a new architectural / standards risk by the current change.