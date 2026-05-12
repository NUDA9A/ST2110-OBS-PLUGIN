# ST2110-OBS-PLUGIN — Plan rules

## 0. Definitions

- **MUST** = mandatory requirement without exceptions unless this file explicitly allows one.
- **MUST NOT** = direct prohibition.
- **MAY** = allowed but optional action.
- **Current task scope** = the concrete question, change, review, or design direction currently given by the user.
- **Actually read** = open and inspect the current content of the required file(s) for the current task scope.
  This MUST NOT be replaced by:
  - memory;
  - earlier snippets;
  - earlier turns;
  - old maps;
  - guessed file contents;
  - assumptions that files are unchanged.
- **Fully grounded** = based on all reading and checks that are required for the current task scope.
- **Relevant code** = the production files whose current contents are needed for the current task scope.
- **Code check** = checking the actual current code contents, not only prior discussion, maps, or backlog wording.
- **Already implemented** = the requested behavior is already present in current code or in newer local code explicitly provided by the user in chat.
- **Copy-ready** = output can be copied into the repository without reconstruction from scattered fragments.
- **Primary repository** = connected repository implementing this project: `NUDA9A/ST2110-OBS-PLUGIN`.
- **MTL reference repository** = connected repository used only as authoritative MTL API/reference material: `NUDA9A/Media-Transport-Library`.
- **Pinned MTL reference branch** = `mtl-ref-v26.01`.
- **DistroAV reference repository** = connected repository used only as authoritative OBS/plugin/UI/NDI-reference material: `NUDA9A/DistroAV`.
- **Pinned DistroAV reference branch** = `distro-av-ref-v6.2.1`.
- **MTL compact context docs** = project-local MTL context documents:
  - `mtl_context_index.md`;
  - `mtl_runtime_context.md`;
  - `mtl_video_rx_context.md`;
  - `mtl_audio_rx_context.md`;
  - `mtl_task_context_map.md`.
- **DistroAV compact context docs** = project-local DistroAV context documents:
  - `distroav_context_index.md`;
  - `distroav_obs_plugin_context.md`;
  - `distroav_obs_source_context.md`;
  - `distroav_obs_frontend_context.md`.
- **Relevant standards set** = the subset of uploaded ST 2110 / RP 2110 PDFs actually needed for the current task scope.
- **code_map update request** = an explicit user request to update `code_map` / `code_map_index.md` / `code_map_shard_*.md`.

## 1. Rule layers and precedence

Assistant MUST follow these rule layers together:

1. Project instructions;
2. this file `plan_rules.md`;
3. `architecture_rules.md` whenever architecture, standards, pipeline boundaries, modeling, validation, runtime behavior, or extensibility are touched;
4. `conventions.md`;
5. `interaction_mode_update_code_map.md` only when the user explicitly asks to update `code_map`.

Conflict handling:
- if Project instructions conflict with repository rule files, Project instructions win;
- if `interaction_mode_update_code_map.md` defines stricter rules for a code-map update request, that file wins for that request only;
- if `conventions.md` conflicts with this file or `architecture_rules.md`, this file / `architecture_rules.md` win.

There is no general multi-mode workflow anymore.
Outside explicit `code_map` updates, Assistant MUST work directly from the user’s task scope.

## 2. Sources and authority

Assistant MUST use sources in this order:

1. actual production files from the Primary repository;
2. project copies of:
  - `plan_rules.md`;
  - `architecture_rules.md`;
  - `conventions.md`;
  - `interaction_mode_update_code_map.md` when relevant;
  - MTL compact context docs when relevant;
  - DistroAV compact context docs when relevant;
3. relevant original MTL reference files from the MTL reference repository on `mtl-ref-v26.01`, but only for MTL-scoped work;
4. relevant original DistroAV reference files from the DistroAV reference repository on `distro-av-ref-v6.2.1`, but only for DistroAV-scoped work;
5. the Relevant standards set from the uploaded ST 2110 / RP 2110 PDFs;
6. newer local code or text explicitly provided by the user in chat.

Authoritative-source rules:
- actual production files are authoritative for project behavior;
- rule files and compact context docs MUST be taken from the project copies unless newer local versions are explicitly provided in chat;
- `code_map`, `code_map_index.md`, and `code_map_shard_*.md` are documentation artifacts, not source of truth for ordinary task work;
- `code_map` MAY help orient repository structure, but MUST NOT replace reading actual relevant files;
- compact context docs are scoping aids, not substitutes for exact original reference material when exact API/enum/field/lifecycle behavior is needed;
- the MTL reference repository MUST be read only on `mtl-ref-v26.01`;
- the DistroAV reference repository MUST be read only on `distro-av-ref-v6.2.1`.

## 3. Default workflow

The default workflow is simple.

For the current task scope Assistant MUST:

1. determine whether the request is:
  - code-grounded;
  - architecture-grounded;
  - standards-grounded;
  - MTL-scoped;
  - DistroAV-scoped;
  - documentation-only;
  - or some combination of these;
2. read only the files actually needed for that scope;
3. perform a code check when the request depends on current implementation state;
4. determine the Relevant standards set when standards matter;
5. answer the user directly, without inventing artificial stages.

The user sets the vector of work in the message itself.
Assistant MUST NOT force the user through a staged mode system when the user did not ask for one.

## 4. Required reading by scope

### 4.1 Ordinary code/design/review work

Assistant MUST actually read:
- `plan_rules.md`;
- `conventions.md`;
- `architecture_rules.md` when the request is architecture-aware, standards-aware, pipeline-aware, or review-oriented;
- the actual relevant production files;
- newer local code in chat, if present.

### 4.2 Standards-aware work

If the current task scope depends on ST 2110 / RP 2110 behavior, Assistant MUST:
- determine the Relevant standards set;
- actually read that set;
- limit claims to what that set supports.

Assistant MUST NOT claim to have checked standards outside the actually read subset.

### 4.3 MTL-scoped work

For MTL-scoped work Assistant MUST first read:
- `mtl_context_index.md`;
- `mtl_task_context_map.md`;
- the relevant task-specific compact MTL context docs.

If those compact docs are insufficient, Assistant MUST then read the relevant original MTL reference files on branch `mtl-ref-v26.01`.

Assistant MUST NOT read original MTL reference files for non-MTL work.

### 4.4 DistroAV-scoped work

For DistroAV-scoped work Assistant MUST first read:
- `distroav_context_index.md`;
- the relevant compact DistroAV context docs.

If those compact docs are insufficient, Assistant MUST then read the relevant original DistroAV reference files on branch `distro-av-ref-v6.2.1`.

Assistant MUST NOT read original DistroAV reference files for non-DistroAV work.

## 5. Code-check rule

Whenever the request depends on current repository state, Assistant MUST perform a code check against actual current files.

This check MUST NOT rely only on:
- `code_map`;
- backlog wording;
- prior turns;
- memory;
- inferred architecture.

If the requested behavior is already implemented:
- Assistant MUST say so explicitly;
- Assistant MUST NOT propose redundant implementation work.

If the scope is only partially implemented:
- Assistant MUST separate implemented scope from missing scope;
- Assistant MUST propose only the remaining work.

## 6. No-pretence rule

Assistant MUST NOT:
- say that files were studied unless they were actually read for the current task scope;
- say that code was checked unless the actual relevant files were checked;
- present an answer as standards-grounded unless the Relevant standards set was actually read;
- present an answer as MTL-grounded unless the required compact MTL context docs and any required original MTL reference files were actually read;
- present an answer as DistroAV-grounded unless the required compact DistroAV context docs and any required original DistroAV reference files were actually read.

If required material is missing or unread, Assistant MUST explicitly name the gap and limit the answer accordingly.

## 7. Output rules

Assistant MAY choose the response shape that best fits the request.

However:
- repository-facing output MUST remain copy-ready;
- file paths MUST be explicit for repository-facing changes;
- full-file replacement is appropriate when the file’s behavior/rules are being globally reshaped;
- otherwise Assistant SHOULD limit output to the necessary blocks only;
- Assistant MUST clearly distinguish:
  - what is confirmed from files;
  - what is a design recommendation;
  - what remains unread or uncertain.

## 8. Tests and absent tests

Assistant MUST NOT assume that tests exist.

If the repository currently has no relevant test files for the scope:
- Assistant MUST say that test work is currently absent or deferred if that matters;
- Assistant MUST NOT invent `tests_file_map` workflow obligations for ordinary work.

If the user later asks for tests and tests exist, Assistant MAY read and update them directly as ordinary repository files.

## 9. code_map rule

Outside explicit `code_map` update requests:
- `code_map` is auxiliary only;
- it MUST NOT drive file selection instead of repository search and actual file reading;
- it MUST NOT be treated as authoritative when it disagrees with actual code.

During explicit `code_map` update requests:
- follow `interaction_mode_update_code_map.md` in addition to this file.

## 10. Core references

Standards and protocol references that commonly matter in this project:
- SMPTE ST 2110-10:2022;
- SMPTE ST 2110-20:2022;
- SMPTE ST 2110-21:2022;
- SMPTE ST 2110-30:2025;
- SMPTE RP 2110-25:2023;
- RFC 3550;
- RFC 4175.

Reference repositories and compact context retained by project policy:
- `NUDA9A/Media-Transport-Library` on `mtl-ref-v26.01`;
- `NUDA9A/DistroAV` on `distro-av-ref-v6.2.1`;
- the compact MTL context docs;
- the compact DistroAV context docs.