# ST2110-OBS-PLUGIN — Interaction mode: ОБНОВЛЕНИЕ code_map.md

## 0. Definitions

- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Fully read**, **Copy-ready**, and **Production-file selection path** inherit the meanings defined in `plan_rules.md`.
- **Listed file** = a `.hpp` file explicitly named by the user for this mode.
- **New listed file** = a listed file marked by the user with `(новый)`.

## 1. General mode rules

This mode updates production-map entries only.

It does NOT perform:
- task design;
- implementation review;
- test generation;
- test-map updates.

Assistant MUST update shard entries, not generate an unrelated fresh map from scratch.

## 2. Mandatory reading

For each Listed file Assistant MUST actually read:

- `plan_rules.md`;
- `conventions.md`;
- this file `interaction_mode_update_code_map.md`;
- `code_map_index.md`;
- the relevant `code_map_shard_*.md` that currently contains the entry, or that should contain it for a new file;
- the actual Listed file from the primary repository.

If newer local file content is explicitly provided in chat for a Listed file, that newer local content becomes the authoritative source for that file for this response.

Assistant MAY read directly connected production files when needed to describe:
- role;
- relationships;
- public entities;
- architectural boundaries.

## 3. Existing entry rule

If the Listed file already has an entry in the relevant shard:
- Assistant MUST update that existing entry;
- Assistant MUST NOT regenerate a completely unrelated replacement from scratch;
- Assistant MUST preserve stable information that is still correct;
- Assistant MUST reflect the actual current file contents.

## 4. New entry rule

If the Listed file is a New listed file:
- Assistant MUST select the proper shard using `code_map_index.md`;
- Assistant MUST generate a new entry in the style of the surrounding shard.

## 5. Output

For each Listed file Assistant MUST provide:

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