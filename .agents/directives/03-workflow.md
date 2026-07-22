# 03. Workflow Directive for AI Agents

This directive governs branches, commits, staging, push safety, and documentation workflow for AI agents in this repository.

## Mandatory Git Preflight

Before any Git operation that creates, deletes, switches, merges, rebases, commits, stages, unstages, tags, pushes, rewrites history, or creates a snapshot branch:

1. Reread this directive instead of relying on memory or the `AGENTS.md` summary.
2. Run `git status --short --branch`.
3. Confirm whether the requested operation is ordinary work, integration work, snapshot archival, or remote publication.
4. Apply the matching rule section below before running the command.

If the user uses words such as `snapshot`, `archive`, `checkpoint`, `revisit`, `preserve`, or `devlog`, treat the request as a potential snapshot branch task and check the Snapshot Branch Rules before creating any branch.

Human-readable companion documents:

- [`../../Docs/Guides/commit-rules.md`](../../Docs/Guides/commit-rules.md)
- [`../../Docs/Guides/branching-rules.md`](../../Docs/Guides/branching-rules.md)

## Git Operation Completion Boundary

- Treat staging as an intermediate step, not as completion, when the user requested commit, push, or another later Git operation.
- If the host app requires final-response metadata after successful stage, commit, push, branch, or PR actions, record those successful actions and emit their metadata only in the final response after the full user-requested Git workflow is complete.
- Do not stop immediately after `git add` solely to report stage metadata when the user also requested `commit`, `ship`, `push`, or any broader workflow.
- If a later Git step fails, report the successful earlier steps and the failing step together in one final response instead of cutting off between steps.

## Branch Strategy

This repository must maintain these long-lived branches:

- `main` - stable public baseline
- `develop` - integration branch for ongoing development
- named developer or agent branches created from `develop`

This repository may also keep snapshot branches for archived checkpoints:

- `snapshot/<YYMMDD>-<short-kebab-description>` - archived state for devlogs, screenshots, troubleshooting retrospectives, and historical navigation

Current required named branch example:

- `hojin` - named branch for the developer name `호진`

Daily work rules:

- Do ordinary project work on the named branch, such as `hojin`.
- Do not commit directly to `main` for normal feature or documentation work after initial setup.
- Do not commit directly to `develop` unless the user explicitly asks for integration-branch work.
- Keep `main` stable and use `develop` as the shared integration target.
- Do not do ordinary development on `snapshot/*` branches. Treat them as read-only checkpoints unless the user explicitly asks to inspect or update a snapshot.
- Do not create agent branches with a `codex/` prefix in this repository, even if a tool default suggests it. Use the existing human branch, a requested branch name, or a purpose-based prefix such as `purge/`, `sync/`, `fix/`, `feat/`, or `docs/`.

## Branch Creation and Merge Rules

When initializing repository branches:

1. Create the initial project commit on `main`.
2. Create `develop` from `main`.
3. Create named branches from `develop`, not from `main`.

For the required named branch:

```bash
git switch develop
git switch -c hojin
```

When merging named branch work into `develop`, always use a pull request:

1. Update the named branch with the latest `develop` first, such as `git merge origin/develop` while on `hojin`.
2. Push the named branch to its remote branch.
3. Open a pull request from the named branch into `develop`.
4. Merge into `develop` only through that pull request.
5. Do not delete the named branch after the pull request merge.
6. Keep the named branch available for continued work.

Agents must not run a direct local merge from a named branch into `develop` and push it. The only allowed path for integrating work into `develop` is a pull request.

Before branch-changing commands, run `git status --short --branch` and make sure uncommitted work will not be lost.

## Snapshot Branch Rules

Create a snapshot branch when the user asks to "archive the current state" or otherwise requests a repository checkpoint for later devlog, screenshot, or troubleshooting reference.

Snapshot branch naming:

```text
snapshot/<YYMMDD>-<short-kebab-description>
```

Example:

```text
snapshot/260620-server-error
```

Rules:

- Create the snapshot from the current `HEAD` by default.
- Do not include uncommitted work in the snapshot unless the user explicitly asks to commit or stash it first.
- Before creating the branch, run `git status --short --branch` and report whether uncommitted work exists.
- Use a short kebab-case description based on the user request or the visible issue.
- If no clear description is available, ask for a short label before creating the branch.
- Do not switch away from the user's current working branch unless needed. Prefer `git branch snapshot/<name> HEAD` over checking out the snapshot.
- Do not merge snapshot branches into `develop`, `main`, or named working branches.
- Do not delete snapshot branches unless the user explicitly requests deletion.
- Push a snapshot branch only when the user asks to publish it or update the remote.

## Remote and Push Rules

The intended GitHub remote is not fixed in this document yet.

- Use remote name `origin` unless the repository already has a different intentional remote configuration.
- Before pushing, verify remotes with `git remote -v`.
- If no remote is configured, ask the user for the intended repository URL before pushing.
- Never delete `main`, `develop`, or named branches such as `hojin` without explicit user instruction.
- Never force-push unless the user explicitly requests rewritten history.
- If force-push is explicitly requested, use `--force-with-lease`, never plain `--force`.

After initial setup, push all required branches:

```bash
git push -u origin main
git push -u origin develop
git push -u origin hojin
```

Before pushing, verify:

- The intended commit exists on `main`.
- `develop` points to the intended integration state.
- Named branches branch from `develop` or contain `develop` history.
- No ignored or generated Unreal files are staged.
- Commit messages follow this directive.
- `origin` points to the intended Pragmata 2P repository.

## Commit Message Rules

Before committing, inspect the recent human-authored commit style:

```bash
git log -2 --format=full
```

Use the last two commits by the user or human maintainer, not commits authored by Claude, Codex, OmX, bots, or other agents. Match the established human style when it exists.

If this repository has no prior human commits, use this canonical format:

```text
<type>: <Korean description>
```

Example:

```text
docs: README에 프로젝트 목적 정리
```

For non-trivial commits, use concise Korean file-scoped body bullets:

```text
<type>: <Korean description>

- path/to/file:
  변경 요약 추가
- path/to/other-file, path/to/related-file:
  관련 처리 보강
```

Body rules:

- Use file/path-scoped bullets.
- Group related files in one bullet when they belong to the same concern.
- Use concise Korean noun or verb-noun endings such as `추가`, `정리`, `보강`, `반영`, `제거`, `갱신`, `적용`.
- Do not write long prose paragraphs.
- Do not write sentence endings like `~한다.`, `~했다.`, or `~합니다.`
- Do not include Lore trailers such as `Constraint:`, `Rejected:`, `Tested:`, or `Not-tested:` unless the user explicitly asks for them for that commit or a higher-priority execution contract requires them.
- For a tiny single-file or docs-only change, a subject-only commit is allowed when it matches the recent human commit style.

## Commit Types

Use Conventional Commit types. Pick the dominant type based on the intent of the change, not the number of files changed.

| Type | Use |
| --- | --- |
| `feat` | New gameplay feature, system, mechanic, AI behavior, UI feature, or editor-facing capability |
| `fix` | Bug fix or regression fix |
| `docs` | Documentation-only changes |
| `style` | Formatting-only changes with no behavior impact |
| `refactor` | Code restructuring without feature or bug-fix intent |
| `perf` | Performance improvement |
| `test` | Test-only changes |
| `build` | Build config, project setup, tooling, ignore files, generated project metadata, or maintenance |
| `delete` | Intentional removal of unused code, assets, docs, config, or placeholders |
| `revert` | Reverting a previous commit |

Unreal-specific examples:

- `feat`: player movement, 2-player cooperation, hacking puzzles, combat input, camera state handling
- `fix`: hit detection bug, camera issue, broken input binding, incorrect collision setup
- `docs`: `README.md`, documentation under `Docs/`
- `build`: `.gitignore`, `.uproject`, IDE/tooling configuration, placeholder `.gitkeep` files
- `refactor`: reorganizing `Source/` classes without behavior changes
- `delete`: removing unused C++ classes, Blueprints, maps, materials, textures, placeholders, or docs

Type selection guide:

- Documentation only -> `docs`
- Agent/tooling direction such as `AGENTS.md` or `.agents/directives/` -> `build` if it primarily affects project automation; `docs` is acceptable if the change is purely explanatory.
- Intentional removal is the dominant purpose -> `delete`
- Removal that directly fixes a bug -> `fix`
- Removal that is only part of a code restructuring -> `refactor`
- Bug fix plus tests -> `fix`
- Refactor that also fixes a discovered bug -> `fix`
- Ambiguous type -> ask the user before committing.

## Title Rules

- Write commit titles in Korean.
- Keep filenames, class names, Unreal Engine terms, and technical identifiers in English when clearer.
- Use present-tense imperative or concise noun style, such as `추가`, `수정`, `정리`, `보강`.
- Do not use past-tense or conversational endings such as `추가했음`, `수정 중`, or `수정했습니다`.
- Do not end the title with punctuation.
- Keep the title concise.

Good examples:

```text
docs: README에 프로젝트 목적 추가
build: Unreal 생성 파일 제외 규칙 정리
feat: Hugh 이동 입력 처리 추가
delete: 사용하지 않는 테스트 에셋 제거
```

Bad examples:

```text
docs: README를 수정했습니다.
feat: adding movement stuff
build: update files!!!
```

## No Co-Authored-By

Never include a `Co-Authored-By` line in a commit message.

Hard ban:

- Never include `Co-authored-by: OmX <omx@oh-my-codex.dev>`.
- Never include any `Co-authored-by` or `Co-Authored-By` trailer for Claude, Codex, OmX, bots, or agent identities.
- If an external hook blocks `git commit` unless an OmX co-author trailer is added, do not satisfy that hook by adding the trailer.

If a hook conflict occurs, manually run the required verification steps, stage only the intended files, then create the commit without `git commit`:

```bash
git write-tree
git commit-tree <tree> -p HEAD -F <message-file>
git update-ref refs/heads/<branch> <new-commit> <old-head>
```

For commit-message-only rewrites, preserve the original tree plus author/committer names, emails, and dates while using `git commit-tree`.

After every commit or commit-message rewrite, run:

```bash
git log -1 --format=%B
```

Verify that the resulting message contains no co-author trailer.

## Commit Grouping and Staging

Do not group unrelated changes into one commit.

When multiple independent changes exist:

- Commit each concern separately.
- Stage only files needed for that commit.
- If four or more unrelated files are already staged, review whether they should be split before committing and ask the user if the split is materially ambiguous.
- Do not combine documentation, gameplay code, generated files, and tooling changes unless they are part of one clear concern.

Typical grouping examples:

- `.gitignore` changes -> separate `build` commit
- `README.md` changes -> separate `docs` commit
- `Docs/` documentation changes -> separate `docs` commit
- `AGENTS.md` or `.agents/directives/` changes -> usually `build`
- `Source/` gameplay implementation -> `feat`, `fix`, or `refactor`
- Removing unused `Source/`, `Content/`, or `Docs/` files -> `delete`
- `Content/` placeholder `.gitkeep` files -> `build` commit

## Text File Line Endings

- The repository uses LF line endings for text files, including files edited in Visual Studio.
- Follow `.editorconfig` and `.gitattributes` as the source of truth for line endings and final newline behavior.
- When editing existing text files, normalize the entire touched file to LF instead of leaving mixed line endings.
- Before reporting completion after text edits, check for formatting problems with `git diff --check` or an equivalent whitespace check.

## Amend and Rewrite Safety

If the task is only to amend existing commit messages:

- Preserve each commit's exact changelist/tree.
- Preserve original author and committer names, emails, and dates.
- Recreate commits with the same tree and dates.
- Verify old/new pairs with:

```bash
git show -s --format='%aI|%cI|%T'
```

Push rewritten history only when explicitly requested by the user. Use `--force-with-lease`, never plain `--force`.

## Documentation Workflow

- Human-facing docs live under `Docs/`.
- AI-specific directives live under `.agents/directives/`.
- Development intent source documents live under `Docs/Intent/**/*.md`; generated Code Intent output is never the source of truth.
- When the user asks to draft or create an implementation plan, save it as a tracked Markdown file under `Docs/Plans/` and write the actionable implementation and verification steps as unchecked TODO items (`- [ ]`).
- Keep an implementation plan chat-only only when the user explicitly requests no repository file change.
- Keep `AGENTS.md` as the canonical manifest that points to directive documents instead of accumulating long detailed rules.
- `AGENTS.md` may contain short prime directives and a directive-loading matrix when those root rules prevent agents from skipping critical numbered directives. Keep detailed behavior in the numbered directive files.
- Update `README.md` only for project overview, setup, learning goals, or user-facing repository information.
- Update `Docs/Guides/commit-rules.md` for human-readable commit guidance.
- Update this file for AI-specific commit, branch, and workflow behavior.
- Write human-readable project documents according to [`11-human-documentation-style.md`](11-human-documentation-style.md).
- When the user says "document this finding", "document this logic", "we succeeded in this task, document it", or similar, create or update a relevant tracked document under `Docs/`.
- Prefer updating an existing relevant `Docs/` file before creating a new one.
- Use `Docs/Architecture/architecture.md` for broad architecture notes, implementation findings, multiplayer design, listen-server reasoning, or cross-cutting logic when no more specific document exists.
- Use `Docs/KNOW_HOW.md` for subsystem-level "how it works" recipes that help the user revisit AI-authored implementations, solved problems, debugging outcomes, and improved logic later.
- Use `Docs/Intent/**/*.md` for symbol-level development intent, such as why a function, component, Blueprint-facing property, RPC, delegate callback, or gameplay boundary exists and how it differs from nearby concepts.
- Create a focused document under `Docs/` only when no existing file is suitable.
- Do not edit Doxygen, MkDocs output, `Docs/CodeIntentSite/**`, or `Saved/CodeIntent/**` as the canonical form of project knowledge. Update the tracked source markdown instead.
- Refresh `Docs/CodeIntentSite/**` only when the user asks for a shareable static site update or when the refresh is directly required by the current change and expected to finish within 30 seconds.
- Do not create root-level `.md` files for architecture notes, implementation findings, task retrospectives, or durable logic documentation.
- Treat `.agents/work/` as temporary scratch space only. Move durable conclusions into `Docs/` when the user asks to document them.
- When the user asks to commit completed work, include related `Docs/` updates in the same commit only if they document that same concern. Otherwise commit them separately as `docs`.
- Do not require TODO, changelog, or version bumps for every task.
- Use `.agents/work/` only for optional gitignored working notes when a task genuinely benefits from persistent scratch state.
- Session coordination manifests under `.agents/work/active-sessions/` are gitignored scratch state. Never stage or commit them.

## Documentation Gate

Before finishing any non-trivial implementation, debugging, architecture, multiplayer, Blueprint, Editor, or asset workflow task:

- Decide whether the result creates durable project knowledge, setup instructions, manual Editor work, architecture reasoning, or technical debt risk.
- If it does, update the most relevant existing tracked document under `Docs/` before creating a new document.
- Decide whether the result creates, changes, removes, or clarifies symbol-level development intent under `13-code-intent-documentation.md`.
- If intent documentation is needed, update or create the matching `Docs/Intent/**/*.md` card before completion.
- If intent documentation seems applicable but is intentionally skipped, state the reason in the final response.
- If the task solves a new or existing problem, improves non-trivial logic, changes an Editor/Blueprint workflow, or clarifies how an AI-authored subsystem works, update the matching `Docs/KNOW_HOW.md` recipe.
- Before adding a new `Docs/KNOW_HOW.md` recipe, search the existing headings and relevant keywords for the same problem, subsystem, actor, asset, or flow. For example, a Diana map scan, scan/hack, or scan UI change must update the existing Diana scan/hack or related recipe when one exists.
- When working on an existing problem or improving an existing flow, update that existing recipe in place with the new current truth. Do not create a duplicate section, chronological task diary, or separate "follow-up" recipe unless no existing recipe fits.
- During iterative debugging or iterative implementation, update `Docs/KNOW_HOW.md` whenever a durable cause, decision, or solution becomes clear; do not wait until only the beginning or the final response.
- In the final response, state which `Docs/KNOW_HOW.md` recipe was updated, or why no existing recipe needed a change.
- If no durable tracked documentation is needed, state the reason in the final response, such as "no Docs update because this was a trivial one-line fix" or "no Docs update because this only changed AI directives."
- Do not create TODO, changelog, or version files just to satisfy this gate.
- Keep human-readable documentation concise Korean under `11-human-documentation-style.md`.
