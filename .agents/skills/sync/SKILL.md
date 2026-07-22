---
name: sync
description: 현재 브랜치를 develop 최신 상태와 동기화. sync, update, develop merge, push 요청 시 stash 보호 적용.
---

# Sync

Use this skill to update the original working branch from `develop` while preserving local uncommitted work.

## Workflow

1. Run `git status --short --branch` and record the current branch as the original branch.
2. Stash local changes from the original branch only when there are uncommitted changes.
   - Use a descriptive stash message such as `sync/<original-branch> before develop merge`.
   - Record whether this workflow created a new stash.
3. Switch to `develop` and pull the latest remote changes.
   - Prefer `git switch develop`.
   - Run `git pull --ff-only origin develop`.
   - Stop if the pull fails.
4. Switch back to the original branch and merge `develop`.
   - Run `git switch <original-branch>`.
   - Run `git merge develop`.
   - Stop for user resolution if the merge conflicts.
5. Push the original branch.
   - Verify `git remote -v` before pushing.
   - Push with `git push origin <original-branch>` unless the branch has a different configured upstream.
   - Never force-push unless the user explicitly requests it.
6. Stash pop.
   - Only run `git stash pop` if step 2 created a stash in this workflow.
   - If no stash was created, report that stash pop was skipped to avoid applying an older stash.
   - If `stash pop` conflicts, stop and report the conflicted files.
7. Run final `git status --short --branch` and report the branch, push result, merge result, and stash-pop result.

## Guardrails

- Do not merge the original branch into `develop`.
- Do not commit directly on `develop`.
- Do not delete branches.
- Do not use `git reset --hard`, `git checkout -- <path>`, or force-push.
- If the original branch is `main`, `develop`, or `snapshot/*`, confirm intent before continuing unless the user explicitly named that branch.
