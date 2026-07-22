---
name: build-fix
description: "[언리얼 에디터 꺼짐] Pragmata_2P Unreal Editor target build 실행 전 현재 프로젝트 Unreal Editor를 자동 종료하고 compile/UHT/link 오류를 최소 수정. 사용자가 build, compile, Unreal C++ 검증, build error 수정, build-and-fix loop를 요청할 때 사용."
---

# Build Fix

## Workflow

`Pragmata_2P` Unreal build 검증과 compile-error 수정을 위한 project-local workflow.

경고: 이 스킬의 build script는 build 전에 Unreal Editor를 종료. 저장하지 않은 Editor asset 변경이 사라질 수 있음.

1. Load project directives before acting:
   - Always read `AGENTS.md`, `.agents/directives/01-behavior.md`, `.agents/directives/03-workflow.md`, and `.agents/directives/10-session-coordination.md`.
   - Before compiling or touching `Source/`, `.Build.cs`, or runtime config, read `.agents/directives/02-unreal-code.md`.
   - If a fix touches Blueprint assets, Editor automation, security-sensitive operations, or human docs, read the matching directive listed in `AGENTS.md`.
2. Run `git status --short --branch` and inspect active manifests under `.agents/work/active-sessions/`.
3. If editing tracked files, create or update this session's active manifest before edits.
4. Preserve existing dirty files. Do not revert, normalize, or opportunistically clean unrelated work.
5. Run the Editor target build with the bundled script. The script auto-closes Unreal Editor before build:

   ```powershell
   .\.agents\skills\build-fix\scripts\run-editor-build.ps1
   ```

6. Editor 종료 규칙:
   - First close only `UnrealEditor.exe` processes whose `CommandLine` matches the current `.uproject` path or `Pragmata_2P`.
   - If no project match exists but at least one Unreal Editor process cannot be identified by command line, close all `UnrealEditor.exe` processes.
   - If all visible Unreal Editor processes are clearly for other projects, leave them running.
7. If the build fails, read the generated log path printed by the script and extract the first actionable error group. Prefer UHT and compiler errors over trailing summary noise.
   - If the log says Live Coding is active, retry with this skill's current script before changing code.
   - If the log says `UnrealEditor-Pragmata_2P.dll` cannot be opened for write, confirm the script's Editor termination log before treating it as a compile or link problem.
8. Fix the smallest responsible code or config surface:
   - Missing complete Unreal type: add the correct include rather than rewriting logic around an incomplete type.
   - Shadow diagnostics: rename the local symbol; do not suppress warnings.
   - Reflection/UHT errors: fix the macro, property, include, or generated-body boundary directly.
   - Link/module errors: inspect module usage before changing `.Build.cs`; add only the dependency that is actually used.
9. For non-trivial C++ changes, apply the Korean intent-comment rules from `02-unreal-code.md`.
10. Re-run the build after each focused fix until it passes or a non-local blocker is proven.
11. Before final response, run `git diff --check` for touched text files when text edits were made.
12. Report:
    - Build command and result.
    - Log path.
    - Unreal Editor termination result.
    - Files changed.
    - Whether comment coverage, durable docs, validation, and unrelated-file protection were checked.

## Build Target

Default target:

```powershell
Build.bat Pragmata_2PEditor Win64 Development -Project=Pragmata_2P.uproject -WaitMutex -NoHotReloadFromIDE
```

The script resolves the repo root from the skill directory, defaults to `C:\Program Files\Epic Games\UE_5.8`, and writes logs to `.agents/work/build-fix/`.

## Log Reading

Use narrow searches before reading entire logs:

```powershell
rg -n "error C|fatal error|UnrealHeaderTool|UHT|error:|warning C" .agents/work/build-fix
```

Treat warnings-as-errors, UHT failures, and missing module dependencies as build blockers. If the first visible error is a cascade, inspect the nearest earlier file-specific error.

## Boundaries

- Do not run a full build unless the user asked for it; this skill is for explicit build requests.
- Do not modify assets or launch Editor automation unless the user request and directives allow it.
- Do not commit, stage, push, stash, switch branches, or merge unless separately requested.
