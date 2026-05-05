# ST2110-OBS-PLUGIN — Interaction mode: ПРОВЕРКА

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Code check**, **Copy-ready**, **MTL task**, and **Production-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Responsibility block** = one coherent implementation block selected for standards/architecture review.
- **Review Stage 1** = responsibility-block partition step.
- **Review Stage i** = review of one user-approved responsibility block.
- **Relevant standards set** = the subset of uploaded ST 2110 / RP 2110 PDFs actually needed for the current responsibility block.

## 1. General mode rules

Mode `ПРОВЕРКА` is a multi-step review workflow.

The steps are:

1. Review Stage 1 — partition the implementation into `N` responsibility blocks;
2. Review Stage `i` — review one user-approved block.

Assistant MUST remain in this mode until the user explicitly changes the mode.

Default scope of this mode is implementation review, not test-coverage review.
Assistant MUST NOT read tests in this mode unless the user explicitly asks for test review inside this mode.

## 2. Review Stage 1 — block partition

At Review Stage 1 Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_review.md`;
- `code_map_index.md`;
- the relevant `code_map_shard_*.md` needed to partition the repository into coherent responsibility blocks.

Assistant MAY additionally read selected actual production files if needed to avoid a misleading partition.

Assistant MUST then partition the implementation into `N` responsibility blocks and present that partition to the user.

The response MUST contain:
- a numbered list of responsibility blocks;
- a short definition of the scope of each block.

Assistant MUST wait for user approval of the partition before proceeding to per-block review.

## 3. Review Stage i — mandatory reading

For each approved responsibility block, Assistant MUST actually read:

- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- this file `interaction_mode_review.md`;
- production-map navigation sources through the Production-file selection path:
    - `code_map_index.md`;
    - the relevant `code_map_shard_*.md`;
    - the actual relevant production files for the block;
- the Relevant standards set for that block;
- for an MTL block, the compact MTL context docs required for that block:
  - `docs/mtl_context_index.md`;
  - `docs/mtl_task_context_map.md`;
  - the task-specific compact MTL context docs selected by that map;
- if those compact docs are insufficient for the current block review step, the relevant original MTL reference files on pinned branch `mtl-ref-v26.01`;
- for a DistroAV block, the compact DistroAV context docs required for that block:
  - `docs/distroav_context_index.md`;
  - the relevant compact DistroAV context docs;
- if those compact docs are insufficient for the current block review step, the relevant original DistroAV reference files on pinned branch `distro-av-ref-v6.2.1`;
- newer local code provided in chat, if any.

If the block is NOT an MTL block, Assistant MUST NOT read MTL compact context docs or original MTL reference files.

If the block is NOT a DistroAV block, Assistant MUST NOT read DistroAV compact context docs or original DistroAV reference files.

At this stage Assistant MUST perform a code check against the actual selected production files.

## 4. Review Stage i — checks

For the current responsibility block Assistant MUST check:

- compliance with the relevant standards set for that block;
- compliance with `architecture_rules.md`;
- compliance with the relevant original MTL reference material actually required/read for an MTL block, using the compact MTL context docs as review-scoping context;
- compliance with the relevant original DistroAV reference material actually required/read for a DistroAV block, using the compact DistroAV context docs as review-scoping context.
- whether existing implementation already satisfies the expected behavior;
- whether the current block contains hidden narrowing, non-extensible structure, silent fallbacks, duplicated responsibility, or unmodeled varying parameters.

Assistant MUST NOT rely only on:
- shard descriptions;
- plan wording;
- memory;
- earlier discussion.

## 5. Review Stage i — output

If the Assistant finds nonconformities, each nonconformity MUST be оформлено как задача in the current `plan.md` task style.

The output for each such task MUST include at least:

- what kind of nonconformity it is:
    - standards mismatch;
    - architecture mismatch;
    - or both;
- where it is located;
- what exactly is wrong;
- how and where to fix it.

Assistant MUST provide those tasks as copy-ready blocks suitable for insertion into the current `plan.md` format used in the repository.

If no nonconformities are found for the current block, Assistant MUST explicitly say that no discrepancies were found for that block.

## 6. Already-implemented rule in review mode

If the expected behavior for a reviewed block is already implemented correctly:
- Assistant MUST NOT invent corrective tasks for already-correct code;
- Assistant MUST explicitly state that no additional production code is needed for that reviewed scope.

## 7. Test scope

By default this mode is production review only.

Assistant MUST NOT read tests or `tests_file_map_*` files unless the user explicitly asks to review tests as part of this mode.