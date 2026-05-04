# ST2110-OBS-PLUGIN — Interaction mode: ЗАДАЧА

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Code check**, **Already implemented**, **Copy-ready**, **MTL task**, **Production-file selection path**, and **Test-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Stage 1** = task-context formation and API/design response.
- **Stage 2** = implementation step.
- **Stage 3** = production-implementation verification step.
- **Stage 4** = tests update step.
- **Relevant standards set** = the subset of uploaded ST 2110 / RP 2110 PDFs that is actually needed for the current task scope.

## 1. General mode rules

Mode `ЗАДАЧА` is a staged workflow.

The stages are:

1. Stage 1 — context formation and API/design response;
2. Stage 2 — implementation by the user or by the assistant when explicitly requested;
3. Stage 3 — production-implementation verification;
4. Stage 4 — tests update / test-fix loop.

Assistant MUST stay in this mode until the user explicitly changes the mode.

Assistant MUST NOT skip from Stage 1 directly to Stage 4.
Assistant MUST NOT study tests before Stage 4 unless the user explicitly leaves this mode.

## 2. Stage 1 — mandatory reading and context formation

At Stage 1 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_task.md`;
- production-map navigation sources through the Production-file selection path:
    - `code_map_index.md`;
    - the relevant `code_map_shard_*.md`;
    - the actual relevant production files;
- newer local chat code changes, if they exist.

Assistant MUST determine which uploaded standards PDFs are relevant to the task and then MUST actually read that Relevant standards set.

If the task is an MTL task, Assistant MUST also actually read all configured MTL reference files from:
- repository `NUDA9A/Media-Transport-Library`;
- branch `mtl-ref-v26.01`.

If the task is NOT an MTL task, Assistant MUST NOT read MTL reference files.

At Stage 1 Assistant MUST NOT:
- read `tests_file_map_index.md`;
- read `tests_file_map_shard_*.md`;
- read test `.cpp` files;
- reason from test coverage as if tests were already checked.

Before proposing changes, Assistant MUST perform a code check against the actual selected production files to determine whether the requested behavior is already implemented.

If the requested behavior is already implemented:
- Assistant MUST explicitly say that no additional production code is needed;
- Assistant MUST NOT propose redundant production API changes;
- Assistant MUST remain in `ЗАДАЧА` mode and wait for the user's next instruction.

## 3. Stage 1 — required response shape

Stage 1 response MUST contain:

- architecture requirements relevant to the task;
- exact scope of production-file changes;
- copy-ready API updates grouped by file;
- expected behavior for every new or semantically changed method / helper / class / struct / policy / adapter;
- temporary support limits, if any;
- explicit file paths for every change.

Stage 1 response MUST be grounded only in:
- actually read rule files;
- actually read relevant production files;
- actually read relevant standards;
- actually read MTL reference files for an MTL task.

Assistant MUST NOT present Stage 1 as test-grounded, because tests are intentionally excluded at this stage.

## 4. Stage 1 — API block rules

Assistant MUST provide API in copy-ready form.

### 4.1 Structs

If a struct is new:
- Assistant MUST provide the full struct in copy-ready form;
- all fields MUST be present.

If a struct already exists and needs to be changed:
- Assistant MUST provide the full updated struct in copy-ready form;
- Assistant MUST explicitly say that the existing struct must be updated.

### 4.2 Classes

If a class is new:
- Assistant MUST provide the full class declaration in copy-ready form;
- all fields MUST be present;
- all methods MUST be declared with argument names.

If a class already exists:
- existing method declarations MUST be resent only when their return type or argument list changes;
- if an existing method body must later change but its declaration does not change, Assistant MUST NOT resend the unchanged declaration just for that reason;
- new fields MUST be provided as a copy-ready block containing only the new fields, with explicit instruction that they must be added;
- new methods MUST be provided as a copy-ready block containing only those method declarations, with explicit instruction that they must be added;
- if inheritance must change, Assistant MUST provide only the relevant class declaration line / header fragment needed to apply the inheritance update.

### 4.3 Helpers

Helpers MUST follow the same add/update rules as class methods.

For helper declarations Assistant MUST include required qualifiers where relevant, such as:
- `inline`;
- `constexpr`;
- `[[nodiscard]]`;
- `noexcept`.

If a helper is new:
- provide its declaration block in copy-ready form.

If a helper already exists:
- resend only the changed declaration block when its signature/contract changes;
- otherwise describe required body changes in the expected-behavior section.

### 4.4 New files

If a new production file is required:
- Assistant MUST provide the full file in copy-ready form;
- the file MUST follow the same API/body rules of this mode.

## 5. Stage 1 — expected behavior requirements

After the API blocks, Assistant MUST describe the expected implementation behavior for every new or semantically changed method / helper.

For each such item Assistant MUST describe:

- inputs;
- validation boundary;
- success path;
- state changes / side effects;
- returned errors / failure behavior;
- invariants after the call;
- temporary support limits, if any.

Assistant MUST describe enough behavior so that implementing exactly that behavior will fully close the task without duplicated responsibility or hidden logic.

By default implementation bodies MUST NOT be provided at Stage 1.

## 6. Stage 2 — implementation step

At Stage 2 one of two paths is allowed.

### 6.1 User-implementation path

If the user implements the task themself, they MUST send all changed production files in full.

Assistant then proceeds to Stage 3.

### 6.2 Assistant-implementation path

If the user explicitly asks the assistant to implement, the following is assumed:
- the user has already applied the API changes from Stage 1;
- the assistant now needs to provide implementation bodies / concrete patches.

At this path Assistant MUST provide:

- for newly introduced methods/helpers:
    - implemented bodies in copy-ready form;
- for already existing methods/helpers:
    - only the necessary point changes for the implementation body;
- no redundant resend of unchanged declarations.

After the user applies those changes and sends all changed production files in full, Assistant proceeds to Stage 3.

## 7. Stage 3 — production implementation verification

At Stage 3 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_task.md`;
- the same relevant standards set that is required for the task, updated if the scope changed;
- for an MTL task, all configured MTL reference files;
- the actual changed production files from:
    - the primary repository if the changes are already there;
    - or newer local full-file versions provided by the user in chat.

At Stage 3 Assistant MUST NOT:
- read tests maps;
- read test files;
- present the result as test-checked.

Stage 3 verification MUST check:

- compliance with the requested task;
- compliance with the selected relevant standards set;
- compliance with `architecture_rules.md`;
- compliance with the MTL reference material for an MTL task;
- absence of duplicated responsibility or hidden fallback logic;
- absence of newly introduced undocumented deviations.

If the implementation is not acceptable:
- Assistant MUST describe what to change in the same output style as Stage 1, limited to the remaining corrections;
- Assistant SHOULD describe expected behavior more deeply for the problematic methods/helpers;
- the workflow returns to Stage 2.

If the production implementation is acceptable:
- Assistant MUST explicitly say that the production implementation is accepted for this mode stage;
- the workflow proceeds to Stage 4.

## 8. Stage 4 — tests update

At Stage 4 Assistant MUST actually read:

- `plan_rules.md`;
- `conventions.md`;
- this file `interaction_mode_task.md`;
- tests-map navigation sources through the Test-file selection path:
    - `tests_file_map_index.md`;
    - the relevant `tests_file_map_shard_*.md`;
    - the actual relevant test files;
- actual changed production files when needed for test design.

Assistant SHOULD prefer extending existing test files over creating new ones.

If a new test target is actually needed, Assistant MUST provide:
- the exact `add_st2110_test(...)` line;
- the full new test `.cpp`.

For every updated or new test file, Assistant MUST provide the full `.cpp` in copy-ready form.

At Stage 4 Assistant MAY update tests iteratively for as long as the user keeps this mode active.

If the user later sends one failing test file and asks to update it, Assistant MUST return the full updated test file.

## 9. No implicit end

Mode `ЗАДАЧА` has no automatic final step.

It continues as long as the user remains in this mode:
- production fixes may loop through Stages 2 and 3;
- tests may loop inside Stage 4.

If the user explicitly switches to another mode, the current mode ends at that point.