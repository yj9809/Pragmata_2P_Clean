# 13. Code Intent Documentation Directive for AI Agents

This directive governs development intent capture, intent restoration, and Code Intent generated artifacts.

## Source Of Truth

- `Docs/Intent/**/*.md` is the canonical source for development intent.
- `Docs/CodeIntentSite/**` is a tracked readable snapshot for team members. It is not the canonical source.
- Doxygen, Blueprint scan output, MkDocs output, graph JSON, and local Code Intent HTML under `Saved/CodeIntent/**` are generated references. Do not edit them as source documents.
- If generated output contradicts `Docs/Intent/**/*.md`, inspect the code and update the tracked intent document or source code. Do not patch the generated output by hand.

## When Intent Documentation Is Required

Before finishing any non-trivial code, Blueprint-facing API, component, subsystem, architecture, or debugging task:

- Check whether the work creates, changes, removes, or clarifies a function, class, component, Blueprint-facing property, RPC, delegate callback, gameplay flow, or system boundary whose purpose may be unclear later.
- Update or create a focused `Docs/Intent/**/*.md` card when the changed symbol is reusable, easy to confuse with another symbol, called from multiple places, network-sensitive, Blueprint-facing, or AI-authored.
- For duplicate or near-duplicate concepts, document the distinction explicitly. Example: why one damage path exists, where it is used, and when another damage path must not be used.
- If intent documentation is not needed, state the reason in the final response.

## Intent Restoration

When asked to recover existing code intent, or when current intent is unclear:

- Inspect the smallest relevant code and docs first with `rg` or equivalent fast search.
- Use existing `Docs/Intent`, `Docs/Systems`, `Docs/Architecture`, `Docs/KNOW_HOW.md`, git log, git blame, and changelist docs as evidence.
- Use Doxygen and Blueprint scan only as supporting references for call/reference discovery.
- Mark uncertainty plainly in the intent document when evidence is incomplete.
- Prefer updating an existing intent card over creating a duplicate.

## Intent Conflict Gate

Before planning or implementing a change, compare the request with the narrowest relevant canonical intent card and supporting source comments or current-truth documentation.

- Treat an explicit statement of required behavior, forbidden behavior, ownership, authority, lifecycle, default/reset policy, or system boundary as clear intent when current code and supporting evidence agree.
- When the request clearly contradicts that intent, stop before changing plans, code, docs, assets, Git state, or build state.
- Cite the conflicting intent and ask the user to choose between explicitly overriding it or aborting the new task and preserving it.
- Do not silently treat the latest prompt as authorization to overturn documented intent.
- On explicit override, revise the canonical `Docs/Intent/**/*.md` card and any current-truth `Docs/KNOW_HOW.md` recipe in the same task so the repository records the new decision.
- On abort, leave the existing implementation and intent documents unchanged.
- If evidence is stale, contradictory, or inferred rather than explicit, report the ambiguity and continue investigation; do not present it as a clear conflict.

## Generated Output Policy

- Do not auto-generate Code Intent output unless it is directly needed for the current change and is expected to finish within 30 seconds.
- Do not run full Doxygen generation, Blueprint scan, or large HTML snapshot regeneration unless the user explicitly requests it.
- `Saved/CodeIntent/**` remains untracked scratch.
- Preserve expensive Doxygen and Blueprint scan results through `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` when the user asks to refresh or preserve them.
- `Docs/CodeIntentSite/**` may be refreshed only when the user asks for a shareable static site update, or when the change is small enough to regenerate quickly and is directly tied to the current task.

## Writing Rules

- Human-readable intent documents under `Docs/Intent/**` follow `11-human-documentation-style.md`: concise Korean, technical identifiers in English, and no generated-result prose dumps.
- Keep each intent card focused on why the symbol exists, where it is used, what should not call it, and what adjacent symbol it can be confused with.
- Code comments remain short and local under `02-unreal-code.md`. Longer reasoning belongs in `Docs/Intent/**` or the most relevant tracked `Docs/**` document.
