---
name: gen-gd
description: Docs/Operations/unstaged-assets.md 기준 gd-upload와 복구용 zip 재생성. Google Drive 업로드 번들 갱신 요청 시 사용.
---

# Gen GD

Regenerate `gd-upload/` from `Docs/Operations/unstaged-assets.md` so the Google Drive upload bundle preserves repo-relative asset paths.

## Workflow

1. Read `AGENTS.md` and the required directives, especially `.agents/directives/12-unstaged-assets.md`.
2. Run the bundled script from the repository root:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .agents/skills/gen-gd/scripts/gen-gd.ps1
   ```

3. Verify the output:
   - every listed asset exists in `gd-upload/<repo-relative-path>`
   - `gd-upload/unstaged-assets-content.zip` exists
   - zip entries start with `Content/...`
   - `git status --short -- gd-upload` shows nothing because `gd-upload/` is ignored

## Notes

- Do not delete source assets from `Content/`.
- Do not commit unless the user explicitly asks.
- Overwrite only `gd-upload/unstaged-assets-content.zip`; preserve other zip files in `gd-upload/` because they may be user-owned artifacts.
- If the target zip is locked, report the locked file and process; do not kill processes without explicit user instruction.
