# 12. Unstaged Asset Directive for AI Agents

This directive governs requests to keep Unreal assets out of Git history while leaving the files on disk.

## When To Apply

Apply this directive when the user asks to:

- Unstage an asset.
- Ignore an asset.
- Keep an asset out of Git history.
- Add an asset to the unstaged assets list.
- Move a large binary asset to external team storage instead of normal Git.

## Required Flow

1. Read active session manifests before editing tracked files.
2. Inspect the target asset paths with `git status --short -- <paths>`.
3. Confirm the files exist on disk when practical.
4. Add exact repo-relative asset file paths to `.gitignore` with a short comment explaining why they are ignored.
5. Update `Docs/Operations/unstaged-assets.md` with the asset paths, reason, and external location.
6. Remove only the Git index entries with `git rm --cached -- <paths>` when the user asks for the files to be unstaged or ignored.
7. Never delete the asset files from disk unless the user explicitly asks for deletion.
8. Ask whether `gd-upload/` should receive only the newly unstaged asset paths or the complete unstaged asset set; default to newly unstaged paths when the user does not need a full bundle.
9. Verify with `git check-ignore -v --no-index <paths>` and `git status --short -- <paths>`.
10. Do not commit unless the user explicitly asks.

## Gitignore Policy

- Keep unstaged asset ignores in their own `.gitignore` section.
- Treat `Docs/Operations/unstaged-assets.md` as the source of truth for unstaged asset paths.
- Add one exact repo-relative file path per externally stored asset.
- Do not use folder-wide, recursive, or broad filename patterns for unstaged assets, especially Map assets.
- New Map assets must surface in `git status` so agents can identify which files are new and need Google Drive upload handling.
- Keep `.gitignore` and `Docs/Operations/unstaged-assets.md` ordered so similar file paths stay grouped together.
- When adding a new unstaged asset, update `Docs/Operations/unstaged-assets.md` first, then add the same exact path to `.gitignore`.

## Map Asset Classification

For `Content/Map/Props/` and `Content/Map/PropsWithAnim/`, only these assets are purge targets:

- Mesh assets, including files under `SM/`, `SM_*.uasset`, `SK_*.uasset`, `sm[0-9]*.uasset`, `*_unreal*.uasset`, and `*_mesh*.uasset`.
- Texture assets, including files under `Textures/`.
- Animation assets, including files under `Animations/` and files whose names include `Anim`, `Animation`, `MotionList`, `MotionBank`, or `_all_animations`.

Do not unstage these Map assets:

- `*_Skeleton.uasset`
- `*_PhysicsAsset.uasset`
- `Materials/`
- `BP_*.uasset`
- `*.umap`
- Other non-mesh, non-texture, non-animation helper assets

When a branch is used as the latest Map asset source, copy from that branch only for the approved source paths. Do not merge the source branch just to restore assets.

## Google Drive Upload Folder

When an asset is unstaged or added to the unstaged asset list:

- Keep `gd-upload/` ignored by Git.
- Treat `gd-upload/CodeIntent/` as a separate preserved area for Code Intent heavy artifacts, not as part of the unstaged asset bundle.
- Default `unstage` behavior is to copy only the newly unstaged asset into `gd-upload/` while preserving its repo-relative path.
- The copied tree must look like `gd-upload/Content/...`, so uploading `gd-upload/` preserves paths that can be restored into the project root.
- Do not empty `gd-upload/` automatically. If the user wants a folder containing only new unstaged assets, ask them before deleting existing `gd-upload/` contents or let them empty the folder first.
- Use `gen-gd` only when the user wants the complete upload bundle refreshed.
- Full `gen-gd` generation copies every asset listed in `Docs/Operations/unstaged-assets.md` into `gd-upload/` and creates or refreshes `gd-upload/unstaged-assets-content.zip` from `gd-upload/Content`.
- `gen-gd`, `unstage`, and cleanup commands may create, update, or overwrite `gd-upload/Content/**` and `gd-upload/unstaged-assets-content.zip` only. They must not delete, empty, overwrite, or repackage `gd-upload/CodeIntent/**`.
- Full bundle zip entries must start with `Content/...`; after extracting the zip in the project root, the files must return to their original locations.
- If `gd-upload/unstaged-assets-content.zip` already exists, overwrite only that zip. Preserve other zip files in `gd-upload/`.
- If a user explicitly asks to clean, empty, rebuild, or replace all of `gd-upload/`, stop and confirm whether `gd-upload/CodeIntent/` must be preserved before running any deletion.
- If a stale zip is locked by another process, report the locked file and process instead of deleting unrelated files or killing processes.
- Prefer the project-local `gen-gd` skill to regenerate the full folder and zip from `Docs/Operations/unstaged-assets.md`.

## Project Skills

- Use `unstage <repo-relative-path>` when the user asks to unstage or ignore a specific asset. If no path argument is provided, ask the user for the filename/path before continuing. Ask whether to copy only the new asset or rebuild the full upload bundle; default to only the new asset.
- Use `gen-gd` when the user asks to rebuild the Google Drive upload folder or when an unstaged asset workflow needs the complete upload bundle regenerated.

## Documentation Rules

- Keep `Docs/Operations/unstaged-assets.md` concise Korean.
- Use exact repo-relative paths with forward slashes.
- Use `팀 공유 Google Drive` as the location when the user identifies the team shared Google Drive as the source of truth.
- Keep technical identifiers, asset names, and Unreal terms in English when clearer.

## Caution

If an asset is already tracked in `HEAD`, `git rm --cached` stages its removal from the repository in the next commit while leaving the local file intact. Mention that effect clearly before finalizing when the user's wording is ambiguous.
