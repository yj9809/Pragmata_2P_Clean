---
name: ship
description: 현재 변경사항 커밋. $ship, ship, commit 요청 시 no-coauthor와 커밋 분리 규칙 적용.
---

# Ship

Commit the current unstaged changes following these rules strictly.

If the user provides extra text after `$ship`, treat it as commit scope or message guidance.

## Completion Boundary

- Staging is not the end of `$ship`; it is only an intermediate step before commit.
- If the user explicitly asked to push as part of the same request, continue through the requested push after committing.
- If the host app requires final-response directives or metadata for successful stage/commit/push actions, collect them and emit them together in the final answer after all requested actions are done.
- Never stop immediately after staging solely to report stage metadata when the user requested commit, ship, push, or any broader Git workflow.

1. **No Co-Authored-By**: Never include a Co-Authored-By line in the commit message.

    **Hard ban details:**
    - Never include `Co-authored-by: OmX <omx@oh-my-codex.dev>`.
    - Never include any `Co-authored-by` / `Co-Authored-By` trailer for Claude, Codex, OmX, bot, or agent identities.
    - If an external hook blocks `git commit` unless an OmX co-author trailer is added, do **not** satisfy that hook by adding the trailer.
    - In that conflict case, manually run the required verification and formatting steps, stage only the intended files, then create the commit without `git commit`:

        ```bash
        git write-tree
        git commit-tree <tree> -p HEAD -F <message-file>
        git update-ref refs/heads/<branch> <new-commit> <old-head>
        ```

    - For commit-message-only rewrites, preserve the original tree plus author/committer names, emails, and dates while using `git commit-tree`.
    - After every commit or commit-message rewrite, run `git log -1 --format=%B` and verify there is no co-author trailer.

2. **Commit message format**: Read the last two commits by the user (not by another collaborator, Claude, Codex, or any bot) via `git log` and match their style. The default format is:

    ```text
    <type>: <Korean description>
    ```

    **Commit body format (mandatory for any non-trivial commit):** Use concise Korean file-scoped bullets:

    ```text
    <type>: <Korean description>

    - path/to/file:
      변경 요약 추가
    - path/to/other-file, path/to/test:
      관련 테스트와 호출 경계 보강
    ```

    Body rules:
    - Use file/path-scoped bullets. Group related files in one bullet when they belong to the same concern.
    - Use concise Korean statement-like noun/verb-noun endings such as `추가`, `정리`, `보강`, `반영`, `제거`, `갱신`, `적용`.
    - Do **not** write prose sentences like `~한다.`, `~했다.`, `~합니다.`, or long rationale paragraphs.
    - Do **not** include extra trailers such as `Constraint:`, `Rejected:`, or `Tested:` unless the user explicitly asks for them for that commit.
    - For a tiny single-file/docs-only change, a subject-only commit is allowed only when the last two human commits also use subject-only style.
    - Before committing, run `git log -2 --format=full` and compare the drafted message against the recent human commit style. If it does not match, rewrite it before `git commit`.

    **Commit type table** (Conventional Commits spec — pick the most accurate type; if multiple apply, pick the dominant one based on the intent of the change, not the file count):

    | Type       | Meaning                                      | Use when |
    | ---------- | -------------------------------------------- | -------- |
    | `feat`     | New functionality                            | Adding a user-facing feature, UI, API, tool, or gameplay capability |
    | `fix`      | Bug fix                                      | Fixing a regression, broken behavior, crash, or incorrect output |
    | `docs`     | Documentation-only change                    | Changing README, docs, comments, or human-facing documentation only |
    | `style`    | Formatting-only change                       | Formatting, whitespace, lint formatting, or no-behavior style changes |
    | `refactor` | Structural code change without feature/fix   | Renaming, extracting functions, reorganizing code, or type cleanup |
    | `perf`     | Performance improvement                      | Reducing runtime cost, memory use, file size, or repeated work |
    | `test`     | Test-only change                             | Adding or updating tests and fixtures only |
    | `chore`    | Build, tooling, config, or agent metadata    | Dependency, CI, agent directive, build script, or config changes |
    | `revert`   | Reverting a previous commit                  | `git revert` results |

    **Type selection guide**:
    - Docs only → `docs`.
    - Agent behavior, tooling, or workflow metadata → `chore` when it is closer to project tooling than human documentation.
    - Bug fix plus tests → `fix`.
    - Refactor that also fixes a discovered bug → `fix` if the final intent is bug correction.
    - If still ambiguous, ask the user.

    **Title rules:**
    - Use imperative present tense.
    - Start the title content lowercase after `<type>:` when writing English.
    - Do not end with punctuation.
    - Prefer Korean descriptions when the project's directives ask agents to answer or document in Korean. File names, proper nouns, and technical terms may remain in English.

    **Amend/rewrite safety:** If the task is only to amend existing commit messages, preserve each commit's exact changelist/tree and original author + committer datetime. Recreate commits with the same tree and dates, then verify old/new pairs with `git show -s --format='%aI|%cI|%T'`. Push rewritten history only when explicitly requested, and use `--force-with-lease`, never plain `--force`.

2a. **Commit grouping**: 여러 무관한 변경을 하나의 commit에 묶지 말 것. 기능별·관심사별로 분리해 commit. 한 번에 4개 이상의 무관한 파일이 staged 상태면, 분리 가능 여부를 검토하고 필요하면 user에게 확인.

    **여러 독립 변경이 동시에 있을 때**:
    - 각 변경을 별도 commit으로 순차 처리
    - commit마다 해당 변경에 필요한 파일만 stage
    - 첫 번째 commit을 만든 뒤 남은 변경이 있으면, 같은 절차를 반복해 다음 commit 생성
    - 서로 다른 변경을 하나의 commit message나 release note로 합치지 말 것

4. **Update branch-specific PR file if the project uses one**: If project directives require a branch-specific PR file and the current branch is not the default branch, update the expected PR file with a concise entry describing what changed. Never create or update a generic root `PR.md` unless the active project explicitly requires it.

6. **Commit only, do NOT push**: Stage relevant files, commit, and stop. Do not run `git push` unless explicitly prompted by the user.

Before committing, follow any active project directives for validation, generated-file boundaries, large-asset policy, and staging safety.
