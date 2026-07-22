---
name: unstage
description: 지정한 큰 Unreal asset을 Git 추적에서 제외하고 gd-upload에 복사. unstage, ignore, Google Drive 이동 요청 시 사용. 경로 인수 필수.
---

# Unstage

Keep a specified asset out of normal Git history while preserving the local file and preparing it for Google Drive upload.

## Required Argument

This skill must be called with a repo-relative filename/path argument.

- If the user provides no path, ask for the filename/path and stop.
- Accept exact repo-relative paths such as `Content/Character/Hugh/SM_Hugh.uasset`.
- Do not guess among multiple candidate assets.

## Workflow

1. Read `AGENTS.md` and the required directives, especially `.agents/directives/12-unstaged-assets.md`.
2. Verify active session manifests before editing tracked files.
3. Ask the user which `gd-upload` copy mode to use:
   - `NewOnly` (default): copy only the specified newly unstaged asset into `gd-upload/<repo-relative-path>`.
   - `All`: rebuild the complete `gd-upload/` folder and `unstaged-assets-content.zip` from `Docs/Operations/unstaged-assets.md` via `gen-gd`.
4. If the user does not choose a mode and continuing is appropriate, use `NewOnly`.
5. Run the bundled script from the repository root:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .agents/skills/unstage/scripts/unstage.ps1 -Path "Content/..." -CopyMode NewOnly
   ```

6. Inspect the resulting diffs in `.gitignore` and `Docs/Operations/unstaged-assets.md`.
7. Report the newly unstaged file clearly so the user can upload only that file when useful:
   - source path in the project, such as `Content/.../Asset.uasset`
   - copied upload path, such as `gd-upload/Content/.../Asset.uasset`
   - file size
   - copy mode used
   - full bundle zip path only when `CopyMode All` refreshed it
8. Verify:
   - source asset still exists on disk
   - `git check-ignore -v --no-index -- <path>` reports the `.gitignore` rule
   - `git status --short -- <path>` shows no staged content for ignored untracked files, or staged deletion only when the file was already tracked in `HEAD`
   - `gd-upload/<repo-relative-path>` exists and matches the source file size
   - for `CopyMode All`, `gd-upload/unstaged-assets-content.zip` has entries beginning with `Content/...`

## Guardrails

- Never delete the source asset from disk.
- Only use `git rm --cached -- <path>` for tracked/indexed files.
- Do not commit unless the user explicitly asks.
- Do not delete or empty `gd-upload/` unless the user explicitly asks.
- Default to copying only newly unstaged assets so users can empty `gd-upload/` first and get a clean folder containing only the new upload payload.
- Use `CopyMode All` only when the user wants the complete upload bundle refreshed.
- Always point out the single copied file under `gd-upload/`.
