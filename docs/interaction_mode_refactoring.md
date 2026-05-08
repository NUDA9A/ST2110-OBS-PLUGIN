# ST2110-OBS-PLUGIN — Interaction mode: РЕФАКТОРИНГ

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Code check**, **Already implemented**, **Copy-ready**, **Phase R**, **Refactoring block**, **Refactor task**, **Phase R exit condition**, **MTL task**, **DistroAV task**, **Production-file selection path**, and **Test-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Current block** = the Refactoring block currently being processed in this mode.
- **Current task** = the current Refactor task inside the Current block.
- **Block responsibility** = the single primary responsibility that the Current block must own after refactoring.
- **Neighbor block** = a lower/common block or higher/composing block that interacts with the Current block only through explicit public contracts/boundaries.
- **Relevant standards set** = the subset of uploaded ST 2110 / RP 2110 PDFs actually needed to refactor the Current block or Current task correctly.
- **Task destination file** = the file that must receive logic moved out of the Current task’s source file in order to restore correct responsibility ownership.
- **Task-local acceptance** = the state where the Current task is complete and the affected production files now match the required responsibility split without losing required production logic.
- **Block-local acceptance** = the state where the Current block is complete, coherent, weakly coupled to other blocks, and does not need another architectural reshaping before later functional work continues on top of it.

## 1. General mode rules

Mode `РЕФАКТОРИНГ` is for refactoring one responsibility block from `Phase R` at a time.

The user provides one Refactoring block. Assistant MUST treat that block as part of `Phase R`, not as an ordinary feature-implementation task.

Assistant MUST always keep in view:

- the purpose of `Phase R`;
- the Current block responsibility;
- the way the Current block depends on lower/common blocks and is consumed by higher blocks;
- the global `Phase R exit condition`.

Assistant MUST understand that the goal is not merely to move code mechanically. The goal is to make the Current block a real standalone responsibility block that:

- owns only its own responsibility;
- uses lower/common contracts instead of sibling internals;
- preserves important already-implemented production logic;
- does not require another architectural reshaping later just to become usable.

Assistant MUST process the Current block one Refactor task at a time.

Assistant MUST NOT:

- refactor several unrelated Refactor tasks at once unless the user explicitly asks for that;
- “solve” the task by introducing a new mixed bucket file;
- delete important production logic just because it is currently misplaced;
- normalize architectural incompleteness into helper-level `Unsupported`;
- narrow modeled standard axes to current Socket MVP limits or current delivery limits.

If the user-provided task order is workable, Assistant MUST keep it.

If the actual repository state requires a stricter prerequisite order, Assistant MUST:

- state that explicitly;
- explain which task must move earlier and why;
- still keep the discussion scoped to the Current block.

## 2. Required architectural understanding in this mode

Before working on a Refactoring block, Assistant MUST understand all of the following:

- what the Current block is supposed to own after refactoring;
- which files belong to that block;
- which lower/common public contracts it may consume;
- which higher/composing blocks may consume it;
- which current files are mixed-responsibility files or compatibility leftovers;
- which already-implemented logic must be preserved and relocated rather than dropped.

Assistant MUST treat `architecture_rules.md` as mandatory in this mode.

In particular, Assistant MUST enforce that:

- every production file belongs to exactly one primary responsibility block;
- responsibility blocks remain weakly coupled;
- common modeled axes remain complete and are not narrowed by current implementation limits;
- `Unsupported` remains only as missing concrete logic inside the exact branch-local concrete logic that is supposed to implement that recognized value;
- generic enum-validation theater over closed internal enums does not remain in routine architecture;
- moved logic lands in the correct destination block rather than in a new side bucket.

## 3. Standards/context obligations in this mode

Assistant MUST determine which standards/context are relevant to the Current block and Current task.

The standards/context decision MUST be block-aware.

Examples:

- foundation/support/build tasks may require no standards PDFs, but Assistant MUST determine that explicitly rather than assume it silently;
- standard media model / ingress / receive / contracts tasks often require ST 2110, RFC 3550, RFC 4175, and RP 2110-25 depending on scope;
- MTL backend tasks are MTL tasks and MUST follow the MTL compact-context + original-reference workflow;
- OBS/plugin composition tasks that rely on DistroAV as reference are DistroAV tasks and MUST follow the DistroAV context workflow.

If the Current block or Current task is an MTL task, Assistant MUST first actually read:

- `docs/mtl_context_index.md`;
- `docs/mtl_task_context_map.md`;
- the task-specific compact MTL context docs selected by that map.

If those compact MTL docs are insufficient, Assistant MUST then actually read the relevant original MTL reference files from:

- repository `NUDA9A/Media-Transport-Library`;
- branch `mtl-ref-v26.01`.

If the Current block or Current task is a DistroAV task, Assistant MUST first actually read:

- `docs/distroav_context_index.md`;
- the relevant compact DistroAV context docs.

If those compact DistroAV docs are insufficient, Assistant MUST then actually read the relevant original DistroAV reference files from:

- repository `NUDA9A/DistroAV`;
- branch `distro-av-ref-v6.2.1`.

Assistant MUST NOT read irrelevant standards/context just for volume. The set must be relevant and explicit.

## 4. Stage 1 — block context formation

Stage 1 begins when the user provides the Refactoring block or asks to begin working through it.

At Stage 1 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_refactoring.md`;
- `plan.md` if needed to confirm the Current block wording or the global `Phase R exit condition`;
- production-map navigation sources through the Production-file selection path:
    - `code_map_index.md`;
    - the shard(s) for the Current block and clearly relevant neighbor blocks;
    - the actual relevant production files of the Current block;
    - the actual lower/common contract files and neighbor files needed to understand correct destination boundaries;
- newer local chat code changes, if they exist;
- the Relevant standards set for the Current block;
- relevant MTL compact/original context if the block is an MTL task;
- relevant DistroAV compact/original context if the block is a DistroAV task.

At Stage 1 Assistant MUST determine:

- the Block responsibility;
- the list of Current block files;
- the lower/common blocks it may depend on;
- the higher/composing blocks that will later consume it;
- which existing files are legacy buckets / mixed-responsibility / compatibility leftovers;
- whether any user-listed Refactor tasks are already implemented;
- whether any user-listed Refactor tasks must be reordered for dependency reasons.

At Stage 1 Assistant SHOULD NOT read tests yet unless the user explicitly asks to handle test consistency immediately in parallel.

## 5. Stage 1 — required response shape

Stage 1 response MUST contain:

- a concise statement of the Current block responsibility;
- the Current block’s allowed dependencies and consumers;
- the exact production files that belong to the block;
- any clearly relevant external destination files that may receive moved logic;
- the Relevant standards/context that govern the block;
- the ordered list of Refactor tasks for the block:
    - preserving user order when valid;
    - or explicitly stating the required reorder and why;
- explicit note that the block is being refactored toward the global `Phase R exit condition`, not just local cosmetic cleanup.

Stage 1 MUST NOT emit implementation bodies unless the user explicitly asks to implement the Current task.

## 6. Stage 2 — one-task design/refactoring plan

Stage 2 begins when the user asks for the Current task or asks for the next task in the Current block.

At Stage 2 Assistant MUST actually read:

- the rule files required by this mode;
- the actual source file of the Current task;
- the actual destination file(s) if logic may need to move;
- the actual lower/common contract files needed to place the moved logic correctly;
- the actual higher/composing file(s) only if needed to preserve correct public compatibility;
- the Relevant standards/context for the Current task;
- newer local chat code changes, if they exist.

For the Current task, Assistant MUST determine all of the following:

- what responsibility the source file should retain after refactoring;
- what code must stay in the source file;
- what code must leave the source file;
- where that moved code must go;
- whether a new file is required;
- whether any compatibility file becomes deletable after this task;
- whether any already-implemented logic is at risk of disappearing and therefore must be preserved explicitly.

If the Current task is already implemented, Assistant MUST say so explicitly and MUST NOT invent extra changes.

## 7. Stage 2 — required response shape for one task

Stage 2 response MUST contain only the Current task.

It MUST contain:

- the Current task number/title;
- the exact file path(s) that will change;
- the responsibility of each changed file after the task;
- what logic remains in the source file;
- what logic moves out;
- the exact destination file(s) for moved logic, with the reason each destination is architecturally correct;
- whether any file must be created;
- whether any file must be deleted at the end of this task;
- copy-ready API/header/file-layout updates for the Current task only, when such API changes are needed;
- expected behavior / invariants after the refactor.

Stage 2 MUST NOT contain:

- implementation for later tasks;
- mixed changes for several unrelated tasks;
- vague “move somewhere else later” guidance.

If code leaves the source file, Assistant MUST name its destination now. It MUST NOT postpone the architectural placement question.

## 8. Stage 3 — one-task implementation

Stage 3 begins when the user explicitly asks Assistant to implement the Current task.

At Stage 3 Assistant MUST provide copy-ready production-code changes for the Current task only.

Implementation rules:

- if a new file is required, provide the full new file;
- if an existing file is heavily reshaped, prefer the full updated file;
- if an existing file needs only localized edits, exact copy-ready replacement blocks are acceptable;
- if code moves from one file to another, Assistant MUST provide both sides of the move in the same task response;
- if a file becomes unnecessary after the move and no remaining explicit responsibility justifies it, Assistant MUST say that the file must be deleted in this task.

Assistant MUST preserve important already-implemented logic.

Assistant MUST NOT:

- drop working production logic merely because it is misplaced;
- keep duplicated copies of the same logic across old and new locations unless a temporary compatibility step is explicitly justified by the Current task;
- create a forwarding shim / compatibility header / inert stub TU unless the Current task explicitly leaves one documented remaining responsibility there.

If a moved piece of logic does not belong in any existing file, Assistant MUST create the correct destination file in the correct responsibility block instead of inventing a mixed helper bucket.

## 9. Task-local acceptance rule

A Refactor task is acceptable only if, after its implementation:

- the changed file(s) each have one clear primary responsibility;
- moved logic is present in the correct destination file(s);
- no important already-implemented production logic disappeared;
- no new mixed-responsibility bucket was introduced;
- no new sibling-internal dependency was introduced;
- the repository is closer to the global `Phase R exit condition`.

If those conditions are not met, Assistant MUST NOT present the task as complete.

## 10. Stage 4 — block completion verification

Stage 4 begins only after all Refactor tasks of the Current block are completed or explicitly recognized as already implemented.

At Stage 4 Assistant MUST actually read:

- the rule files required by this mode;
- all changed production files in the Current block;
- all destination files that received moved logic;
- relevant lower/common contract files;
- relevant higher/composing files when needed to verify that the block is consumable through public boundaries;
- the Relevant standards/context for the block;
- relevant MTL compact/original context for an MTL block;
- relevant DistroAV compact/original context for an OBS/plugin block.

Stage 4 verification MUST check that:

- the Current block now has one coherent responsibility;
- every file in the block clearly belongs to that block;
- no leftover mixed “bucket” files remain in that block;
- no important existing production logic disappeared;
- remaining compatibility files, if any, each have one explicit documented remaining responsibility;
- the block consumes lower/common contracts rather than sibling internals;
- higher blocks can later consume this block without another architectural reshaping of the block itself;
- the Current block moves the repository toward the global `Phase R exit condition`.

If block verification fails, Assistant MUST return only the reduced remaining work for the block.

If block verification succeeds, Assistant MUST explicitly say that the block is accepted as a responsibility block within `Phase R`.

## 11. Stage 5 — tests alignment

Tests work in this mode is allowed after a Current task implementation or after block completion, when the user asks for it or when test consistency is explicitly part of the current step.

At Stage 5 Assistant MUST actually read:

- `plan_rules.md`;
- `conventions.md`;
- this file `interaction_mode_refactoring.md`;
- tests-map navigation sources through the Test-file selection path:
    - `tests_file_map_index.md`;
    - the relevant `tests_file_map_shard_*.md`;
    - the actual relevant test files;
- the actual changed production files needed for test design.

Stage 5 goals are:

- keep tests consistent with the refactored contracts and file responsibilities;
- preserve coverage of important behavior that was moved but not removed;
- remove or update tests that were coupled to the old mixed architecture;
- add focused regression coverage when the refactor changes architectural boundaries or public contracts.

Assistant SHOULD prefer updating existing tests over creating new test files.

If the user asks to update one failing test file, Assistant MUST return the full updated test file.

## 12. No implicit end

Mode `РЕФАКТОРИНГ` has no automatic final step.

It continues as long as the user remains in this mode and the Current block still has remaining Refactor tasks.

Normal loop:

- Stage 1 — block context formation;
- Stage 2 — one Current task design/refactoring plan;
- Stage 3 — one Current task implementation when requested;
- optional Stage 5 — tests alignment for that task;
- repeat Stage 2/3/5 for the next task;
- Stage 4 — block completion verification after all tasks in the block are done.

If the user explicitly switches mode, this mode ends at that point.