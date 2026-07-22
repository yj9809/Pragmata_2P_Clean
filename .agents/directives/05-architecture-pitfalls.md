# 05. Architecture and Pitfalls Directive for AI Agents

## Project Profile

- Project: `Pragmata_2P`
- Engine: Unreal Engine 5.8
- Type: plain Unreal Engine C++ project
- Goal: learning-purpose Pragmata clone project extended with original 2-player gameplay by separating Hugh and Diana into distinct playable roles
- Main project file: `Pragmata_2P.uproject`
- Main source module: `Source/Pragmata_2P/`

## Ownership Boundary

- This directive owns Unreal architecture, implementation pitfalls, listen-server reasoning, and manual Editor/Blueprint guidance.
- Repository file hygiene, generated/local state, root document restrictions, and binary asset safety are owned by [`06-security-safety.md`](06-security-safety.md).
- Documentation timing and durable finding location are owned by [`03-workflow.md`](03-workflow.md); human-facing document style is owned by [`11-human-documentation-style.md`](11-human-documentation-style.md).

## Known Unreal Pitfalls

- This project is not Lyra-based. Do not import Lyra, GAS, GameFeature, Experience, CommonUI, or UIExtension assumptions unless the project explicitly adds them later.
- Hard-loading Blueprint, AI, animation, StateTree, or data assets from constructors can create editor startup and schema validation failures.
- CDO-time asset loads can occur during cook, editor startup, module initialization, and class discovery.
- PIE can load assets that are missing from packaged builds. Treat runtime soft-load and string path assets as uncooked until `MapsToCook` or `/Game/CookLabels` coverage is verified.
- StateTree-linked Blueprint dependency chains are especially sensitive to early hard loads.
- Delegate callback parameter order must be verified from project usage, delegate declarations, or engine source.
- Blueprint and UMG layout work should usually remain in the Editor, with C++ limited to creation, state, and data binding unless the user asks for code-driven layout.
- HUD and Widget Blueprint data should flow through the project's MVVM ViewModels. Gameplay actors/components own authority and replication, ViewModels adapt that state with FieldNotify, and widgets should bind to ViewModels rather than reaching into gameplay objects directly.
- Full Unreal builds are expensive and should not run autonomously unless the user explicitly asks.

## Cook and PrimaryAssetLabel Guardrails

- Runtime map transitions must be covered by `ProjectPackagingSettings.MapsToCook`, not by `PrimaryAssetLabel`.
- Runtime non-map assets that are reached through `TSoftObjectPtr`, `TSoftClassPtr`, `FSoftObjectPath`, string `/Game/...` loads, or indirect debug/runtime fallback paths must have an explicit cook owner.
- The current cook owner for runtime non-map soft-load assets is `/Game/CookLabels`:
  - `PAL_CoreRuntime`: GameFlow, GameMode, UI, Enhanced Input, lobby ring material.
  - `PAL_LobbyPreview`: lobby preview mesh and idle animation assets.
  - `PAL_EnemyRuntime`: runtime enemy, hacked mesh, AI, and damage text assets.
- Do not solve package misses by adding broad `/Game/Character` or `/Game/Enemy` directory cook rules. Use explicit PAL entries for the runtime assets that are actually needed.
- When a task adds, moves, renames, or soft-loads a runtime asset, validate whether a PAL explicit asset entry or `MapsToCook` entry is needed before completion.
- For this project, new runtime Enhanced Input assets should be listed explicitly in `PAL_CoreRuntime`: add both the `IMC_*` asset and each runtime `IA_*` asset, even when the action is also referenced from an already-listed mapping context.
- When using Editor automation or UnreelMCP for PAL work, read back the saved label asset after mutation and verify `Cook Rule = Always Cook`, `Is Runtime Label = true`, and the expected explicit asset list.
- If PAL validation cannot be automated, report the exact manual Editor verification steps and do not claim packaged coverage as verified.

## Listen-Server Reasoning

Before implementing multiplayer behavior:

- Identify which actor or subsystem has authority for the decision.
- Separate server responsibilities from client presentation.
- Define the replication path for any state that clients must see.
- Define RPC direction and validation rules before adding RPC functions.
- Prefer server-side validation for gameplay decisions, especially character selection, combat, hacking, interaction, inventory, score, and progression.
- Treat clients as intent senders and replicated-state viewers unless a local-only prediction or UI concern is explicitly identified.
- For Hugh and Diana flows, state what Diana can request, what Hugh's listen server validates, and what both players must observe.
- Treat Hugh listen server and Diana client assumptions as design-sensitive. Verify them against local code and live planning notes before implementation.

## Manual Editor Work

When a requested change requires the Unreal Editor or Blueprint:

- State the required manual steps clearly.
- Include asset names, Content Browser paths, panels, property names, exact expected values, and verification steps when known.
- Do not pretend C++ can safely replace an Editor-only assignment.
- Use `Docs/Operations/blueprint-tasks.md` only when a durable tracked task list is useful.
- Otherwise, write concise manual steps in the response.
