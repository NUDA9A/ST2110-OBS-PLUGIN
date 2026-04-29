# ST2110-OBS-PLUGIN — Conventions

## Validation and parsing

- Parse strictly: malformed wire data must produce an explicit error, not silent coercion.
- Validate configs explicitly before use/start.
- Fallbacks/default values must be explicit at the call site or config construction layer, not hidden inside parsers or validation helpers.
- `Unsupported` should mean: the input is structurally valid enough to recognize, but this implementation does not support it yet.
- `InvalidValue` should mean: the input/config violates required constraints.
- `is_valid()` is only a convenience wrapper around `validate_*() == Error::Ok` and must not contain divergent logic.

## Extensibility

- Format-specific constraints should stay localized in helper functions or format switches.
- Adding a new format should usually require:
  - extending enum values;
  - adding a validation `case`;
  - adding tests;
  - not rewriting existing validation flow.

## API and header shaping

- Public and architectural headers should make modeled axes and support boundaries explicit.
- Helper boundaries that affect validation, dispatch, lifecycle, runtime support, or derived values should not remain implicit only in prose.
- If a new helper is required for a clean architecture boundary, it should usually exist as a named function / method / policy / adapter rather than as duplicated inline logic.
- Temporary support limitations should be expressed through explicit modeled or validation boundaries, not by silently omitting API surface that is already architecturally known to be needed.

## Assistant output format for API-bearing tasks

- When a task adds or changes production API, the assistant should provide a copy-ready API skeleton, not only a prose description.
- For each touched or newly introduced production file, the assistant should provide the file in a form that can be copied directly into the repository with:
  - all required includes;
  - `namespace st2110`;
  - all enums, structs, classes, free functions, and helpers relevant to the task;
  - complete method/function declarations with exact signatures.
- The default declaration form for methods and free helpers should be a declaration ending with `;`, written so the user can replace that `;` with a function body `{ ... }`.
- The assistant should not omit helper declarations that are part of the intended API or localized implementation boundary.
- When a new file is proposed, the assistant should provide the full file skeleton, not only the changed fragment.
- When an existing file is changed substantially, the assistant should prefer sending the full replacement file unless the user explicitly asked for a smaller diff.
- For every declared method/helper, the assistant should also describe the expected implementation behavior:
  - inputs and validation expectations;
  - success result / state changes / side effects;
  - failure behavior and returned errors;
  - invariants that must remain true after the call.
- Expected behavior should be specific enough that the user can implement the body without guessing intended semantics.
- The assistant should keep implementation bodies absent by default unless the user explicitly asks for full implementation code.