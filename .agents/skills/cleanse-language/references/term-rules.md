# Term Rules

Apply these rules only to prose in source comments and human-readable docs. Preserve code identifiers, paths, commands, code spans, fenced code blocks, asset names, and Unreal API names.

## Default Replacements

| Prose term | Replacement | Notes |
| --- | --- | --- |
| `taint` as a noun | `오염` | Keep identifiers such as `bIsTainted`, `CleanseTaint`, `OnRep_Tainted` |
| `tainted` as an adjective | `오염된` | Use for enemy/material/visual descriptions |
| English cleanse-skill prose | `정화 스킬` | Keep identifiers such as `IA_Cleanse`, `UDianaCleanseComponent`, `RequestCleanse` |

## Preservation Examples

Change:

```text
tainted enemy visual
```

To:

```text
오염된 enemy visual
```

Preserve:

```text
`bIsTainted`
`CleanseTaint()`
/Game/Blueprints/Player/Input/Diana/IA_Cleanse
```

## Search Guidance

Prefer focused searches:

```powershell
rg -n "taint|tainted|정화 스킬" Source Docs -S
```

Then exclude generated or non-source locations:

```powershell
rg -n "taint|tainted|정화 스킬" Source Docs -S -g "!Docs/CodeIntentSite/**"
```
