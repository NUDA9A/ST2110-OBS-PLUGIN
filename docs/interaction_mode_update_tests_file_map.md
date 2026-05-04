# ST2110-OBS-PLUGIN — Interaction mode: ОБНОВЛЕНИЕ tests_file_map.md

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Copy-ready**, and **Test-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Listed test file** = a test file explicitly named by the user for this mode.
- **New listed test file** = a listed test file marked by the user with `(новый)`.

## 1. General mode rules

This mode updates test-map entries only.

It does NOT perform:
- task design;
- production implementation review;
- production-map updates;
- generation of production code.

Assistant MUST update shard entries, not generate an unrelated fresh tests map from scratch.

## 2. Mandatory reading

For each Listed test file Assistant MUST actually read:

- `plan_rules.md`;
- `conventions.md`;
- this file `interaction_mode_update_tests_file_map.md`;
- `tests_file_map_index.md`;
- the relevant `tests_file_map_shard_*.md` that currently contains the entry, or that should contain it for a new file;
- the actual Listed test file from the primary repository.

If newer local file content is explicitly provided in chat for a Listed test file, that newer local content becomes the authoritative source for that file for this response.

Assistant MAY read `tests/CMakeLists.txt` when needed to describe target linkage or target placement accurately.

Assistant MAY read the directly relevant production header/source files when needed to describe what the test covers accurately.

## 3. Existing entry rule

If the Listed test file already has an entry in the relevant shard:
- Assistant MUST update that existing entry;
- Assistant MUST NOT regenerate a completely unrelated replacement from scratch;
- Assistant MUST preserve stable coverage statements that remain correct;
- Assistant MUST reflect the actual current test file contents.

## 4. New entry rule

If the Listed test file is a New listed test file:
- Assistant MUST select the proper shard using `tests_file_map_index.md`;
- Assistant MUST generate a new entry in the style of the surrounding shard.

## 5. Output

For each Listed test file Assistant MUST provide:

- the shard path;
- whether the entry must be:
    - replaced;
    - or added;
- one full copy-ready entry block for that file.

Assistant MUST NOT return scattered bullet fragments for one entry.
Assistant MUST return the whole entry block for that file.

## 6. Scope limits

By default this mode covers only the files explicitly listed by the user.

Assistant MUST NOT silently update unrelated entries unless:
- they are necessary to keep the selected entry accurate;
- and the assistant explicitly states that those adjacent entry updates are also required.