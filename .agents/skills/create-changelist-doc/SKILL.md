---
name: create-changelist-doc
description: Create a Korean, reviewer-friendly changelist document under `Docs/Create/` that records exactly what code or documentation changed for a task, including file-by-file summaries, before/after snippets, notes, and verification. Use when the user asks to document exact code changes, make a Docs/Create changelist, preserve a task's implementation details for review, or produce a git-style before/after change document.
---

# Create Changelist Doc

## Goal

Create a human-readable changelist document that lets a reviewer understand the task without opening every changed file.

Output location:

```text
Docs/Create/<YYYYMMDD-HHMMSS>-<short-task-title>.md
```

## Workflow

1. Identify the exact task scope.
   - Use the user's requested task title when available.
   - If the task is already committed, inspect the relevant commit with `git show`.
   - If the task is uncommitted, use `git diff`, `git diff --cached`, and `git status --short`.
   - Exclude unrelated dirty files explicitly.

2. Determine changed files.
   - Prefer `git diff --name-only` for uncommitted work.
   - Prefer `git show --name-only --format=` for a committed task.
   - Group related files by system when many files changed.
   - Do not document `.agents/work/active-sessions/` manifests.

3. Capture before/after evidence.
   - Use actual code snippets from the diff.
   - Keep snippets short enough for review.
   - Include deleted code in `이전`.
   - Include new or final code in `이후`.
   - For large rewrites, show the decisive call path or API surface, not the whole file.

4. Write in concise Korean.
   - Keep identifiers, file paths, class names, functions, Unreal terms, and commands in English.
   - Use headings such as `핵심 변경`, `변경 파일 목록`, `무엇이 바뀜`, `이전`, `이후`, `비고`, `검증 결과`.
   - Prefer tables and bullets over long prose.
   - Avoid polite endings such as `~합니다`, `~했습니다`, `~됩니다`.

5. Add verification.
   - Record commands already run for the task.
   - If no validation was run, state the remaining manual validation.
   - Separate false positives from real issues.

6. Validate the document.
   - Run `git diff --check -- Docs/Create/<file>.md`.
   - Search for stale English template labels if the document should be Korean:

```powershell
rg -n "Summary|Generated|Changed files|Before|After|Notes|Verification" Docs/Create/<file>.md
```

## Required Shape

Use this structure unless the user asks for a different format:

````markdown
# <Korean task title>

생성 시각: <YYYY-MM-DD HH:MM:SS>

## 핵심 변경

- <task-level change>
- <task-level change>

## 변경 파일 목록

| 파일 | 변경 요약 |
| --- | --- |
| `path/to/file.cpp` | <short summary> |

## `path/to/file.cpp`

### 무엇이 바뀜

- <file-level change>

### 이전

```cpp
<actual previous snippet>
```

### 이후

```cpp
<actual final snippet>
```

### 비고

<review note, migration note, or caveat>

## 검증 결과

### <validation name>

```text
<command>
```

결과:

```text
<result>
```
````

## File Selection Rules

- Include only files that belong to the requested task.
- Mention known unrelated dirty files near the top, but do not include them as changed files.
- Include config redirects when they preserve renamed Blueprint-facing properties.
- Include documentation files when they were changed to reflect the code behavior.
- Include generated or binary assets only when the user explicitly asks and the diff is reviewable. Otherwise document exact Editor/manual verification steps.

## Snippet Rules

- Prefer snippets around changed functions, declarations, config entries, or doc paragraphs.
- For removed functions, place the removed function signature/body in `이전`.
- For renamed properties, include both old and new declarations plus any redirects.
- For behavior changes, include the new trigger condition or call path.
- For docs-only changes, quote the old stale line and the new corrected line.
- Do not paste huge full-file diffs.

## Naming

Use a stable, lowercase kebab-case task title:

```text
Docs/Create/20260703-161329-receive-damage-only-hitreact-refactor.md
Docs/Create/20260703-170500-hugh-movement-intent-guide.md
```

If the task title is unclear, derive it from the dominant code behavior, not from a chat phrase.

## Final Response

Report:

- created document path
- whether unrelated dirty files were excluded
- validation commands run
