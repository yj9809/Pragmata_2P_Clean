# 06. Security and Safety Directive for AI Agents

## Trust Boundaries

- Do not commit secrets, API keys, tokens, passwords, credentials, machine-local paths used as secrets, or private runtime state.
- Do not expose private environment values in documentation or commit messages.
- Do not edit projects outside this repository unless the user explicitly asks.
- The gold standard reference project is read-only by default.

## Repository File Safety

- Track project-authored files such as `Pragmata_2P.uproject`, `Config/`, `Content/`, `Source/`, `Docs/`, `.agents/directives/`, `README.md`, and `AGENTS.md`.
- Never create new `.md` files at the repository root except the allowed root docs: `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`, and `README.md`.
- Use `.gitkeep` only for intentionally preserved empty directories, especially empty leaf folders under `Content/`.
- Treat `.agents/work/` as scratch state unless another directive explicitly says to move durable conclusions into tracked docs.

## Generated and Local State

Never intentionally commit generated Unreal, IDE, cache, or local runtime output:

- `Binaries/`
- `Intermediate/`
- `Saved/`
- `DerivedDataCache/`
- `.vs/`
- `.idea/`
- `.omc/`
- `.omx/`
- `.agents/work/`
- `.agents/prompt-assets/`
- `Saved/CodeIntent/`

Before staging or committing, check `git status --short` and verify that generated files are not included.

Code Intent artifact rules:

- `Docs/Intent/**/*.md` is tracked source documentation. `Docs/CodeIntentSite/**` is a tracked readable snapshot, not source documentation.
- Keep `Saved/CodeIntent/**` untracked scratch.
- Do not commit Doxygen output, Blueprint scan output, graph JSON, compile databases, installers, package caches, `node_modules`, or `.venv`.
- Preserve expensive Doxygen and Blueprint scan output only through `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` when the user asks to refresh or preserve it.
- Keep `gd-upload/CodeIntent/**` ignored by Git.

Generated output execution rules:

- Do not auto-generate output when the command is expected to take more than 30 seconds.
- Do not run full Doxygen generation, Blueprint scan, or large HTML snapshot regeneration unless the user explicitly asks.
- Fast generated output is allowed only when it is directly required by the current change and expected to finish within 30 seconds.
- If generated output is skipped because it is expensive, report the exact command or artifact that remains manual.

## External References

- Treat `Wanted4_ArenaBattleNetwork` as a reference, not as editable project state.
- Do not copy private or machine-specific configuration from reference projects.
- Do not import plugins, dependencies, or architecture from a reference project without confirming they fit `Pragmata_2P`.
- If a reference path is machine-local, prefer documenting the resolution rule rather than relying on the absolute path.

## Destructive Operation Safety

- Never run destructive git commands such as `git reset --hard`, `git checkout -- <path>`, branch deletion, or plain force-push unless explicitly requested.
- Do not delete user changes just because they are unrelated.
- If unrelated local changes exist, leave them untouched and mention them in the final status when relevant.
- If a branch rewrite is explicitly requested, use `--force-with-lease`, never plain `--force`.

## Unreal Content Safety

- Treat `.umap` and `.uasset` files as binary assets that may contain user Editor work.
- Do not modify binary Content assets unless the user explicitly asks for that asset-level work.
- If binary asset changes are already present and unrelated to the task, do not revert or restage them.
