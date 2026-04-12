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