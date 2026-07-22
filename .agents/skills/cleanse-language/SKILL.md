---
name: cleanse-language
description: Cleanse banned or awkward prose terms in source comments and human-readable docs while preserving code identifiers, paths, code spans, and generated output. Use when asked to replace words in comments/docs, normalize Korean project terminology, clean in-code comments, update Docs wording, or apply project term rules without renaming symbols.
---

# Cleanse Language

## Workflow

1. Read `AGENTS.md` and relevant directives before repo work:
   - Always: `.agents/directives/01-behavior.md`, `.agents/directives/03-workflow.md`, `.agents/directives/10-session-coordination.md`
   - Source comments: `.agents/directives/02-unreal-code.md`
   - Human docs: `.agents/directives/11-human-documentation-style.md`
   - Intent docs: `.agents/directives/13-code-intent-documentation.md`
2. Read `references/term-rules.md` when applying stored project terminology.
3. Confirm the requested term mapping:
   - Use the user's explicit replacement if provided.
   - If the term is not in `term-rules.md` and the replacement is ambiguous, ask before editing.
4. Search candidates with `rg` in the smallest relevant scope.
   - Default scope: `Source/**/*.h`, `Source/**/*.cpp`, `Docs/**/*.md`
   - Exclude by default: `Docs/CodeIntentSite/**`, `Saved/**`, `.agents/work/**`, generated/cache/build output
5. Classify each hit before editing:
   - Change: prose in C++ comments or human documentation paragraphs
   - Preserve: code identifiers, file paths, asset paths, `inline code`, fenced code blocks, command examples, Unreal class/function/property names
   - Ask: unclear UI/user-facing copy or a mixed prose/code sentence where replacing could alter meaning
6. Edit only the classified prose hits with `apply_patch`.
7. Re-run `rg` for the old term and report remaining hits as one of:
   - preserved identifier/code
   - generated or excluded output
   - unresolved prose that still needs a decision
8. Run `git diff --check` for touched text files.

## Editing Rules

- Preserve exact identifiers such as `bIsTainted`, `CleanseTaint`, `IA_Cleanse`, class names, categories, paths, and commands.
- Do not rename files, assets, UPROPERTY names, Blueprint API names, or C++ symbols.
- Keep Korean source comments short and literal; avoid polite endings and trailing punctuation.
- Keep docs concise Korean, while leaving technical identifiers in English.
- Do not update generated Code Intent HTML, Doxygen output, or cached site output as source of truth.

## Reporting

When done, report:

- terms changed
- files touched
- remaining old-term hits and why they remain
- validation command results
