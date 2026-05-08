# ST2110-OBS-PLUGIN — Plan rules

## 0. Definitions

- **MUST** = обязательное требование без исключений, кроме явно записанных в этом файле.
- **MUST NOT** = прямой запрет.
- **MAY** = допустимое действие, но не обязательное.
- **Current mode** = активный режим работы ассистента в текущем диалоге.
- **Mode change** = явное указание пользователя переключить режим работы.
- **Mode step** = текущий этап внутри активного режима.
- **Actually read** = открыть и прочитать актуальное содержимое файла для current mode / mode step.  
  This MUST NOT be replaced by:
  - memory;
  - earlier snippets;
  - earlier turns;
  - generated combined maps;
  - shard descriptions without reading the actual file when the mode requires the actual file;
  - assumptions that files are unchanged.
- **Fully read** = прочитать файл целиком либо ту его часть, которая explicitly required by the current mode and is sufficient to proceed without guessing.  
  If the required part cannot be identified safely, the whole file MUST be read.
- **Relevant code** = production/test files, whose actual contents are needed to verify behavior, API, architecture, coverage, or whether a requested change is already implemented.
- **Code check** = checking actual current code contents, not only plans, maps, shard descriptions, or discussion.
- **Already implemented** = the required behavior is already present in current code or in newer local code explicitly provided in chat.
- **Fully grounded** = based on all required reading and checks for the current mode / mode step.
- **Copy-ready** = content can be copied into the repository without reconstructing it from scattered fragments.
- **Primary repository** = connected repository implementing this project: `NUDA9A/ST2110-OBS-PLUGIN`.
- **MTL reference repository** = connected repository used only as authoritative MTL reference source for MTL tasks: `NUDA9A/Media-Transport-Library`.
- **Pinned MTL reference branch** = `mtl-ref-v26.01`.
- **MTL task** = a task whose scope touches:
  - `ST2110_WITH_MTL` or other MTL-specific build guards / build wiring;
  - MTL backend implementation, tests, lifecycle, configuration, stats, or mapping;
  - MTL API usage;
  - behavior constrained by MTL-specific docs, headers, wrappers, or sample applications.
- **MTL reference files** = the mandatory files from the MTL reference repository on the pinned MTL reference branch. For this project they are:
  - `README.md`;
  - `doc/design.md`;
  - `doc/kernel_socket.md`;
  - `doc/external_frame.md`;
  - `include/mtl_api.h`;
  - `include/st20_api.h`;
  - `include/st30_api.h`;
  - `include/st_pipeline_api.h`;
  - `include/st30_pipeline_api.h`;
  - `app/sample/sample_util.h`;
  - `app/sample/rx_st20_pipeline_sample.c`;
  - `app/sample/rx_st30_pipeline_sample.c`;
  - `lib/src/st2110/pipeline/st20_pipeline_rx.c`;
  - `lib/src/st2110/pipeline/st30_pipeline_rx.c`.
- **MTL compact context docs** = compact MTL context artifacts stored in the Project memory and Primary repository:
  - `mtl_context_index.md`;
  - `mtl_runtime_context.md`;
  - `mtl_video_rx_context.md`;
  - `mtl_audio_rx_context.md`;
  - `mtl_task_context_map.md`.
- **Relevant original MTL reference files** = the subset of configured MTL reference files actually needed for the current mode step after reading the required MTL compact context docs.
- **MTL context selection path** = mandatory MTL lookup chain:
  1. `docs/mtl_context_index.md`;
  2. `docs/mtl_task_context_map.md`;
  3. the relevant task-specific compact MTL context docs;
  4. if the compact docs are insufficient, the Relevant original MTL reference files from the MTL reference repository on the pinned MTL reference branch.
- **DistroAV reference repository** = connected repository used only as authoritative OBS/plugin/UI reference source for DistroAV-derived tasks: `NUDA9A/DistroAV`.
- **Pinned DistroAV reference branch** = `distro-av-ref-v6.2.1`.
- **DistroAV task** = a task whose scope uses DistroAV only as OBS/plugin/UI reference material, including:
  - OBS plugin build wiring;
  - OBS module registration;
  - OBS source/input lifecycle;
  - OBS properties/UI;
  - plugin-global config/settings UI;
  - OBS output/frontend lifecycle patterns.
- **DistroAV reference files** = the mandatory files from the DistroAV reference repository on the pinned DistroAV reference branch. For this project they are:
  - `CMakeLists.txt`;
  - `src/plugin-main.cpp`;
  - `src/plugin-main.h`;
  - `src/ndi-source.cpp`;
  - `src/ndi-finder.cpp`;
  - `src/ndi-finder.h`;
  - `src/config.cpp`;
  - `src/config.h`;
  - `src/forms/output-settings.cpp`;
  - `src/forms/output-settings.h`;
  - `src/main-output.cpp`;
  - `src/main-output.h`;
  - `src/preview-output.cpp`;
  - `src/preview-output.h`;
  - `src/ndi-output.cpp`.
- **DistroAV compact context docs** = compact DistroAV OBS/plugin/UI context artifacts stored in the Project memory and Primary repository:
  - `distroav_context_index.md`;
  - `distroav_obs_plugin_context.md`;
  - `distroav_obs_source_context.md`;
  - `distroav_obs_frontend_context.md`.
- **Relevant original DistroAV reference files** = the subset of configured DistroAV reference files actually needed for the current mode step after reading the required DistroAV compact context docs.
- **DistroAV context selection path** = mandatory DistroAV lookup chain:
  1. `docs/distroav_context_index.md`;
  2. the relevant compact DistroAV context docs;
  3. if the compact docs are insufficient, the Relevant original DistroAV reference files from the DistroAV reference repository on the pinned DistroAV reference branch.
- **Sharded production map** = `code_map_index.md` plus all `code_map_shard_*.md` files.
- **Sharded tests map** = `tests_file_map_index.md` plus all `tests_file_map_shard_*.md` files.
- **Combined production map** = generated `code_map.md`.
- **Combined tests map** = generated `tests_file_map.md`.
- **Production-file selection path** = mandatory lookup chain:
  1. `code_map_index.md`;
  2. the relevant `code_map_shard_*.md`;
  3. the actual production files.
- **Test-file selection path** = mandatory lookup chain:
  1. `tests_file_map_index.md`;
  2. the relevant `tests_file_map_shard_*.md`;
  3. the actual test files.
- **Architecture rules file** = `architecture_rules.md`.
- **Mode rules file** = one of:
  - `interaction_mode_task.md`;
  - `interaction_mode_review.md`;
  - `interaction_mode_update_code_map.md`;
  - `interaction_mode_update_tests_file_map.md`;
  - `interaction_mode_unique.md`;
  - `interaction_mode_refactoring.md`.

- **Phase R** = `Phase R — Responsibility-block refactoring` from `plan.md`.
- **Refactoring block** = one contiguous block of `Phase R` tasks that refactors one responsibility block.
- **Refactor task** = one concrete file-level task inside the current Refactoring block.
- **Block completion** = the state in which all Refactor tasks of the current Refactoring block are either:
  - implemented;
  - explicitly recognized as already implemented;
  - or removed / merged / replaced by an updated block plan agreed in the dialog,
    and the resulting production block satisfies its stated responsibility without losing required production logic.
- **Phase R exit condition** = the explicit `Exit condition for Phase R` recorded in `plan.md`; it is the global architectural target that every Refactoring block MUST move the repository toward.

## 1. Rule sets and precedence

Assistant MUST follow these rule layers together:

1. Project instructions;
2. this file `plan_rules.md`;
3. the current mode rules file;
4. `architecture_rules.md` where the current mode requires it;
5. `conventions.md`.

Conflict handling:
- if Project instructions conflict with any repository rule file, Project instructions win;
- if this file defines a common default and the current mode file defines a stricter or more specific rule for the current mode, the current mode file wins for that mode;
- if `conventions.md` conflicts with this file or the current mode file, this file / the current mode file wins;
- architecture requirements from `architecture_rules.md` remain mandatory whenever the current mode requires architecture checking or architecture-aware design.

Assistant MUST NOT silently mix rules from different modes.
Assistant MUST NOT apply a mode-specific shortcut outside the active mode.

## 2. Sources and authority

Assistant MUST use sources in this order:

1. Primary repository:
- production `.hpp` / `.cpp`;
- test files;
- `plan.md`;
- sharded maps:
  - `code_map_index.md`;
  - `code_map_shard_*.md`;
  - `tests_file_map_index.md`;
  - `tests_file_map_shard_*.md`;
- generated combined maps:
  - `code_map.md`;
  - `tests_file_map.md`;
2. Project copies:
- `plan_rules.md`;
- `architecture_rules.md`;
- `conventions.md`;
- the current mode rules file;
- if the current mode step is an MTL task, MTL compact context docs:
  - `mtl_context_index.md`;
  - `mtl_runtime_context.md`;
  - `mtl_video_rx_context.md`;
  - `mtl_audio_rx_context.md`;
  - `mtl_task_context_map.md`;
- if the current mode step is a DistroAV task, DistroAV compact context docs:
  - `distroav_context_index.md`;
  - `distroav_obs_plugin_context.md`;
  - `distroav_obs_source_context.md`;
  - `distroav_obs_frontend_context.md`;
3. if the current mode step is an MTL task, the MTL reference repository on pinned branch `mtl-ref-v26.01`:
- the Relevant original MTL reference files;
4. if the current mode step is a DistroAV task, the DistroAV reference repository on pinned branch `distro-av-ref-v6.2.1`:
- the Relevant original DistroAV reference files;
5. uploaded ST 2110 / RP 2110 PDFs selected as relevant by the current mode;
6. newer local changes explicitly provided in chat.

Authoritative source rules:
- `plan_rules.md`, `architecture_rules.md`, `conventions.md`, and all mode files MUST be taken from Project copies;
- production/test code of this project MUST be taken from the Primary repository unless newer local code is provided in chat;
- sharded maps MUST be taken from the Primary repository unless newer local versions are provided in chat;
- generated `code_map.md` / `tests_file_map.md` are convenience artifacts only and MUST NOT replace the sharded-map workflow when the current mode requires shard-based lookup;
- MTL compact context docs MUST be taken from the Primary repository unless newer local versions are provided in chat;
- DistroAV compact context docs MUST be taken from the Primary repository unless newer local versions are provided in chat;
- MTL compact context docs are context-forming artifacts and MUST NOT override:
  - Project copies;
  - actual production/test code;
  - uploaded standards PDFs;
  - original MTL reference files when exact original API / enum / field / lifecycle / stats / sample behavior is required;
- DistroAV compact context docs are context-forming artifacts and MUST NOT override:
  - Project copies;
  - actual production/test code;
  - uploaded standards PDFs;
  - original DistroAV reference files when exact OBS/plugin/UI API / callback / lifecycle / object-usage behavior is required;
- original MTL reference files MUST be taken from the MTL reference repository on branch `mtl-ref-v26.01`;
- original DistroAV reference files MUST be taken from the DistroAV reference repository on branch `distro-av-ref-v6.2.1`;
- Assistant MUST NOT use `main` of the MTL reference repository as authoritative for MTL tasks;
- Assistant MUST NOT use `main` of the DistroAV reference repository as authoritative for DistroAV tasks;
- Assistant MUST NOT substitute the configured MTL reference files with remembered snippets, release notes, project copies, or other branches/tags unless the rules are explicitly updated;
- Assistant MUST NOT substitute the configured DistroAV reference files with remembered snippets, release notes, project copies, or other branches/tags unless the rules are explicitly updated.

Assistant MUST NOT ask the user to resend files already available in:
- the Primary repository;
- Project copies;
- the connected MTL reference repository for an MTL task;
- the connected DistroAV reference repository for a DistroAV task.

Assistant MAY ask only for:
- newer local uncommitted changes;
- missing / inaccessible / insufficient standards excerpts;
- clearly relevant unavailable files.

## 3. Mode system

Valid modes are:

- `ЗАДАЧА`
- `ПРОВЕРКА`
- `ОБНОВЛЕНИЕ code_map.md`
- `ОБНОВЛЕНИЕ tests_file_map.md`
- `УНИКАЛЬНЫЙ`
- `РЕФАКТОРИНГ`

Mode activation rules:
- a new workflow MUST start only after the user explicitly names the mode;
- after activation, the mode remains the Current mode until the user explicitly changes it;
- a follow-up user message inside the same workflow MUST be treated as continuation of the Current mode unless the user explicitly changes the mode;
- before acting in a mode, Assistant MUST read the corresponding mode file and follow it.

If no Current mode exists and the user message does not explicitly set one, Assistant MUST:
- ask the user to specify the mode;
- limit the response to that clarification only.

Assistant MUST NOT silently reinterpret:
- `ЗАДАЧА` as `ПРОВЕРКА`;
- `ПРОВЕРКА` as `УНИКАЛЬНЫЙ`;
- `РЕФАКТОРИНГ` as `ЗАДАЧА`;
- `РЕФАКТОРИНГ` as `ПРОВЕРКА`;
- `ЗАДАЧА` as `РЕФАКТОРИНГ`;
- `ПРОВЕРКА` as `РЕФАКТОРИНГ`;
- map-update modes as implementation modes.

## 4. Sharded map workflow

When the current mode requires choosing production files, Assistant MUST use the Production-file selection path:

1. read `code_map_index.md`;
2. identify the relevant responsibility block(s);
3. read the shard(s) for those block(s);
4. use the shard file lists and block/subblock responsibilities to choose the concrete production files;
5. read the actual production files.

When the current mode requires choosing test files, Assistant MUST use the Test-file selection path:

1. read `tests_file_map_index.md`;
2. identify the relevant shard(s);
3. read the relevant shard entries;
4. read the actual test files.

Block-shard rules:

- `code_map_index.md` is the entry point for block-aware production-file selection.
- Each `code_map_shard_*.md` describes one responsibility block, its subblocks, and the files that belong to that block.
- A shard is authoritative for block/file selection only as a map artifact; it MUST NOT replace actual file reading when the mode requires actual-file reading.
- Assistant MUST use block responsibility and file membership from the shards to avoid selecting files by name similarity alone.
- If a file appears to mix multiple block responsibilities, Assistant MUST treat that as an architectural smell and verify by reading the actual file before making block-grounded claims.

Maps help block/file selection and expected subsystem placement, but MUST NOT replace reading actual files when the mode requires actual-file reading.

If a map conflicts with actual code/tests:
- actual code/tests win;
- map-update output MUST reflect actual code/tests.

## 5. Reference-repository context rules

If the current mode step is an MTL task, Assistant MUST actually read all required items from the MTL context selection path.

If the required compact MTL context docs are insufficient for the current step, Assistant MUST then actually read the Relevant original MTL reference files from:
- repository `NUDA9A/Media-Transport-Library`;
- branch `mtl-ref-v26.01`.

If the compact MTL context docs are insufficient, Assistant SHOULD also search the Primary repository for clearly relevant current project files rather than guessing.

Assistant MUST NOT read the entire configured original MTL reference set by default if the compact MTL context docs plus the Relevant original MTL reference files are already sufficient for the current step.

If the current mode step is a DistroAV task, Assistant MUST actually read all required items from the DistroAV context selection path.

If the required compact DistroAV context docs are insufficient for the current step, Assistant MUST then actually read the Relevant original DistroAV reference files from:
- repository `NUDA9A/DistroAV`;
- branch `distro-av-ref-v6.2.1`.

Assistant MUST NOT read the entire configured original DistroAV reference set by default if the compact DistroAV context docs plus the Relevant original DistroAV reference files are already sufficient for the current step.

If the current mode step is NOT an MTL task, Assistant MUST NOT read original MTL reference files and SHOULD NOT mention them.

If the current mode step is NOT a DistroAV task, Assistant MUST NOT read original DistroAV reference files and SHOULD NOT mention them.

If the MTL reference repository or pinned branch is unavailable for an MTL task, Assistant MUST treat that as missing required material.

If the DistroAV reference repository or pinned branch is unavailable for a DistroAV task, Assistant MUST treat that as missing required material.

## 6. Missing material rule

If any file required by the current mode / mode step:
- was not actually read;
- is unavailable;
- is incomplete;
- is accessible only through stale prose, memory, map text, or an old snippet;
- or cannot be fully read in the part required to proceed,

Assistant MUST:
- explicitly name what is missing;
- request only that missing material;
- stop before continuing the workflow step that depends on it;
- limit the response to a partial answer.

Assistant MUST NOT pretend that context was fully checked.

If a required MTL compact context doc, required original MTL reference file, or required standards PDF for the current step was not actually read:
- Assistant MUST explicitly name it as missing;
- Assistant MUST NOT present the result as fully grounded;
- Assistant MUST stop before the workflow step that depends on it.

If a required DistroAV compact context doc or required original DistroAV reference file for the current step was not actually read:
- Assistant MUST explicitly name it as missing;
- Assistant MUST NOT present the result as fully grounded;
- Assistant MUST stop before the workflow step that depends on it.

## 7. No-pretence rule

Assistant MUST NOT:
- say that rules/work/context were studied unless the required rule files for the current mode were actually read;
- say that code was checked unless the required actual files were actually read;
- present an answer as standards-grounded unless the standards files required by the current mode were actually read;
- present an answer as MTL-grounded unless the required compact MTL context docs, and any required Relevant original MTL reference files for that MTL step, were actually read;
- present an answer as DistroAV-grounded unless the required compact DistroAV context docs, and any required Relevant original DistroAV reference files for that DistroAV step, were actually read;

If the current mode requires only selected relevant standards PDFs, Assistant MUST NOT claim to have checked standards outside that selected set.

## 8. Already-implemented rule

Whenever the current mode requires a code check, the check MUST be grounded in actual current files.

This check MUST NOT rely only on:
- shard descriptions;
- generated maps;
- backlog wording;
- earlier discussion;
- assistant memory.

If the requested behavior is already implemented:
- Assistant MUST NOT propose redundant implementation;
- Assistant MUST explicitly say that no additional production/test code is needed for that scope;
- Assistant MAY point only to remaining non-implementation steps allowed by the current mode.

If the scope is only partially implemented:
- Assistant MUST separate existing behavior from missing scope;
- Assistant MUST propose only the remaining work.

## 9. Combined maps

Generated `code_map.md` and `tests_file_map.md` MAY exist as repository convenience artifacts.

However:
- they MUST NOT replace shard-based navigation in modes that explicitly require shard lookup;
- they MUST NOT be treated as more authoritative than:
  - the sharded maps;
  - the actual code/tests.

## 10. References

- SMPTE ST 2110-10:2022
- SMPTE ST 2110-20:2022
- SMPTE ST 2110-21:2022
- SMPTE ST 2110-30:2025
- SMPTE RP 2110-25:2023
- RFC 3550
- RFC 4175
- Wireshark dissector ST2110-20
- MTL reference repository: `NUDA9A/Media-Transport-Library`
- pinned MTL reference branch: `mtl-ref-v26.01`
- MTL compact context docs listed in section 0
- configured MTL reference files listed in section 0, for MTL tasks only
- DistroAV reference repository: `NUDA9A/DistroAV`
- pinned DistroAV reference branch: `distro-av-ref-v6.2.1`
- DistroAV compact context docs listed in section 0
- configured DistroAV reference files listed in section 0, for DistroAV tasks only