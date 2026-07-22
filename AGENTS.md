# AGENTS.md

This is the canonical agent manifest for this project. Claude Code, OpenAI Codex, and Google Gemini all read from here.
Directives, project-local skills, prompts, and shared agent resources live under `.agents/`. Tool-specific config such as `.claude/`, `.codex/`, or `.gemini/` should only hold tool-local settings.

## Project Context

- Project: `Pragmata_2P`
- Engine: Unreal Engine 5.8
- Type: plain Unreal Engine C++ project
- Purpose: learning-oriented Pragmata clone project extended with original 2-player gameplay by separating Hugh and Diana into distinct playable roles

## User Directives

Direct edits go here. AI agents must read and follow the directive documents before acting.

### Prime Directives

These root rules are intentionally short and non-optional. Load the numbered directive files for the full rules.

- Treat this file as the always-loaded contract, not as a substitute for the numbered directives.
- Before changing Unreal C++, Blueprint-facing code, `.Build.cs`, runtime config, or any `Source/` file, read [`.agents/directives/02-unreal-code.md`](.agents/directives/02-unreal-code.md) and apply its comment, include, Blueprint boundary, asset loading, networking, and build rules.
- Before non-trivial implementation, debugging, architecture, Blueprint-facing API, or code-intent work, read [`.agents/directives/13-code-intent-documentation.md`](.agents/directives/13-code-intent-documentation.md) and decide whether `Docs/Intent/**/*.md` must be updated.
- Before changing Blueprint assets, Widget Blueprints, `.uasset` behavior, or Editor-authored layout/visibility behavior, read [`.agents/directives/05-architecture-pitfalls.md`](.agents/directives/05-architecture-pitfalls.md); also read [`.agents/directives/08-unreelmcp-tools.md`](.agents/directives/08-unreelmcp-tools.md) whenever the task uses Unreal Editor tooling, UnreelMCP, MCP, command-driven Editor automation, or any non-manual Editor operation.
- Before adding, moving, renaming, or soft-loading runtime assets, check packaging ownership: maps belong in `MapsToCook`, runtime non-map soft loads belong in `/Game/CookLabels` `PrimaryAssetLabel` assets, and every package/build change must validate PAL coverage before completion.
- Keep every code change surgical. Touch only files and symbols needed for the requested task, and do not rewrite adjacent code, comments, formatting, assets, or docs opportunistically.
- For every non-trivial implementation, debugging, or architecture task, finish with an explicit check of comment coverage, durable documentation need, validation performed, and unnecessary files left untouched.
- When work continues or revisits an existing problem, update the matching `Docs/KNOW_HOW.md` recipe in place before creating a new section.
- When directive loading is ambiguous, load the narrowest additional directive that matches the files or behavior being changed instead of loading the whole directive set.
- If a requested change clearly conflicts with documented development intent, stop before planning or editing, cite the conflicting intent, and ask the user to choose whether to override that intent or abort the new task and preserve it.
- When an Unreal Editor session is running with PythonScriptPlugin/UnreelMCP Python enabled, automated build-time editor shutdown can crash inside `python311`/`UnrealEditor_PythonScriptPlugin`; verify the crash context before attributing it to project C++.
- Avoid adding Widget Blueprint children with raw Python `new_object` + `add_child` unless the WidgetBlueprint variable GUID map is regenerated/validated; otherwise UMG compile can emit `Widget [...] was added but did not get a GUID` ensure reports. Prefer editor-authored widgets or verify by reopening and compiling the WBP.
- UE 5.8 marks `RawInput` deprecated; for Windows DualSense/gamepad backend work prefer `GameInputWindows` and use local `PlayerController` state for UI prompt switching.
- When editing Enhanced Input `InputMappingContext` assets through UE Python commandlets, verify the saved asset after the run; in this project `map_key()` alone may not dirty/save the IMC, while explicitly updating the `mappings` array and resaving did persist the mapping.
- UE 5.8 Level Sequence natural completion can emit `OnStop` immediately before `OnFinished`; completion-sensitive code must compare current time with duration before treating `OnStop` as an interruption.
- Montage slots that must play concurrently, such as Diana body and face layers, must belong to separate Skeleton SlotGroups or the later montage can interrupt the earlier one.
- Before running `gen-gd`, verify that every backticked `.uasset` token in `Docs/Operations/unstaged-assets.md` is a concrete existing path. The current parser also reads wildcard examples and stops before zip creation when any parsed source is missing.
- Diana node interaction props selected by camera ray must expose a runtime `QueryOnly`, `Visibility=Block` surface. A Blueprint-saved `NoCollision` value makes the interaction fail before the prop handler and can therefore produce no acquisition log.
- Cinematic presentation teleports must validate the character collision shape and camera sight line, then restore the authoritative pre-cinematic transform on both success and abort. A camera-only Level Sequence does not need a character possessable merely to render the world actor.
- Walker weapon visibility follows each peer's actual `AttackPatterns` montage lifecycle through `OnMontageStarted` and `OnMontageEnded`. Do not tie it to the shorter `AttackHitWindow` or replicated `bIsAttacking`, whose arrival can lag multicast playback.

Before any repository action, including read-only investigation that may lead to edits, every AI session must read these entrypoint directives in full:

1. [`.agents/directives/01-behavior.md`](.agents/directives/01-behavior.md)
2. [`.agents/directives/03-workflow.md`](.agents/directives/03-workflow.md)
3. [`.agents/directives/10-session-coordination.md`](.agents/directives/10-session-coordination.md)

Before editing, compiling, running Unreal tools, changing Git state, or touching assets, also read every directive below that applies to the task. Do not rely only on this `AGENTS.md` summary when a directive file exists.

### Directive Loading Matrix

Load directives by task surface, even when the user does not explicitly name the directive.

| Task surface | Must read |
| --- | --- |
| Every session or repository action | `01-behavior.md`, `03-workflow.md`, `10-session-coordination.md` |
| Non-trivial implementation, debugging, architecture, Blueprint-facing API, or code-intent work | `13-code-intent-documentation.md`; add `02-unreal-code.md`, `05-architecture-pitfalls.md`, or `11-human-documentation-style.md` by touched surface |
| `Source/**/*.cpp`, `Source/**/*.h`, `.Build.cs`, Unreal runtime/config, Blueprint-facing C++ | `02-unreal-code.md`, `13-code-intent-documentation.md`; add `05-architecture-pitfalls.md` when architecture, Editor, Blueprint, or networking pitfalls are relevant |
| Korean source comments or Unreal C++ intent comments | `02-unreal-code.md`; add `11-human-documentation-style.md` only when writing user-facing documentation prose |
| `Content/**/*.uasset`, Blueprint assets, Widget Blueprints, Editor-authored layout/visibility/asset behavior | `05-architecture-pitfalls.md`; add `13-code-intent-documentation.md` when the work changes reusable Blueprint-facing behavior or restores intent; add `08-unreelmcp-tools.md` when using Unreal Editor tooling, UnreelMCP, MCP, command-driven Editor automation, or any non-manual Editor operation; add `09-unreelmcp-operation-recipes.md` only for live Editor operations covered by a recipe in `09`; add `02-unreal-code.md` when C++ or Blueprint-facing C++ boundaries change |
| Runtime asset references, soft loads, packaging cook lists, `Content/CookLabels`, `PrimaryAssetLabel`, `MapsToCook` | `02-unreal-code.md` when code references assets; `05-architecture-pitfalls.md`; add `08-unreelmcp-tools.md` when inspecting or mutating saved Unreal asset data |
| Human-readable docs, README changes, changelogs, findings, retrospectives, Blueprint task notes | `11-human-documentation-style.md`; add `13-code-intent-documentation.md` when writing or reorganizing intent docs |
| Durable findings, architecture notes, multiplayer/listen-server reasoning | `03-workflow.md`, `11-human-documentation-style.md`, `13-code-intent-documentation.md`; add `05-architecture-pitfalls.md` when Unreal architecture is involved |
| Asset unstaging, ignore rules, Git index removal, Google Drive upload bundle | `12-unstaged-assets.md`, `06-security-safety.md` |
| Security, secrets, destructive operations, external references, MCP/UnreelMCP, Notion | `06-security-safety.md`, `04-reference-project.md`, `08-unreelmcp-tools.md`, or `07-notion-knowledge.md` as applicable |

- [`.agents/directives/01-behavior.md`](.agents/directives/01-behavior.md) - persona, Korean response rule, token-efficient investigation, planning and validation behavior
- [`.agents/directives/02-unreal-code.md`](.agents/directives/02-unreal-code.md) - Unreal C++ coding rules, comments, includes, Blueprint boundaries, asset loading, build policy
- [`.agents/directives/03-workflow.md`](.agents/directives/03-workflow.md) - branch strategy, commit rules, staging, push safety, documentation workflow
- [`.agents/directives/04-reference-project.md`](.agents/directives/04-reference-project.md) - gold standard `Wanted4_ArenaBattleNetwork` reference policy and path resolution
- [`.agents/directives/05-architecture-pitfalls.md`](.agents/directives/05-architecture-pitfalls.md) - project structure, file locations, generated folders, Unreal pitfalls, Editor task handling
- [`.agents/directives/06-security-safety.md`](.agents/directives/06-security-safety.md) - trust boundaries, generated file safety, reference-project safety, destructive-operation guardrails
- [`.agents/directives/07-notion-knowledge.md`](.agents/directives/07-notion-knowledge.md) - live Notion project documentation policy and `기획 정리` source
- [`.agents/directives/08-unreelmcp-tools.md`](.agents/directives/08-unreelmcp-tools.md) - UnreelMCP tool usage, recovery, and cross-agent setup
- [`.agents/directives/09-unreelmcp-operation-recipes.md`](.agents/directives/09-unreelmcp-operation-recipes.md) - domain-specific UnreelMCP operation recipes
- [`.agents/directives/10-session-coordination.md`](.agents/directives/10-session-coordination.md) - multi-session edit coordination using gitignored active-session manifests
- [`.agents/directives/11-human-documentation-style.md`](.agents/directives/11-human-documentation-style.md) - concise Korean style rules for human-readable project documents
- [`.agents/directives/12-unstaged-assets.md`](.agents/directives/12-unstaged-assets.md) - AI flow for unstaging and ignoring externally stored large assets
- [`.agents/directives/13-code-intent-documentation.md`](.agents/directives/13-code-intent-documentation.md) - `Docs/Intent` source-of-truth rules, intent restoration, and Code Intent generated-output policy

User-authored directives under `.agents/directives/` take precedence over tool-local config on conflict.

## Project-Local Skills

Reusable project workflows live under `.agents/skills/`:

- [`.agents/skills/build-fix/SKILL.md`](.agents/skills/build-fix/SKILL.md) - [언리얼 에디터 꺼짐] Unreal Editor target build 실행과 compile/UHT/link 오류 최소 수정
- [`.agents/skills/cleanse-language/SKILL.md`](.agents/skills/cleanse-language/SKILL.md) - code identifier를 보존하며 source comment와 human docs의 금지/어색한 표현을 프로젝트 용어로 정리
- [`.agents/skills/code-intent-check/SKILL.md`](.agents/skills/code-intent-check/SKILL.md) - code intent 계획, 누락 조건, worktree 청결 검증
- [`.agents/skills/code-intent-restore/SKILL.md`](.agents/skills/code-intent-restore/SKILL.md) - git/docs/code 사용처 기반 symbol 개발 의도 복원
- [`.agents/skills/code-intent-write-card/SKILL.md`](.agents/skills/code-intent-write-card/SKILL.md) - `Docs/Intent/` 의도 카드 생성과 갱신
- [`.agents/skills/code-intent-build-site/SKILL.md`](.agents/skills/code-intent-build-site/SKILL.md) - local code intent site와 graph output 생성
- [`.agents/skills/code-intent-organize-docs/SKILL.md`](.agents/skills/code-intent-organize-docs/SKILL.md) - `Docs/` inventory, 분류, 이동, link 검증
- [`.agents/skills/code-intent-scan-blueprints/SKILL.md`](.agents/skills/code-intent-scan-blueprints/SKILL.md) - UnreelMCP 기반 Blueprint asset 연결 scan
- [`.agents/skills/create-changelist-doc/SKILL.md`](.agents/skills/create-changelist-doc/SKILL.md) - `Docs/Create/`에 작업별 파일 변경 전후와 검증 결과를 남기는 changelist 문서 생성
- [`.agents/skills/gen-gd/SKILL.md`](.agents/skills/gen-gd/SKILL.md) - `Docs/Operations/unstaged-assets.md` 기준 `gd-upload/` 번들 생성
- [`.agents/skills/land/SKILL.md`](.agents/skills/land/SKILL.md) - [주의: PR 즉시 병합] build-fix 검증부터 ship, sync, ready PR merge와 branch 재정렬까지 수행
- [`.agents/skills/land-quick/SKILL.md`](.agents/skills/land-quick/SKILL.md) - [주의: build-fix 생략] 사용자 판단으로 build gate를 건너뛰고 빠르게 PR merge
- [`.agents/skills/sync/SKILL.md`](.agents/skills/sync/SKILL.md) - stash 보호로 현재 브랜치와 `develop` 동기화
- [`.agents/skills/unstage/SKILL.md`](.agents/skills/unstage/SKILL.md) - 지정 asset 추적 제외와 `gd-upload/` 복사

## Human Documentation

Human-readable project documentation lives under `Docs/`:

- [`Docs/Guides/commit-rules.md`](Docs/Guides/commit-rules.md) - human-readable commit message guide
- [`Docs/Guides/branching-rules.md`](Docs/Guides/branching-rules.md) - human-readable branch workflow guide
- [`Docs/Operations/unstaged-assets.md`](Docs/Operations/unstaged-assets.md) - human guide and list for assets kept out of normal Git history
- [`Docs/Guides/unreal-coding-rules.md`](Docs/Guides/unreal-coding-rules.md) - human-readable Unreal class naming and GAS usage guide
- `Docs/Architecture/architecture.md` - tracked architecture notes, implementation findings, multiplayer design notes, and durable logic documentation when created
- [`Docs/KNOW_HOW.md`](Docs/KNOW_HOW.md) - subsystem-level how-it-works recipes for revisiting AI-authored implementation work
- [`Docs/Intent/`](Docs/Intent/) - tracked canonical development intent cards for functions, components, and Blueprint-facing behavior
- [`Docs/CodeIntentSite/index.html`](Docs/CodeIntentSite/index.html) - tracked static Code Intent entrypoint for team members who do not have local generator tools
- [`README.md`](README.md) - project overview, purpose, engine version, and repository notes

## File Locations

| Kind | Location | Tracked? | Notes |
| --- | --- | --- | --- |
| Canonical agent manifest | `AGENTS.md` | Yes | Root entrypoint for all agents |
| Tool redirect files | `CLAUDE.md`, `GEMINI.md` | Yes | Thin redirects to this file |
| Project MCP config | `.mcp.json`, `.gemini/settings.json`, `.codex/config.toml` | Yes | Cross-agent UnreelMCP setup |
| AI directives | `.agents/directives/` | Yes | Canonical user-authored instructions |
| Project-local skills | `.agents/skills/` | Yes | Reusable agent workflows for this repository |
| Reusable prompt templates | `.agents/prompts/` | Yes | Optional shared prompt text |
| Prompt reference assets | `.agents/prompt-assets/` | No | Images or other large prompt references |
| Agent working notes | `.agents/work/` | No | Optional scratch notes for complex tasks |
| Session coordination manifests | `.agents/work/active-sessions/*.md` | No | Per-session edit intent and currently edited paths |
| Human project docs | `Docs/` | Yes | Human-facing project documentation |
| Code Intent source docs | `Docs/Intent/` | Yes | Canonical development intent cards; edit these instead of generated output |
| Code Intent static site | `Docs/CodeIntentSite/` | Yes | Lightweight generated HTML that must work from a clean clone without local generators |
| Code Intent heavy artifacts | `gd-upload/CodeIntent/code-intent-heavy-artifacts.zip` | No | Ignored Google Drive upload bundle for Doxygen and Blueprint scan outputs |
| Architecture and logic findings | `Docs/Architecture/architecture.md`, `Docs/KNOW_HOW.md`, or relevant `Docs/` files | Yes | Durable findings requested by the user |

## Repository Expectations

Track project-authored files such as `Pragmata_2P.uproject`, `Config/`, `Content/`, `Source/`, `Docs/`, `.agents/directives/`, `README.md`, and this file.

Detailed repository safety, generated/local-state exclusions, root document restrictions, and binary asset safety live in [`.agents/directives/06-security-safety.md`](.agents/directives/06-security-safety.md).

## Tool-Specific Entry Points

- Claude Code: `CLAUDE.md` redirects here.
- Codex CLI: this file is auto-loaded.
- Gemini CLI: `GEMINI.md` redirects here.
