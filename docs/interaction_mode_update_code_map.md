# ST2110-OBS-PLUGIN — Interaction mode: ОБНОВЛЕНИЕ code_map

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Copy-ready**, and **Primary repository** inherit the meanings defined in `plan_rules.md`.
- **Listed file** = a production file explicitly named by the user for code-map update work.
- **New listed file** = a listed file marked by the user as new.
- **Current map docs** = `code_map_index.md`, `code_map.md`, and all `code_map_shard_*.md` files that currently exist in the repository.

## 1. Scope

This file applies only when the user explicitly asks to update `code_map`.

This workflow updates code-map documentation only.
It does NOT by itself perform:
- production implementation;
- architecture review of the whole repository;
- test-map work.

## 2. Authority model inside code-map work

During code-map update work:
- actual production files are the source of truth;
- current map docs are editable documentation artifacts only;
- current map docs MAY be stale, incomplete, or wrongly placed.

Therefore Assistant MUST use the current map docs only to:
- find existing entry locations if they exist;
- preserve shard structure/style where still useful;
- determine what documentation must be replaced.

Assistant MUST NOT treat an existing map entry as proof that the described file still behaves that way.

## 3. Mandatory reading

For each Listed file Assistant MUST actually read:
- `plan_rules.md`;
- `conventions.md`;
- this file `interaction_mode_update_code_map.md`;
- the actual Listed file from the Primary repository;
- the current relevant map docs if they exist and are needed to place or replace the entry.

Assistant MAY read directly related production files when needed to describe:
- file role;
- relationships;
- public entities;
- neighboring ownership boundaries.

## 4. Existing-entry rule

If an entry for the Listed file already exists in the current map docs:
- Assistant MUST update that entry based on actual current code;
- Assistant MUST preserve still-correct stable information where appropriate;
- Assistant MUST NOT keep stale statements just to match the old entry.

## 5. New-entry rule

If the Listed file is new or missing from the current map docs:
- Assistant MUST place it in the most suitable shard if a shard structure exists;
- if the current shard structure itself is obviously wrong for the file, Assistant MUST say so explicitly;
- Assistant MAY still provide the best local placement proposal instead of blocking on global map perfection.

## 6. Output

For each Listed file Assistant MUST provide:
- the map document path that should be updated;
- whether the entry is:
  - replace existing entry;
  - add new entry;
  - or move + replace entry;
- one full copy-ready entry block for that file.

Assistant MUST NOT return one entry as scattered fragments.

If adjacent map changes are strictly necessary to keep the entry coherent, Assistant MUST state that explicitly and provide those adjacent blocks separately.

## 7. Multiple files

If the user lists multiple files, Assistant SHOULD handle them one file at a time in the response, grouped clearly by file.

## 8. code_map outside this mode

Outside this explicit mode, `code_map` is not authoritative and MUST NOT be used instead of actual file reading.