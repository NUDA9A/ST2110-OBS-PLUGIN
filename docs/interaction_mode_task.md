# ST2110-OBS-PLUGIN — Interaction mode: ЗАДАЧА

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Code check**, **Already implemented**, **Copy-ready**, **MTL task**, **Production-file selection path**, and **Test-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Stage 1** = task-context formation and verbal execution-plan response.
- **Stage 2** = per-plan-step API/design response.
- **Stage 3** = implementation step for one selected plan step.
- **Stage 4** = full-plan production-implementation verification step.
- **Stage 5** = tests update step.
- **Plan step** = one numbered implementation step in the Stage 1 plan.
- **Plan completion** = the state in which all plan steps are either:
  - implemented;
  - explicitly recognized as already implemented;
  - or removed / merged / replaced by an updated plan agreed in the dialog.
- **Relevant standards set** = the subset of uploaded ST 2110 / RP 2110 PDFs that is actually needed for the current task scope.

## 1. General mode rules

Mode `ЗАДАЧА` is a staged workflow.

The stages are:

1. Stage 1 — context formation and verbal execution plan;
2. Stage 2 — API/design for one selected plan step;
3. Stage 3 — implementation of one selected plan step by the user or by the assistant when explicitly requested;
4. Stage 4 — production-implementation verification after full plan completion;
5. Stage 5 — tests update / test-fix loop.

Assistant MUST stay in this mode until the user explicitly changes the mode.

Assistant MUST NOT skip from Stage 1 directly to Stage 4 or Stage 5.
Assistant MUST NOT study tests before Stage 5 unless the user explicitly leaves this mode.
Assistant MUST NOT emit API for all plan steps at once.
Assistant MUST NOT move to Stage 4 until Plan completion is reached.

Inside one task, the normal loop is:

- Stage 1 — Assistant returns only a verbal plan;
- user either:
  - asks to change that plan;
  - or asks for API for one specific plan step;
- Stages 2 and 3 repeat step-by-step until Plan completion;
- only then Stage 4 may begin.

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

If the task is an MTL task, Assistant MUST first actually read:
- `docs/mtl_context_index.md`;
- `docs/mtl_task_context_map.md`;
- the task-specific compact MTL context docs selected by that map.

If the compact MTL context docs are insufficient for the current task step, Assistant MUST then actually read the relevant original MTL reference files from:
- repository `NUDA9A/Media-Transport-Library`;
- branch `mtl-ref-v26.01`.

Assistant MUST NOT read the entire configured original MTL reference set by default if the compact MTL context docs plus the relevant original files are already sufficient for the current step.

If the task is NOT an MTL task, Assistant MUST NOT read MTL compact context docs or original MTL reference files.

At Stage 1 Assistant MUST NOT:
- read `tests_file_map_index.md`;
- read `tests_file_map_shard_*.md`;
- read test `.cpp` files;
- reason from test coverage as if tests were already checked.

Before forming the plan, Assistant MUST perform a code check against the actual selected production files to determine whether the requested behavior is already implemented.

If the requested behavior is already implemented:
- Assistant MUST explicitly say that no additional production code is needed;
- Assistant MUST NOT propose a redundant implementation plan;
- Assistant MUST NOT propose redundant production API changes;
- Assistant MUST remain in `ЗАДАЧА` mode and wait for the user's next instruction.

If the requested behavior is only partially implemented:
- Assistant MUST separate already implemented scope from remaining work;
- Assistant MUST build the Stage 1 plan only for the remaining work.

## 3. Stage 1 — required response shape

Stage 1 response MUST contain:

- architecture requirements relevant to the task;
- exact scope of production-file changes;
- a numbered verbal plan containing only the remaining work needed to close the task;
- explicit file paths for every plan step;
- temporary support limits, if any;
- explicit note that production verification starts only after all plan steps are completed.

For each plan step, Assistant MUST describe in words:

- the goal of the step;
- the touched production files;
- the architectural boundary / responsibility of the step;
- the completion condition of the step.

Stage 1 response MUST be grounded only in:
- actually read rule files;
- actually read relevant production files;
- actually read relevant standards;
- actually read the compact MTL context docs required for an MTL task, and the relevant original MTL reference files only when the compact docs were insufficient for the current step.

Assistant MUST NOT present Stage 1 as test-grounded, because tests are intentionally excluded at this stage.

## 4. Stage 1 — verbal plan rules

At Stage 1 Assistant MUST return only a verbal plan.

Stage 1 response MUST NOT contain:

- copy-ready API blocks;
- declarations of structs / classes / helpers;
- full file skeletons;
- implementation bodies;
- method-by-method expected behavior blocks.

The plan MUST be structured so that:

- each step can later be requested individually by the user;
- each step is coherent and not artificially mixed with unrelated responsibilities;
- later Stage 2 output can be limited to one step without re-planning the whole task.

If the user asks to adjust the plan before Plan completion, Assistant MUST return an updated verbal plan for the remaining work.

When updating the plan, Assistant MUST NOT emit API unless the user explicitly asks for API for a specific plan step.

## 5. Stage 2 — per-plan-step API/design response

Stage 2 begins only when the user explicitly asks for API/design for one specific plan step.

At Stage 2 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_task.md`;
- production-map navigation sources through the Production-file selection path for the selected plan step:
  - `code_map_index.md`;
  - the relevant `code_map_shard_*.md`;
  - the actual relevant production files;
- the same relevant standards set that is required for the selected plan step, updated if the scope changed;
- newer local chat code changes, if they exist.

If the selected plan step is an MTL step, Assistant MUST first actually read:
- `docs/mtl_context_index.md`;
- `docs/mtl_task_context_map.md`;
- the task-specific compact MTL context docs selected by that map.

If the compact MTL context docs are insufficient for the current Stage 2 step, Assistant MUST then actually read the relevant original MTL reference files from:
- repository `NUDA9A/Media-Transport-Library`;
- branch `mtl-ref-v26.01`.

Assistant MUST NOT read the entire configured original MTL reference set by default if the compact MTL context docs plus the relevant original files are already sufficient for the current step.

If the selected plan step is NOT an MTL step, Assistant MUST NOT read MTL compact context docs or original MTL reference files.

At Stage 2 Assistant MUST NOT:
- read `tests_file_map_index.md`;
- read `tests_file_map_shard_*.md`;
- read test `.cpp` files;
- emit API for more than one plan step at once.

If the user asks to revise the plan instead of asking for API for one step:
- Assistant MUST return only the updated verbal plan;
- Assistant MUST NOT emit API in the same response unless the user also explicitly asks for API for one specific plan step.

If the selected plan step is already implemented:
- Assistant MUST explicitly say that no additional production code is needed for that step;
- Assistant MUST NOT invent redundant API changes;
- Assistant MUST wait for the user's next instruction.

## 6. Stage 2 — required response shape for one selected plan step

Stage 2 response MUST contain only the selected plan step.

Stage 2 response MUST contain:

- the selected plan step number / title;
- exact scope of production-file changes for that step only;
- copy-ready API updates grouped by file for that step only;
- expected behavior for every new or semantically changed method / helper / class / struct / policy / adapter in that step only;
- temporary support limits for that step, if any;
- explicit file paths for every change in that step.

Stage 2 response MUST NOT contain:

- API for any other plan step;
- implementation bodies unless the user explicitly asked to implement the selected step;
- test updates.

## 7. Stage 2 — API block rules for one selected step

Assistant MUST provide API in copy-ready form for the selected plan step only.

### 7.1 Structs

If a struct is new for the selected step:
- Assistant MUST provide the full struct in copy-ready form;
- all fields MUST be present.

If a struct already exists and needs to be changed for the selected step:
- Assistant MUST provide the full updated struct in copy-ready form;
- Assistant MUST explicitly say that the existing struct must be updated.

### 7.2 Classes

If a class is new for the selected step:
- Assistant MUST provide the full class declaration in copy-ready form;
- all fields MUST be present;
- all methods MUST be declared with argument names.

If a class already exists:
- existing method declarations MUST be resent only when their return type or argument list changes for the selected step;
- if an existing method body must later change but its declaration does not change, Assistant MUST NOT resend the unchanged declaration just for that reason;
- new fields MUST be provided as a copy-ready block containing only the new fields, with explicit instruction that they must be added;
- new methods MUST be provided as a copy-ready block containing only those method declarations, with explicit instruction that they must be added;
- if inheritance must change, Assistant MUST provide only the relevant class declaration line / header fragment needed to apply the inheritance update.

### 7.3 Helpers

Helpers MUST follow the same add/update rules as class methods.

For helper declarations Assistant MUST include required qualifiers where relevant, such as:
- `inline`;
- `constexpr`;
- `[[nodiscard]]`;
- `noexcept`.

If a helper is new for the selected step:
- provide its declaration block in copy-ready form.

If a helper already exists:
- resend only the changed declaration block when its signature/contract changes for the selected step;
- otherwise describe required body changes in the expected-behavior section.

### 7.4 New files

If a new production file is required for the selected step:
- Assistant MUST provide the full file in copy-ready form;
- the file MUST follow the same API/body rules of this mode.

## 8. Stage 2 — expected behavior requirements for one selected step

After the API blocks, Assistant MUST describe the expected implementation behavior for every new or semantically changed method / helper in the selected plan step.

For each such item Assistant MUST describe:

- inputs;
- validation boundary;
- success path;
- state changes / side effects;
- returned errors / failure behavior;
- invariants after the call;
- temporary support limits, if any.

Assistant MUST describe enough behavior so that implementing exactly that behavior will fully close the selected plan step without duplicated responsibility or hidden logic.

By default implementation bodies MUST NOT be provided at Stage 2.

## 9. Stage 3 — implementation step for one selected plan step

At Stage 3 one of two paths is allowed for the currently selected plan step.

### 9.1 User-implementation path

If the user implements the selected plan step themself, they MAY:

- ask to adjust the remaining plan;
- ask for API for the next plan step;
- ask the assistant to implement another selected plan step;
- or, after Plan completion, ask to begin Stage 4 verification.

Assistant MUST NOT treat the task as production-accepted merely because one plan step was implemented.

### 9.2 Assistant-implementation path

If the user explicitly asks the assistant to implement the selected plan step, the following is assumed:
- the user has already applied the API changes from Stage 2 for that selected plan step, if any;
- the assistant now needs to provide implementation bodies / concrete patches only for that selected plan step.

At this path Assistant MUST provide:

- for newly introduced methods/helpers in the selected plan step:
  - implemented bodies in copy-ready form;
- for already existing methods/helpers in the selected plan step:
  - only the necessary point changes for the implementation body;
- no redundant resend of unchanged declarations;
- no implementation for other plan steps.

After the user applies those changes, the workflow returns to:
- Stage 2 for the next requested remaining plan step;
- or Stage 4 if Plan completion has been reached and the user asks to begin verification.

## 10. Stage 4 — full-plan production implementation verification

Stage 4 begins only after Plan completion.

At Stage 4 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_task.md`;
- the same relevant standards set that is required for the task, updated if the scope changed;
- the actual changed production files from:
  - the primary repository if the changes are already there;
  - or newer local full-file versions provided by the user in chat;
- for an MTL task, the compact MTL context docs required for the verification step;
- and, if those compact docs are insufficient for the current verification step, the relevant original MTL reference files from:
  - repository `NUDA9A/Media-Transport-Library`;
  - branch `mtl-ref-v26.01`.

At Stage 4 Assistant MUST NOT:
- read tests maps;
- read test files;
- present the result as test-checked.

Stage 4 verification MUST check:

- Plan completion;
- compliance with the requested task;
- compliance with the final approved / updated plan;
- compliance with the selected relevant standards set;
- compliance with `architecture_rules.md`;
- compliance with the relevant original MTL reference material actually required/read for an MTL task, using the compact MTL context docs as task-scoping context;
- absence of duplicated responsibility or hidden fallback logic;
- absence of newly introduced undocumented deviations.

If the implementation is not acceptable:
- Assistant MUST describe the remaining corrections as a reduced verbal plan of the remaining work;
- Assistant MUST NOT emit API for those correction steps unless the user explicitly asks for API for one of them;
- Assistant SHOULD describe expected behavior more deeply for the problematic methods/helpers once the user requests API for the relevant correction step;
- the workflow returns to Stage 2.

If the production implementation is acceptable:
- Assistant MUST explicitly say that the production implementation is accepted for this mode stage;
- the workflow proceeds to Stage 5.

## 11. Stage 5 — tests update

At Stage 5 Assistant MUST actually read:

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

At Stage 5 Assistant MAY update tests iteratively for as long as the user keeps this mode active.

If the user later sends one failing test file and asks to update it, Assistant MUST return the full updated test file.

## 12. No implicit end

Mode `ЗАДАЧА` has no automatic final step.

It continues as long as the user remains in this mode:
- the verbal plan may be updated in Stage 1 / Stage 2;
- per-step API / implementation may loop through Stages 2 and 3;
- production verification may begin only after full Plan completion in Stage 4;
- tests may loop inside Stage 5.

If the user explicitly switches to another mode, the current mode ends at that point.