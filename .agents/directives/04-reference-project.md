# 04. Reference Project Directive for AI Agents

This directive governs how AI agents use external project references for best practices, implementation examples, and architectural comparison.

## Primary Reference

- Gold standard reference project: `Wanted4_ArenaBattleNetwork`
- Current known local path: `D:/UnrealProjects/RonnieJ/Wanted4_ArenaBattleNetwork`
- Treat this project as the first project-specific reference for Unreal Engine C++ patterns, repository organization, and implementation examples when this repository lacks an established local pattern.

## Path Resolution

Because developer machines may use different root folders, do not rely on a single hardcoded path.

Resolve the reference project in this order:

1. Environment variable `PRAGMATA_GOLD_REFERENCE`, if set.
2. Environment variable `WANTED4_ARENA_REFERENCE`, if set.
3. Sibling path from this repository: `../Wanted4_ArenaBattleNetwork`.
4. Known local workspace path: `../RonnieJ/Wanted4_ArenaBattleNetwork`.
5. Current known absolute path: `D:/UnrealProjects/RonnieJ/Wanted4_ArenaBattleNetwork`.

Use the first path that exists and contains the `Wanted4_ArenaBattleNetwork` project. When reporting or documenting the resolved path, prefer the actual discovered path instead of assuming the current known local path.

## When To Consult It

Consult `Wanted4_ArenaBattleNetwork` when:

- Looking for best-practice examples before adding or changing Unreal C++ systems.
- Choosing between multiple reasonable implementation styles.
- This repository has no existing equivalent pattern for the requested feature.
- The user asks for a reference, precedent, best practice, or comparison.
- Documentation, module organization, naming, build setup, input setup, asset reference style, or gameplay system structure is uncertain.

## Priority Order

Use references in this order:

1. Existing `Pragmata_2P` code, config, docs, and user instructions.
2. `Wanted4_ArenaBattleNetwork` as the gold standard project-specific reference.
3. Local Unreal Engine 5.8 source matching `Pragmata_2P.uproject` when API behavior is uncertain.
4. Official Unreal Engine documentation when local source is unavailable or insufficient.

Do not copy patterns from the reference project blindly. Adapt only the parts that fit this repository's plain Unreal C++ project profile and current learning-oriented scope.

## Usage Rules

- Resolve the reference project path using the Path Resolution section before reading files.
- Prefer reading the smallest relevant file set in the resolved reference project before broad searches.
- Clearly distinguish local project facts from reference-project examples.
- Do not edit files under the resolved reference project unless the user explicitly asks.
- Do not import unrelated architecture, plugins, dependencies, or naming conventions only because they exist in the reference project.
- If the reference project cannot be found through any configured path, state that clearly and continue with local project evidence plus Unreal Engine references.

## Documentation Expectations

When a decision is materially based on `Wanted4_ArenaBattleNetwork`, mention the resolved reference file or pattern used in the response or relevant project documentation.
