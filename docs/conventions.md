# ST2110-OBS-PLUGIN — Conventions

## 0. Definitions

- Project copy of this file = authoritative runtime copy.
- If this file conflicts with `plan_rules.md`, `plan_rules.md` wins.
- If this file conflicts with the current mode file, the current mode file wins.
- Terms such as **MUST**, **MUST NOT**, **Actually read**, **Copy-ready**, **MTL task**, **Production-file selection path**, and **Test-file selection path** inherit the meanings defined in `plan_rules.md`.

## 1. Copy-ready output

All repository-facing output MUST be copy-ready.

Rules:
- new production file → full file unless the current mode explicitly says otherwise;
- existing production header / source → only the blocks required by the current mode;
- unchanged declarations MUST NOT be resent unless the current mode explicitly requires a full structure or full file;
- existing inline bodies MUST NOT be resent unless the current mode explicitly requests implementation changes;
- file path MUST be stated for every returned code/document block;
- inserted/replaced blocks MUST be grouped by file, not scattered.

## 2. API/output formatting

Unless the current mode defines a stricter format, Assistant SHOULD:

- group output by file;
- explicitly label each block as:
  - add;
  - replace;
  - update;
  - insert into existing class / struct;
- keep declarations copyable without reconstruction;
- keep comments concise and architecture-facing rather than explanatory prose inside code blocks.

Declarations intended for user implementation SHOULD end with `;`.

Required helper declarations MUST NOT be omitted when they are part of the intended architecture.

## 3. Replaceable documentation blocks

For repository `.md` updates, Assistant MUST provide replaceable blocks rather than scattered line fragments unless the user explicitly asks for a diff.

Default rules:
- map entries → full entry block;
- shard updates → full new/updated shard entry blocks;
- `plan.md` task/subsection updates → full replaceable block in the current file’s established format;
- full-file replacement only when:
  - explicitly requested by the user;
  - or when the file behavior/rule set is globally reshaped.

## 4. Test-file output

When the current mode requires a full test file, Assistant MUST send the full `.cpp`.

When a new test target is actually required, Assistant MUST provide the exact `add_st2110_test(...)` line in the same style as the existing file.

Assistant SHOULD prefer updating an existing test file over creating a new one unless the current mode or subsystem split clearly requires a separate file.

## 5. Plan/status hygiene

Default `plan.md` workflow:
- completed task → `[x]` where declared;
- no default move to `## Done`;
- no duplicate completion marking.

Assistant MUST NOT assume a `## Done` workflow unless the user explicitly wants it.

## 6. Response discipline

Assistant MUST keep architectural obligations, workflow obligations, and formatting obligations separate in the response.

Assistant MUST NOT:
- hide missing required reading;
- imply a full check when only maps or snippets were read;
- merge unrelated files into one oversized replacement unless the user explicitly requests that style.