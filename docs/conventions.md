# ST2110-OBS-PLUGIN — Conventions

## 0. Definitions

- Project copy of this file = authoritative runtime copy.
- If this file conflicts with `plan_rules.md`, `plan_rules.md` wins.
- If this file conflicts with `architecture_rules.md` on architecture meaning, `architecture_rules.md` wins.
- If this file conflicts with `interaction_mode_update_code_map.md` during an explicit code-map update request, the mode file wins for that request.
- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Copy-ready**, and **Fully grounded** inherit the meanings defined in `plan_rules.md`.

## 1. Copy-ready repository output

All repository-facing output MUST be copy-ready.

Default rules:
- new production file → full file;
- existing production file → only the necessary updated blocks unless the user explicitly asked for the full file or the whole file behavior is being reshaped;
- repository `.md` rewrite with global rule changes → full-file replacement is appropriate;
- unchanged declarations MUST NOT be resent unless a full updated structure is required;
- unchanged inline bodies MUST NOT be resent unless the user explicitly asked for implementation changes;
- file path MUST be stated for every returned repository block.

## 2. Grouping and labeling

Assistant SHOULD group repository-facing output by file.

For each returned block Assistant SHOULD label it clearly, for example:
- add;
- replace;
- update;
- insert into existing file;
- full-file replacement.

Assistant MUST NOT scatter one file’s required changes across disconnected fragments without making the insertion/replacement location explicit.

## 3. Documentation updates

For repository `.md` updates, Assistant SHOULD return replaceable blocks by default.

However, when the file’s meaning or rule set is globally reshaped, Assistant SHOULD return the full new file.

Typical cases:
- rule files rewritten from scratch → full-file replacement;
- one code-map entry → one full entry block;
- one plan subsection → one replaceable subsection block.

## 4. Response discipline

Assistant MUST keep these things separate in the response:
- confirmed file-grounded facts;
- design recommendations;
- unresolved questions or missing material;
- repository-ready blocks.

Assistant MUST NOT:
- hide missing reading;
- imply a full check when only partial material was read;
- present code-map descriptions as if they were code;
- merge unrelated files into one oversized pseudo-patch unless the user explicitly asked for that style.

## 5. Expected shape for code/design answers

Unless the user asks for a different format, Assistant SHOULD provide:
- a short architectural reading of the issue;
- exact scope of affected files;
- the recommended change shape;
- copy-ready code or document blocks when repository-facing output is requested.

## 6. Expected shape for review answers

When reviewing code or architecture, Assistant SHOULD separate:
- what is already correct;
- what is missing or wrong;
- what file(s) own the fix;
- whether the problem is ingress-validation, modeling, projection, conversion, backend logic, delivery logic, or OBS composition.

## 7. Tests

Assistant MUST NOT assume a tests workflow when tests are absent.

If tests are requested and exist:
- updating an existing test file is preferred over creating a new one, unless the subsystem split clearly requires a new file;
- a new test target, when truly required, MUST be provided as the exact build-system line plus the full new test file.

## 8. Large rule rewrites

When the user asks to fully reshape project rules, Assistant SHOULD:
- propose the new rule surface first if useful;
- return full replacement files for the affected rule documents;
- explicitly say which old rule files should be deleted or retired if they are no longer part of the active workflow.