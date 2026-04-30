# ST2110-OBS-PLUGIN — Conventions

## Validation and parsing

- Parse strictly: malformed wire data must produce an explicit error, not silent coercion.
- Validate configs explicitly before use/start.
- Fallbacks/default values must be explicit at the call site or config construction layer, not hidden inside parsers or validation helpers.
- `Unsupported` should mean: the input is structurally valid enough to recognize, but this implementation does not support it yet.
- `InvalidValue` should mean: the input/config violates required constraints.
- `is_valid()` is only a convenience wrapper around `validate_*() == Error::Ok` and must not contain divergent logic.

## Extensibility

- Format-specific constraints should stay localized in helper functions or format switches.
- Adding a new format should usually require:
  - extending enum values;
  - adding a validation `case`;
  - adding tests;
  - not rewriting existing validation flow.

## API and header shaping

- Public and architectural headers should make modeled axes and support boundaries explicit.
- Helper boundaries that affect validation, dispatch, lifecycle, runtime support, or derived values should not remain implicit only in prose.
- If a new helper is required for a clean architecture boundary, it should usually exist as a named function / method / policy / adapter rather than as duplicated inline logic.
- Temporary support limitations should be expressed through explicit modeled or validation boundaries, not by silently omitting API surface that is already architecturally known to be needed.

## Assistant output format for API-bearing tasks

- When a task adds or changes production API, the assistant should provide a copy-ready API skeleton, not only a prose description.
- For each new production file, the assistant should provide the file in a form that can be copied directly into the repository with:
  - all required includes;
  - `namespace st2110`;
  - all enums, structs, classes, free functions, and helpers relevant to the task;
  - complete method/function declarations with exact signatures.
- For existing production `.hpp` files, the default output format is **not** a full replacement file.
  - The assistant should send only the new or changed API declaration blocks that are needed for the task.
  - The assistant should not resend unchanged declarations or inline implementations from the rest of the file.
  - If an existing method/helper behavior must change but its signature does not change, the assistant should describe the expected updated behavior instead of reprinting the whole file.
  - A full replacement of an existing production header is allowed only when the user explicitly asks for it.
- The default declaration form for methods and free helpers should be a declaration ending with `;`, written so the user can replace that `;` with a function body `{ ... }`.
- The assistant should not omit helper declarations that are part of the intended API or localized implementation boundary.
- When a new file is proposed, the assistant should provide the full file skeleton, not only the changed fragment.
- For every declared method/helper, the assistant should also describe the expected implementation behavior:
  - inputs and validation expectations;
  - success result / state changes / side effects;
  - failure behavior and returned errors;
  - invariants that must remain true after the call.
- Expected behavior should be specific enough that the user can implement the body without guessing intended semantics.
- Test files are always sent in full copy-ready form.
- The assistant should keep implementation bodies absent by default unless the user explicitly asks for full implementation code.

## Work status and planning hygiene

- Пользовательский workflow по `plan.md` по умолчанию такой:
  - задачи не переносятся в `## Done`;
  - выполненная задача отмечается как `[x]` по месту ее объявления в backlog / phase section;
  - дублировать одну и ту же задачу одновременно и по месту объявления, и в `## Done` не нужно.
- Ассистент не должен предлагать перенос задачи в `## Done`, если пользователь явно не попросил вести `plan.md` именно так.
- Если в `plan.md` уже существует исторический раздел `## Done`, это само по себе не означает, что новые выполненные задачи нужно туда переносить.
- При проверке или обновлении `plan.md` ассистент должен сначала определить фактический workflow пользователя по текущему состоянию файла и недавним указаниям в чате, и только после этого предлагать status updates.

## Context discipline for task proposal and acceptance

- Перед формулированием следующей задачи и перед приемкой текущей реализации ассистент должен реально прочитать все project control `.md` files, а не только выборочно посмотреть один-два файла или опереться на память.
- Под “реально прочитать” понимается:
  - прочитать актуальные `plan.md`, `code_map.md`, `tests_file_map.md` из репозитория;
  - прочитать актуальные `plan_rules.md` и `conventions.md` из active Project;
  - использовать их содержимое при формулировании ответа, а не считать это формальной галочкой.
- Если хотя бы один из обязательных `.md` файлов не был прочитан в актуальной версии, ассистент не должен делать вид, что полный контекст проверен.
- Ассистент не должен утверждать или подразумевать, что “изучил правила / работы”, если не прочитал все обязательные project `.md` files для текущего шага.

## Deviations and already-planned follow-ups

- Ассистент не должен добавлять новый `Spec notes / deviations` пункт только потому, что в текущей задаче обнаружилось еще не реализованное поведение, если:
  - это поведение уже известно;
  - оно уже покрыто существующей запланированной задачей в `plan.md`;
  - текущая задача не усугубляет и не меняет характер этого ограничения.
- В таком случае ассистент должен:
  - сослаться на уже существующую follow-up задачу;
  - не дублировать это как новый deviation;
  - и не засорять `Spec notes / deviations` повторным описанием уже запланированного ограничения.
- Новый deviation нужен только если обнаружено действительно новое расхождение:
  - не отраженное в текущем `plan.md`;
  - не покрытое существующим backlog item;
  - или если текущая реализация ввела новый стандартный/архитектурный риск сверх уже известного ограничения.

## Context reading discipline

- Before proposing the next task, before accepting an implementation, and before saying that the rules/work/context were studied, the assistant must actually read the current versions of the relevant project control `.md` files.
- This includes:
  - `plan.md`;
  - `code_map.md`;
  - `tests_file_map.md` when the task touches tests or coverage;
  - the Project copies of `plan_rules.md`;
  - the Project copies of `conventions.md`.
- The assistant must not rely only on memory, earlier snippets, earlier turns, or assumptions about unchanged content.

- For this project, the assistant must also actually read all ST 2110 / RP 2110 PDF standards currently uploaded in the Project before proposing the next task or accepting the current one.
- The assistant must not read only one “obviously relevant” standard and assume the rest are unnecessary.
- If a required `.md` file or standards PDF was not actually read for the current step, the assistant must not claim that the rules/work/standards context was fully checked.