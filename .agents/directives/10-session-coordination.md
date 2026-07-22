# 10. Session Coordination Directive for AI Agents

This directive governs how multiple AI sessions coordinate edits in the same repository.

## Purpose

Multiple sessions may work in this project at the same time. Before editing repo-tracked files, each session must publish a gitignored active-session manifest that lists its intended and current edit paths.

This is advisory coordination, not an atomic lock. It reduces accidental overlap but does not replace separate worktrees or branches for large parallel work.

## Manifest Location

Use one manifest per active session:

```text
.agents/work/active-sessions/<session-id>.md
```

`.agents/work/` is gitignored. Do not stage or commit active-session manifests.

Choose a session id that is stable for the task, such as:

```text
codex-260607-1530-ai-directives
claude-260607-1530-player-input
gemini-260607-1530-map-review
```

## Manifest Format

Use this markdown shape:

```markdown
# Active Session: <session-id>

- Agent: <agent/tool name if known>
- Branch: <current branch>
- Status: planning | editing | blocked | done
- Task: <short task summary>
- Started: <ISO-8601 timestamp>
- Last updated: <ISO-8601 timestamp>

## Intended Edit Paths

- path/from/repo/root.ext

## Currently Edited Paths

- path/from/repo/root.ext

## Notes / Blockers

- <optional note>
```

Paths must be repo-relative and use forward slashes.

## Required Workflow

Before editing repo-tracked files:

1. Read all manifests under `.agents/work/active-sessions/*.md`.
2. Ignore manifests whose status is `done`.
3. Normalize all paths to repo-relative forward-slash paths.
4. Compare other sessions' intended and currently edited paths against this task's planned edit set.
5. If no conflict exists, create or update this session's manifest before editing.
6. Update the manifest whenever the edit scope changes.
7. After finishing, mark the manifest `done` or delete it.

Do not create coordination manifests for temporary scratch files under `.agents/work/`.

## Conflict Rules

Halt before editing if another active manifest overlaps with this task.

Treat these as conflicts:

- Exact same file path.
- Parent/child path overlap, such as editing `Source/` while another session edits `Source/Pragmata_2P/Foo.cpp`.
- Same binary Unreal asset path, including `.uasset` and `.umap`.
- Same config, project metadata, directive, manifest, or documentation file.
- Any broad operation that may touch paths listed by another session.

When a conflict exists:

1. Report the conflicting session id, task summary, status, and paths.
2. Do not edit the conflicting files.
3. Wait until the conflicting manifest is removed, marked `done`, or the user explicitly directs how to proceed.

If a manifest appears stale, do not ignore it automatically. Report the stale-looking manifest and ask before proceeding.

## Cleanup Rules

- Mark the manifest `done` or delete it after the task is complete.
- If the task is abandoned or blocked, set status to `blocked` and explain the blocker.
- Never commit active-session manifests.
