# 11. Human Documentation Style Directive for AI Agents

This directive governs human-readable project documents written or updated by AI agents.

AI-readable directives under `.agents/directives/` must stay in English. Human-readable project documents must use concise Korean unless the user explicitly requests another language for that document.

## Applies To

Apply this style to human-readable documents such as:

- Changelogs and change summaries.
- Logic and architecture documents.
- Blueprint or Editor task documents.
- Findings, retrospectives, debugging notes, and implementation notes.
- Any other document the user explicitly asks to create or update for human reading.

## Korean Style Rules

Write human-readable documents in concise Korean:

- Prefer short headings, bullets, tables, and checklists over long prose.
- Use plain noun phrases or concise verb-noun endings where possible, such as `추가`, `정리`, `검증`, `확인`, `보강`, `제거`, `적용`.
- Avoid polite or conversational endings such as `~합니다`, `~해요`, `~했습니다`, `~됩니다`.
- Avoid verbose explanatory filler. Include only context that helps the user act, remember, or verify.
- Keep variable names, function names, class names, file paths, Unreal terms, and tool names in English when that is clearer.
- Do not translate technical identifiers into awkward Korean transliterations.
- Use Korean for explanatory text, but keep exact UI labels, asset names, paths, and code identifiers unchanged.

## Reader-First Explanation

When documenting a bug cause, fix, callback, lifecycle event, network flow, or Unreal-specific term for a human developer:

- Start with the prerequisite concept in plain words before naming the bug. Example: explain that a callback is "a function registered now so Unreal can call it later when an event finishes" before discussing a missed montage callback.
- Then describe the concrete runtime chain in numbered or arrow steps: what sets state, what is expected to call back, what fails, and what state remains stuck.
- Separate "what caused the bug" from "what did not cause the bug" when confusion is likely. Example: explicitly say whether the issue is an `AnimNotify`, `AnimNotifyState`, montage delegate, replication delay, or timer lifecycle.
- Keep the first explanation readable for a developer who did not write the system. After that, include exact functions, files, and guard rules.
- Prefer small examples or pseudo-code when they make the state transition obvious.
- Avoid blame language. Focus on the missing handshake, ownership boundary, stale callback, or lifecycle assumption.
- Do not make documentation terse if the user asks for a teaching note, handoff note, or explanation for another developer. In that case, clarity beats maximum brevity.

## Document Shape

For changelogs:

- Lead with what changed.
- Group related files or systems together.
- Keep entries short and scannable.

For logic or architecture documents:

- State the decision or rule first.
- Include the minimum example needed to understand or reproduce the logic.
- Separate verified facts from assumptions when the distinction matters.
- Prefer durable implementation reasoning over raw investigation logs.

For `Docs/KNOW_HOW.md`:

- Write subsystem-level "how it works" recipes, not file-by-file change inventories or chronological task diaries.
- Keep each recipe focused on the current truth: problem, current solution, flow, key locations, modification cautions, verification, and related docs.
- Update the relevant recipe when solving a new or existing problem, improving non-trivial logic, changing an Editor/Blueprint workflow, or confirming a durable debugging cause.
- For an existing problem or flow, revise the existing recipe in place instead of adding a new duplicate section.
- Before creating a new recipe, search existing headings and keywords for the same subsystem, actor, asset, behavior, or user-facing problem. For example, Diana map scan work belongs in the existing Diana scan/hack-related recipe when that recipe already covers the flow.
- Keep the recipe as current truth after each update. Remove or rewrite stale bullets instead of appending a historical correction trail.
- During iterative work, update the recipe when a durable cause, decision, or solution becomes clear; do not defer all updates to the final summary.
- Link to `Docs/Architecture/architecture.md` for design rationale and `Docs/Operations/blueprint-tasks.md` for exact Editor/Blueprint property steps instead of duplicating long content.

For Blueprint or Editor task documents:

- Include asset path, panel or window name, property name, expected value, and verification step when known.
- Keep task steps direct and executable.

## Exceptions

Use another language only when the user explicitly requests it for the human-readable document, or when the document is intentionally targeting an external non-Korean audience.
