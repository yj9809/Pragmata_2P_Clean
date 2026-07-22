# 08. UnreelMCP Tools Directive for AI Agents

This directive governs when and how AI agents should use the `Plugins/UnreelMCP` plugin and its MCP tools.

Read [`09-unreelmcp-operation-recipes.md`](09-unreelmcp-operation-recipes.md) only after a task needs one of the recipe domains listed there, such as Widget, Blueprint graph, Animation, Behavior Tree, StateTree, Material, Niagara, audio, Python, or queued task work.

## Purpose

UnreelMCP provides live Unreal Editor access through:

- An Unreal Editor plugin that exposes a local HTTP backend.
- A Node.js stdio MCP bridge at `Plugins/UnreelMCP/Resources/mcp-bridge/index.js`.
- A public MCP surface for status, operation discovery, operation description, calls, context, and transaction policy.
- Searchable Unreal operations for editor state, assets, actors, Blueprints, widgets, animation, AI assets, input, materials, audio, FX, logs, screenshots, task results, script history, and project context.

Use UnreelMCP when live Editor state or Unreal asset data would be more accurate than filesystem inspection.

## When To Use UnreelMCP

Automatically consider UnreelMCP for tasks involving:

- Current level, actor, transform, selection, viewport, map, or output log state.
- Content Browser assets, asset metadata, dependencies, or saved package state.
- Blueprint, Blueprint graph, Animation Blueprint, Widget Blueprint, material graph, animation sequence, or montage work.
- Enhanced Input actions and mapping contexts.
- GameplayTags, DataTables, DataAssets, CurveTables, or project settings.
- Static mesh UV generation, skeletal mesh physics assets, foliage, landscape, Niagara, Behavior Tree, Blackboard, StateTree, SoundCue, SoundWave, or MetaSound assets.
- UE 5.7 or UE 5.8 API context where live Editor reflection is more useful than raw source inspection.

For plain C++ source edits, documentation-only changes, git work, or config-only changes, normal filesystem inspection is usually sufficient.

## First Check

When UnreelMCP may be useful and MCP tools are available, call `unreal_status` first.

- If connected, use the operation catalog flow below.
- If disconnected, test `http://localhost:55570/mcp/status` when shell access is available.
- If the HTTP backend is down and the user has explicitly authorized automatic Editor/tool execution for the current task, launch Unreal Editor with the UnreelMCP plugin enabled and poll for readiness before continuing.
- If the HTTP backend is down and automatic Editor launch has not been explicitly authorized, report that Unreal Editor must be open with the UnreelMCP plugin enabled.
- If the bridge process fails before connecting, follow the recovery rules below.

## Operation Catalog Flow

Use the searchable operation catalog instead of assuming a fixed large tool list.

1. Call `unreal_status`.
2. Call `unreal_context` for concise project, engine, and plugin context.
3. Call `unreal_tool_search` with a natural, specific query and optional domain.
4. Call `unreal_tool_describe` for the exact `operation_id`.
5. If the operation matches a recipe domain listed in [`09-unreelmcp-operation-recipes.md`](09-unreelmcp-operation-recipes.md), read that matching recipe before mutating.
6. Inspect parameters, examples, risk, read-only status, supported engine versions, and transaction requirements.
7. Call `unreal_call` with the selected `operation_id` and JSON `params`.
8. After mutating operations, verify with a read operation such as asset search/info, log tail, viewport screenshot, or a domain-specific inspect operation.

The catalog search supports multi-word queries. Prefer specific searches such as `save asset package`, `edit property`, `animation notify property`, or `blackboard key property` before guessing an operation id.

If the MCP client exposes only part of the bridge tool list, use raw HTTP from shell as a fallback:

- `GET http://localhost:55570/mcp/status`
- `POST http://localhost:55570/mcp/search`
- `POST http://localhost:55570/mcp/describe`
- `POST http://localhost:55570/mcp/call`
- `POST http://localhost:55570/mcp/operation` is accepted only as a deprecated compatibility alias for `/mcp/call`.

For operations that reference an existing asset, `asset` may be used as a shorter alias for `asset_path`. For create operations, use `asset_path` because it is the destination path.

Reflected property setters can return `data.property_change.before` and `data.property_change.after`. Use that response to verify edits before relying on editor display text.

If a `/Script/ProjectModule.ClassName` class, struct, or object path fails to load, build the relevant editor target and restart Unreal Editor before retrying; the current editor process may still have stale DLLs loaded.

## Public MCP Tools

- `unreal_status`: check connectivity and backend status.
- `unreal_context`: fetch concise project and plugin context.
- `unreal_tool_search`: search the operation catalog.
- `unreal_tool_describe`: inspect one operation's schema, risk, examples, and version support.
- `unreal_call`: execute one catalog operation.
- `unreal_transaction`: inspect transaction state and policy.

The bridge should advertise `unreal_tool_search`, `unreal_tool_describe`, and `unreal_transaction`. If a client-side discovery layer hides them, call the raw HTTP routes above instead of guessing operation schemas.

## Mutating Operations

Use mutating operations only when the user asked for live Editor, asset, Blueprint, level, or project changes.

### Build Authorization For Mutations

- An UnreelMCP edit/mutation request authorizes the agent to run the minimum required project Editor target build, close the current project Editor, and restart it with UnreelMCP enabled when a reflected class, UHT schema, or loaded DLL is stale.
- Do not pause for separate build approval in that case. Build, restart, retry the intended mutation, and report the result.
- This authorization is limited to the requested project mutation and its validation. Keep destructive-operation confirmation, transaction checks, save/readback verification, and unrelated-project safeguards unchanged.

- Prefer dedicated catalog operations over `python.execute`.
- Treat `python.execute` and `python.execute_script` as high risk and available only when `UNREEL_MCP_ENABLE_PYTHON=1` was set before launching Unreal Editor.
- Use `unreal_tool_describe` before mutating calls and check `risk`, `read_only`, `requires_transaction`, and `supported_engines`.
- Ask before destructive or broad operations such as deleting assets, deleting actors, moving/renaming many assets, reimporting assets, broad config rewrites, or any operation that may irreversibly alter `.uasset` or `.umap` content.
- After a mutation, verify with `asset.search`, `asset.info`, `reflection.asset_info`, `asset.save`, `log.tail`, `viewport.screenshot`, or a domain-specific read operation.
- Use [`09-unreelmcp-operation-recipes.md`](09-unreelmcp-operation-recipes.md) for domain recipes instead of guessing operation parameters from memory.

## Failure Recovery

If UnreelMCP tools are unavailable or fail to launch:

1. Verify Node.js and npm:

   ```powershell
   node --version
   npm --version
   ```

2. Verify bridge files:

   ```powershell
   Test-Path Plugins/UnreelMCP/Resources/mcp-bridge/index.js
   Test-Path Plugins/UnreelMCP/Resources/mcp-bridge/node_modules
   ```

3. If `node_modules` is missing, install and test the bridge:

   ```powershell
   cd Plugins/UnreelMCP/Resources/mcp-bridge
   npm install
   npm test
   ```

4. Verify the Unreal Editor HTTP backend:

   ```powershell
   Invoke-RestMethod http://localhost:55570/mcp/status
   ```

5. If the backend is down and the user has explicitly authorized automatic Editor/tool execution for the current task, open Unreal Editor with the UnreelMCP plugin enabled and poll `/mcp/status` until ready. Prefer a project launcher script when available; otherwise launch the project with the matching engine `UnrealEditor.exe`. If automatic launch has not been explicitly authorized, ask the user to open the Editor.

If `/Script/ProjectModule.ClassName` reflection or node creation fails after a C++ source edit, build the minimum required editor target, restart Unreal Editor, and retry. The editor may be running an older DLL until that happens.

If automatic recovery fails, report exactly which check failed and provide the next manual command or Editor action.

## Cross-Agent MCP Setup

Project-scoped MCP config files should point to:

- Command: `node`
- Bridge: `Plugins/UnreelMCP/Resources/mcp-bridge/index.js`
- Environment: `UNREEL_MCP_URL=http://localhost:55570`

`UNREAL_MCP_URL` may work as a backward-compatible fallback, but prefer `UNREEL_MCP_URL` for new setups.

Keep setup conservative:

- Do not store secrets in MCP config.
- Do not enable approval or sandbox bypass by default.
- Do not trust broad external MCP servers just because UnreelMCP is trusted.

## Unsupported Or Version-Specific Operations

If an operation reports `unsupported_in_ue57`, `unsupported_in_ue58`, `unsupported`, or `best_effort`, do not retry the same call repeatedly. Read the operation description, inspect the returned reason, and either choose a supported operation or report the limitation to the user.
