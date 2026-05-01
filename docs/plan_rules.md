# ST2110-OBS-PLUGIN — Plan rules

## 0. Definitions

- **MUST** = обязательное требование без исключений, кроме явно записанных в этом файле.
- **MUST NOT** = прямой запрет.
- **MAY** = допустимое действие, но не обязательное.
- **Current step** = текущее формулирование задачи, текущая приемка или текущий ответ, в котором ассистент утверждает, что изучил rules/work/context.
- **Actually read** = открыть и прочитать актуальное содержимое файла для current step.  
  This MUST NOT be replaced by:
  - memory;
  - earlier snippets;
  - earlier turns;
  - file maps;
  - backlog wording;
  - assumptions that files are unchanged.
- **Relevant code** = production/test files, whose actual contents are needed to verify behavior, API, architecture, coverage, or whether a task is already implemented.
- **Code check** = checking actual current code contents, not only plans, maps, or discussion.
- **Already implemented** = the required behavior is already present in current code or in newer local code explicitly provided in chat.
- **Fully grounded** = based on all required reading and checks for the current step.
- **Copy-ready** = content can be copied into the repository without reconstructing it from scattered fragments.
- **MTL task** = a task whose scope touches:
  - `ST2110_WITH_MTL` or other MTL-specific build guards / build wiring;
  - MTL backend implementation, tests, lifecycle, configuration, stats, or mapping;
  - MTL API usage;
  - behavior constrained by MTL-specific docs, wrappers, or sample applications.
- **MTL reference files** = the Project copies of the MTL materials uploaded for this project. For this project they are:
  - `design.md`;
  - `README.md`;
  - `linux-mtl.h`;
  - `linux-mtl.c`;
  - `mtl-input.c`;
  - `rx_st20p_app.c`;
  - `rx_st30p_app.c`.

## 1. Sources and authority

Assistant MUST use sources in this order:
1. connected repository:
- production `.hpp` / `.cpp`;
- test files;
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md`;
2. Project copies:
- `plan_rules.md`;
- `conventions.md`;
3. if the current step is an MTL task, the Project copies of all MTL reference files;
4. all ST 2110 / RP 2110 PDFs uploaded in the Project;
5. newer local changes explicitly provided in chat.

Authoritative source rules:
- `plan_rules.md` and `conventions.md` MUST be taken from Project copies.
- `plan.md`, `code_map.md`, `tests_file_map.md` MUST be taken from the repository unless newer local versions are provided in chat.
- production/test code MUST be taken from the repository unless newer local code is provided in chat.
- MTL reference files MUST be taken from the Project copies uploaded for this project.

Assistant MUST NOT ask the user to resend files already available in the repository or Project.
Assistant MAY ask only for:
- newer local uncommitted changes;
- missing / inaccessible / insufficient standards excerpts;
- clearly relevant unavailable files.

If the current step is NOT an MTL task, Assistant MUST NOT read MTL reference files and SHOULD NOT mention them.

## 2. One-task workflow

- Assistant MUST work on one task at a time.
- Assistant MUST NOT move to the next task before the current one is accepted.
- By default, implementation is written by the user.
- Assistant MUST NOT provide full implementation unless the user explicitly asks for it.

Default output for a task:
- architecture requirements;
- exact scope of changes;
- expected behavior;
- tests;
- copy-ready API skeletons when production API / headers / helper boundaries change or a new production file is added.

## 3. Mandatory reading before task proposal

Before proposing the next task, Assistant MUST actually read:
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md` when tests / coverage / test selection matter;
- Project copies of `plan_rules.md` and `conventions.md`;
- relevant code;
- all ST 2110 / RP 2110 PDFs uploaded in the Project;
- newer local chat changes, if they exist.

If the current step is an MTL task, Assistant MUST also actually read all MTL reference files.

If the current step is NOT an MTL task, Assistant MUST NOT read MTL reference files.

Before proposing the next task, Assistant MUST also perform a code check to determine whether the task is already implemented.

This check MUST be grounded in code itself.
It MUST NOT rely only on:
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md`;
- backlog text;
- earlier discussion;
- assistant memory.

If the task is already implemented:
- Assistant MUST NOT propose redundant implementation.
- Assistant MUST explicitly say that no additional production/test code is needed.
- Assistant MAY point only to remaining non-implementation steps:
  - acceptance;
  - project `.md` updates;
  - the next truly unfinished task.

If the task is only partially implemented:
- Assistant MUST separate existing behavior from missing scope.
- Assistant MUST propose only the remaining work.

If required reading or code check was not completed for the current step, Assistant MUST NOT present the task proposal as fully grounded.

## 4. Mandatory reading before acceptance

Before accepting an implementation, Assistant MUST actually read:
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md` when relevant;
- Project copies of `plan_rules.md` and `conventions.md`;
- relevant code;
- all ST 2110 / RP 2110 PDFs uploaded in the Project;
- newer local chat changes, if they exist.

If the current step is an MTL task, Assistant MUST also actually read all MTL reference files.

If the current step is NOT an MTL task, Assistant MUST NOT read MTL reference files.

Acceptance MUST verify:
- compliance with the task;
- compliance with `plan.md`;
- compliance with `plan_rules.md`;
- compliance with `conventions.md`;
- compliance with relevant standards requirements;
- absence of new undocumented deviations;
- architecture extensibility;
- adequate test coverage;
- localization of temporary limits and presence of follow-up when needed.

Acceptance MUST NOT be presented as standards-aware unless all uploaded ST 2110 / RP 2110 PDFs were actually read for the current step.

If required reading was not completed for the current step, Assistant MUST NOT:
- say that rules/work/context/standards were studied;
- present acceptance as fully grounded.

## 5. Incomplete context rule

If any required `.md`, relevant code, required standards PDF, or required MTL reference file for an MTL task:
- was not actually read;
- is unavailable;
- is incomplete;
- is replaced only by prose, memory, map, or old snippet,

Assistant MUST:
- explicitly name what is missing;
- request only that missing material;
- limit the response to a partial answer.

Assistant MUST NOT pretend that context was fully checked.

## 6. Task formulation rules

Every task proposal MUST be checked against:
- current rules;
- current architecture;
- current code state;
- current standards context.

If the current step is an MTL task, the task proposal MUST also be checked against:
- current MTL reference files;
- current MTL wrapper / sample usage patterns relevant to the task.

Assistant MUST treat every task as potentially relevant to ST 2110 compliance and architecture extensibility.

Assistant MUST NOT:
- assume a task is too small to require standards reading;
- lock in an avoidable standards deviation;
- turn an MVP limit into architecture;
- replace a known modeled axis / support boundary / validation boundary / derived value with a hardcoded assumption.

If a new deviation is found during task proposal:
- Assistant SHOULD fix it in the current task if scope remains reasonable.
- Otherwise Assistant MUST:
  - explicitly name it;
  - add it to `Spec notes / deviations`;
  - create or reference a follow-up task.

Assistant MUST NOT add a new deviation if:
- the limitation is already known;
- it is already covered by an existing task in `plan.md`;
- the current task does not worsen it or change its nature.

In that case Assistant MUST reference the existing backlog item and MUST NOT duplicate the deviation.

## 7. API/output rules

If a task changes production API / headers / helper boundaries or adds a production file, Assistant MUST provide copy-ready API skeletons.

Output rules:
- new production file → full file;
- existing production `.hpp` → only new / changed declaration blocks;
- full replacement of an existing production header → only if explicitly requested by the user;
- unchanged declarations MUST NOT be resent;
- existing inline implementations MUST NOT be resent.

All declarations intended for user implementation SHOULD end with `;`.

Assistant MUST NOT leave required helper boundaries only in prose if they are part of the intended architecture or API.

For every new or semantically changed method / helper, Assistant MUST describe:
- inputs;
- validation boundary;
- success path;
- state changes / side effects;
- returned errors / failure behavior;
- invariants after the call;
- temporary support limits, if any.

Assistant MUST NOT provide implementation bodies by default.

## 8. Test rules

Assistant MUST prefer extending an existing test file over creating a new one.
Assistant MUST use `tests_file_map.md` to choose the target test file.

A new test target / new `.cpp` test is allowed only if:
- no suitable existing file exists;
- a separate subsystem / boundary is introduced;
- using an existing file would make coverage less maintainable.

When tests are required, Assistant MUST provide:
- the exact `add_st2110_test(...)` line if a new target is actually needed;
- the full `.cpp` for every new test;
- the full `.cpp` for every existing test file that must be replaced.

Test ideas alone are insufficient.

If the user does not report changes to tests previously provided in full, those tests and corresponding `add_st2110_test(...)` lines are assumed copied unchanged.

At acceptance, the user MUST NOT be required to resend tests if they are already available in the repository or were previously provided in full and unchanged.

## 9. Project `.md` update rules

A task is NOT fully accepted until:
1. implementation and tests are checked;
2. required project `.md` updates are checked.

After implementation, Assistant MUST determine whether updates are needed for:
- `plan.md`;
- `code_map.md`;
- `tests_file_map.md`;
- `plan_rules.md`;
- `conventions.md`.

Update triggers:
- `plan.md` → task status, backlog, or `Spec notes / deviations` changed;
- `code_map.md` → production files / roles / APIs / relationships changed;
- `tests_file_map.md` → test files / targets changed;
- `plan_rules.md` → working rules changed;
- `conventions.md` → stable conventions changed.

Assistant MUST provide replaceable blocks, not scattered line fragments:
- `plan.md` → full task block / subsection;
- `code_map.md` → full file block;
- `tests_file_map.md` → full test-file block / subsection;
- `plan_rules.md` and `conventions.md` → full updated file by default when their behavior changes.

Line-level edits are allowed only if the user explicitly asks for a diff.

After the user updates project `.md` files, Assistant MUST verify:
- only completed tasks were marked completed;
- nothing unfinished was closed;
- `plan.md` status is reflected in the correct place;
- maps match actual code/tests;
- new limits / deviations were not lost.

Only then MAY the task be treated as fully accepted.

## 10. Plan/status rules

Default `plan.md` workflow:
- completed tasks are marked `[x]` where declared;
- tasks are NOT moved to `## Done` by default;
- the same task is NOT duplicated both in place and in `## Done`.

A historical `## Done` section does NOT change this default by itself.

## 11. Map rules

`code_map.md` MUST describe actual production code structure:
- architectural role;
- major dependencies / connections;
- key enums / structs / classes / functions / methods.

`tests_file_map.md` MUST describe actual test targets / test files.

If a map conflicts with actual code/tests, actual code/tests win and the map MUST be updated.

Maps help select files but MUST NOT replace reading actual relevant files.

## 12. Architecture rules

Code MUST remain extensible.

Typical future support SHOULD require:
- adding enum/value coverage;
- adding switch / adapter / mapper branches;
- adding tests;
- not rewriting the pipeline.

Assistant MUST NOT lock the architecture to:
- one pixel format only;
- video-only forever;
- one backend only;
- console-only pipeline forever;
- progressive-only semantics forever.

The following MUST be explicit modeled axes / boundaries / derived values where relevant:
- media kind;
- backend kind;
- pixel/storage format;
- `VideoScanMode`;
- packing mode;
- RTP payload type admission;
- completion semantics;
- clock / timestamp / timing / playout policy;
- packet size / MAXUDP policy;
- SDP/raw signaling mapping boundaries;
- video media-description properties;
- audio sampling rate / packet time / samples-per-packet derivation;
- audio conformance level / current support boundary;
- audio channel count / order / mapping;
- receiver capability / support policy.

This list is not exhaustive.
Any known variable standard / signaling / runtime / backend parameter MUST be treated as a modeled axis or derived value unless clearly proven otherwise.

MVP limitations are allowed only as explicit:
- modeled axes;
- support / validation boundaries;
- named policies / helpers / constants;
- localized `Unsupported` / `InvalidValue` branches;
- follow-up-backed temporary limits.

MVP limitations MUST NOT be encoded as:
- magic constants;
- unnamed literals;
- ad hoc branches without named boundaries;
- fixed values that should be derived from signaled/runtime inputs;
- omission of already-known architectural axes.

Example: audio `samples_per_packet` MUST be derived from `sampling_rate_hz` and `packet_time_us`, not hardcoded as `48`.

If an axis / boundary / dispatch already exists architecturally, fuller support MAY be implemented later.
Later tasks SHOULD fill existing branches, not redesign architecture.

Avoidable standards deviations MUST NOT be accumulated intentionally.
If a standards deviation can be fixed now without unreasonable scope growth, it SHOULD be fixed now.
Otherwise it MUST be recorded in `Spec notes / deviations` and backlog.

## 13. Phase goals

- **MVP** = minimal viable ST 2110 video/audio receive on Linux, two backends, basic OBS integration, manual E2E readiness.
- **Medium** = broader format coverage, robustness, edge-cases, UX/observability, testing readiness.
- **Plugin** = stable and usable OBS plugin behavior.
- **Tests** = systematic regression and coverage.
- **Hardening** = performance, recovery, correctness polish.
- **Windows** = optional port of own socket backend without MTL.

## 14. References

- SMPTE ST 2110-10:2022 (Имя в памяти: st2110-10-2022.pdf)
- SMPTE ST 2110-20:2022 (Имя в памяти: st2110-20-2022 (1).pdf)
- SMPTE ST 2110-21:2022 (Имя в памяти: st2110-21-2022.pdf)
- SMPTE ST 2110-30:2025 (Имя в памяти: st2110-30-2025.pdf)
- SMPTE RP 2110-25:2023 (Имя в памяти: rp2110-25-2023.pdf)
- RFC 3550
- RFC 4175
- Wireshark dissector ST2110-20
- Intel MTL docs + `st_pipeline_api`
- Project MTL reference files listed in section 0, for MTL tasks only