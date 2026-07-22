# 02. Unreal C++ Directive for AI Agents

This repository is not Lyra-based and does not use GAS. Lyra, GameFeature, Experience, and Gameplay Ability System assumptions must not be applied here.

## Coding Rules

- Prefer the minimum code that solves the requested problem.
- Avoid over-engineering and speculative extension points.
- Match existing code style, formatting, naming, and Unreal conventions.
- Do not casually rewrite adjacent code that is unrelated to the task.
- If changes make includes, variables, functions, or classes unused, remove the newly-unused code.
- Mention pre-existing dead code when relevant, but do not delete it unless asked.
- Use early returns for error handling when it keeps the flow clearer.
- Use log messages that identify the source location, such as `[ClassName::FunctionName] Message`.

## Header and Include Rules

- When a type is only used as a pointer or reference in a header, prefer a forward declaration where valid.
- If a type is used by value, inherited from, embedded as a member requiring size, or needed by Unreal reflection macros in a way that requires a full definition, include the proper header.
- When moving a type from `.cpp`-only usage into a `.h` signature, add the necessary forward declaration or include in the header at the same time.
- Unreal OnlineSubsystem session value types such as `FOnlineSessionSettings`, `FOnlineSessionSearch`, and `FOnlineSessionSearchResult` require the full definition header, not only `Interfaces/OnlineSessionInterface.h`. Include `OnlineSessionSettings.h` in the `.cpp` before constructing or dereferencing those types.

## Build Error Prevention

- In `AActor`-derived classes, do not name local variables, parameters, or helper identifiers `Role`; it shadows Unreal's inherited `AActor::Role` member and can fail under shadow warnings-as-errors. Use names such as `PlayerRole`, `RequestedRole`, or `AssignedRole`.
- Treat UnrealBuildTool shadow diagnostics as real compile blockers. If adding gameplay role logic, scan changed `AActor` subclasses for identifiers that collide with inherited Unreal members before building.
- When a compile error reports "undefined type" for an Unreal forward-declared struct or class, fix the missing definition include instead of changing the call site around the incomplete type.
- After adding or changing listen-server, OnlineSubsystem, replication, or role-spawn code, run the project Editor target build when the user has approved a build, because UHT and Unreal's warnings-as-errors catch issues normal C++ inspection can miss.

## Unreal UI and Blueprint Boundaries

- Do not hardcode widget layout values such as size, padding, or alignment in C++ unless the user explicitly requests code-driven layout.
- For gameplay data shown in HUD or Widget Blueprints, preserve the project's MVVM boundary: expose replicated/runtime state from gameplay classes, sync it through a `UMVVMViewModelBase` ViewModel with FieldNotify properties, and let widgets bind to the ViewModel instead of polling gameplay actors directly.
- When adding UI-facing gameplay state, add or update the ViewModel binding path in the same task unless the user explicitly asks for a lower-level API only.
- Prefer Blueprint/UMG for visual layout and C++ for widget creation, state, and data binding.
- If a Blueprint asset must be assigned, created, or configured in the Editor, provide clear manual instructions.
- Keep complex Editor, Blueprint, or DataAsset setup out of runtime C++ unless the user explicitly asks for code-driven tooling.
- Use C++ primarily for schema, runtime behavior, validation, replication, and data binding; use Editor, Blueprint, or tooling workflows for asset configuration.

## Unreal Asset Loading Rules

- Do not use `ConstructorHelpers::FClassFinder` or `ConstructorHelpers::FObjectFinder` in constructors to load Blueprint, AI, StateTree, BehaviorTree, Animation Blueprint, DataAsset, or other asset-chain-heavy references unless the user explicitly asks for a hard startup dependency.
- Avoid hard-loading gameplay Blueprint assets from CDO construction paths. These loads can happen during editor startup, cook, class discovery, or module initialization before editor compilers and asset schemas are ready.
- Prefer `TSoftObjectPtr<>`, `TSoftClassPtr<>`, `FSoftObjectPath`, or editor-assigned `UPROPERTY` references for gameplay assets that can be resolved later.
- Resolve soft references only at the point of use, such as `BeginPlay`, explicit initialization, spawning, or a user-triggered action. Handle failed loads with a clear log message.
- Before adding or changing a runtime soft reference, string `/Game/...` path load, or Blueprint-facing asset property that must exist in packaged builds, identify the cook owner in the same task: map assets go in `MapsToCook`; non-map runtime soft-load assets go in the appropriate `/Game/CookLabels` `PrimaryAssetLabel`; hard-referenced assets must still be checked when the reference path is indirect or constructor-loaded.
- For this project, runtime Enhanced Input assets are intentionally conservative: add both new `IMC_*` and new runtime `IA_*` assets to `PAL_CoreRuntime` unless the task explicitly replaces the current PAL policy and validates packaging coverage another way.
- When moving or renaming a runtime asset referenced from C++, update the C++ path/property, update the relevant PAL explicit asset entry, and verify that no stale `/Game/...` path remains.
- Be especially careful with StateTree-linked assets. Loading a Blueprint that indirectly pulls in AIController or StateTree assets too early can make linked StateTrees appear to have an invalid or missing schema even when the assets use the same schema and compile normally when opened manually.
- Before introducing a hard reference to `/Game/...` from C++, trace the dependency chain and confirm it will not force-load unrelated Blueprint, AI, UI, animation, or data assets during editor startup.

## Name and Asset Verification

- Verify actor names, component names, Blueprint class names, Content Browser paths, property names, socket names, input action names, and `BindWidget` names before wiring C++ to them.
- Treat user-provided `/Game/...` paths and Blueprint member names as hints until confirmed through local source, live Editor state, or asset inspection.
- For package-relevant runtime assets, verify PAL or `MapsToCook` coverage before reporting completion. Do not assume PIE success means the asset will be cooked.
- If a required asset, widget, or property cannot be verified, ask for the missing context or provide explicit Editor steps instead of guessing.

## Code Intent Documentation Link

- Before finishing non-trivial Unreal C++, Blueprint-facing API, component, subsystem, RPC, delegate, UI binding, or gameplay flow changes, apply `13-code-intent-documentation.md`.
- Use code comments for local "why this logic exists" notes only. Long intent, usage boundaries, duplicate-symbol distinctions, or historical reasoning belong in `Docs/Intent/**/*.md` or the most relevant tracked `Docs/**` document.
- When a changed symbol can be confused with another symbol, is AI-authored, or is reused from multiple places, update or create the matching `Docs/Intent/**/*.md` card before completion.

## Networking and Listen-Server Rules

- Gameplay decisions are server-authoritative by default.
- Client RPCs should request player intent; the server validates before changing gameplay state.
- Before adding an RPC, identify the owning actor, call direction, authority requirement, and validation rule.
- Keep client-only UI, prediction, camera, and feedback separate from authoritative gameplay state.
- Design replicated state with ownership, visibility, `GetLifetimeReplicatedProps`, and optional `ReplicatedUsing` behavior in mind.
- Do not rely on client-owned values for combat, hacking, interaction, inventory, score, progression, or character selection without server validation.
- For Hugh and Diana multiplayer flows, identify what Diana's client may request, what Hugh's listen server validates, and what state must replicate to both players.
- When replacing world-wide searches with registries or caches, explicitly classify the data as server-authoritative state, replicated state, or per-world local cache before adding `HasAuthority()` gates.
- Do not make focus, preview, marker, reticle, or other client-local candidate caches server-only when client UX needs to find local actors. Keep the local cache populated on each world and revalidate the final gameplay action on the server RPC.
- In listen-server features where either player can host, verify both host layouts: Hugh server / Diana client and Diana server / Hugh client. A fix that works only because the host owns the relevant local cache is incomplete.

## Comments

This section applies whenever code is added or materially modified, even if the user did not explicitly ask for comments.

When adding or modifying comments in code:

- Use Korean comments unless the surrounding codebase clearly uses English comments.
- Keep comments minimal and literal about variable/function names.
- Prefer single-line `//` comments.
- Avoid multi-line comment blocks for ordinary explanations.
- Do not add comments that merely say code was moved, deprecated, or removed; remove obsolete code instead.
- Avoid polite or long sentence endings in Korean comments.
- Do not end Korean comments with punctuation.

### Comment Enforcement

- Review every diff hunk that adds or materially changes a function, RPC, delegate callback, lifecycle override, replicated-state update, input handler, session flow, spawn/possession path, movement prediction path, or UI state transition.
- Add or update a Korean `//` intent comment when the changed logic is non-trivial, protects gameplay/network state, coordinates client/server behavior, binds UI to state, or exists for an Unreal lifecycle reason.
- Do not use file-level comment density as the success metric. The standard is function-by-function intent coverage for changed non-trivial behavior.
- If a function is exempt, it must be plainly obvious under the exemption list below. When unsure, add one short intent comment.
- Before finalizing a code task, state whether the comment gate was applied and whether any changed functions were intentionally exempt.

### Comment Preservation

- Treat existing comments as user-authored context.
- Preserve existing comments unless they are wrong after the code change.
- If code moves, move the relevant comment with the logic.
- If a comment becomes obsolete because logic changed, update it narrowly instead of deleting it.
- Do not rewrite existing comments only to match style.

### Function Comment Coverage

- Every new or materially modified non-trivial function must have a Korean `//` intent comment.
- The comment should explain why the function exists or what gameplay/network state it protects.
- Do not merely restate the function name.
- Exempt only blatantly obvious functions: empty stubs, pure one-line getters/setters, `Super`-only overrides, simple constructors that only create default subobjects, and direct delegate forwarding with no logic.
- For Unreal lifecycle, RPC, `OnRep`, replication, input, movement prediction, spawn, possession, session, and UI state functions, comments are required by default when behavior affects gameplay state, network state, user flow, or local prediction.

### Comment Quality

- Prefer 1-2 short `//` lines before the function or before the non-obvious block.
- Use Korean unless preserving existing English comments.
- Keep Unreal terms such as `RPC`, `OnRep`, `GameState`, `PlayerState`, `listen server`, `prediction`, and `simulated proxy` in English when clearer.
- Avoid sentence endings like `합니다`, long prose, TODO-style historical notes, or comments that say code was moved.

Good examples:

```cpp
// 입력 방향 계산
// 상호작용 대상 검증
// 협동 액션 시작
```

Bad examples:

```cpp
// 입력 방향을 계산합니다.
// TODO: deprecated, logic moved elsewhere.
/* 협동 액션을 처리하는 복잡한 로직입니다. */
```

## Delegate Callback Verification

When writing callback functions for Unreal delegates (`AddDynamic`, `AddUObject`, `BindDynamic`, etc.), never guess the parameter order.

Verify by priority:

1. Search for an existing callback bound to the same delegate in this project.
2. Read the delegate declaration macro, such as `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*`, to confirm the canonical parameter list.
3. If project usage is unavailable and the delegate is engine-defined, inspect the Unreal Engine source or official API documentation.

Wrong delegate parameter order can compile but produce incorrect runtime behavior.

## Source Verification Rules

- For project-specific behavior, inspect this repository first.
- For Unreal Engine API behavior that is uncertain, prefer checking the local Unreal Engine 5.8 source matching `Pragmata_2P.uproject` when available.
- If the engine source path is unavailable, use official Unreal Engine documentation or clearly state uncertainty.
- Do not invent unavailable classes, functions, or modules. If a symbol cannot be found locally or in official references, ask for the missing path or context before implementing against it.

## Build Policy

- Do not run full Unreal builds autonomously for ordinary source-only investigation.
- An UnreelMCP mutating job is standing user authorization to run the minimum relevant Editor target build and restart the project Editor when compilation, reflection, or stale DLL state blocks that edit or its validation. Run it without asking again.
- This standing authorization covers only the current project and the minimum build/restart needed for the requested UnreelMCP asset, Blueprint, or Editor mutation. It does not authorize destructive asset operations, Git operations, or unrelated builds.
- Lightweight read-only inspection and non-build validation are allowed.
- Report the build command and result after the work; do not stop to request permission first.

## Naming Guidance

- Use existing Unreal naming conventions and the current module name, `Pragmata_2P`.
- Do not introduce `DS`, `Lyra`, or GAS-style prefixes just because a reference document used them.
- Do not use artificial class prefix names for C++ classes, Blueprint classes, assets, or generated types. For example, do not create `DVCharacter` when the correct local name is `Character`.
- If a character, actor, component, or system has a clear project name, include that name directly, such as `HughCharacter` or `DianaOperatorPawn`.
- Prefer names that communicate the local concept directly, such as `Hugh`, `Diana`, `Hack`, `Coop`, `Puzzle`, `Combat`, `Partner`, or `Pragmata`, when they fit the implementation.
