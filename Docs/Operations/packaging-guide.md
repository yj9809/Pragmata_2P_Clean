# Unreal Editor 패키징 가이드

목표: Unreal Editor에서 Windows Development package 생성.

1. Unreal Editor에서 `Pragmata_2P.uproject` 열기.

2. `Live Coding` 비활성 확인.
   - toolbar의 compile/live coding 상태 확인
   - Live Coding이 켜져 있거나 이전 빌드가 잠긴 상태면 Editor 종료 후 다시 열기

3. `Edit > Project Settings > Pragmata Game Flow` 확인.
   - `GameFlowClass`: `BP_GameFlow_Default`

4. `Edit > Project Settings > Maps & Modes` 확인.
   - `Server Default Map`: `/Game/Map/LunarMap_T`
   - `Demo`를 package에서 제외할 때 `Server Default Map`도 `Demo`가 아니어야 함

5. `Content/Blueprints/GameFlow/BP_GameFlow_Default` 열기.
   - `LobbyMap`: package에 포함할 lobby map
   - `LobbyGameMap`: lobby start 후 이동할 gameplay map
   - `GameplayGameMode`: `BP_PragGameMode`
   - 값을 바꿨으면 `Compile`, `Save`

6. `Edit > Project Settings > Packaging` 열기.

7. 기본 package 설정 확인.
   - `Build Configuration`: `Development`
   - `For Distribution`: off
   - `Use Pak File`: on
   - `Full Rebuild`: 필요할 때만 on

8. `List of maps to include in a packaged build` 확인.
   - `/Game/Map/Title`
   - `/Game/Map/MatchLobby`
   - `/Game/Map/LunarMap_T`
   - `BP_GameFlow_Default.LobbyMap`을 바꿨으면 그 map 추가
   - `BP_GameFlow_Default.LobbyGameMap`을 바꿨으면 그 map 추가
   - `Demo`, `PlayerTestLevel`은 Editor 전용 테스트 map이므로 추가하지 않음

9. `Additional Asset Directories to Cook`에 `/Game/Character`, `/Game/Enemy` 전체를 추가하지 않기.
   - 현재 유지 대상: `/NNEDenoiser`

10. `Content/CookLabels` 확인.
   - `PAL_CoreRuntime`
   - `PAL_LobbyPreview`
   - `PAL_EnemyRuntime`
   - 각 label의 `Cook Rule`: `Always Cook`
   - 각 label의 `Is Runtime Label`: on
   - 각 label의 `Explicit Assets` 목록 확인

11. 새 runtime asset을 추가, 이동, rename, soft-load했다면 cook owner 확인.
    - UI, Enhanced Input, GameMode, GameFlow asset: `PAL_CoreRuntime`
    - lobby preview mesh/idle animation: `PAL_LobbyPreview`
    - runtime enemy/hacked mesh 관련 asset: `PAL_EnemyRuntime`
    - map asset은 PAL이 아니라 `List of maps to include in a packaged build`에 추가
    - 새 runtime `IMC_*`, `IA_*`는 현재 정책상 `PAL_CoreRuntime`에 각각 직접 추가
    - C++ `TSoftObjectPtr`, `TSoftClassPtr`, `FSoftObjectPath`, string `/Game/...` load 추가 시 PAL 누락 여부 확인

12. PAL 누락 점검.
    - `PAL_CoreRuntime`에 runtime UI, GameFlow, GameMode, Enhanced Input asset 포함
    - `PAL_LobbyPreview`에 lobby preview mesh와 idle animation 포함
    - `PAL_EnemyRuntime`에 runtime enemy/hacked mesh 관련 asset 포함
    - 각 label의 `Cook Rule = Always Cook`
    - 각 label의 `Is Runtime Label = true`
    - directory recursive label 사용 금지, explicit asset 방식 유지

13. `File > Save All` 실행.

14. `Platforms > Windows > Package Project` 선택.

15. 출력 폴더 선택.
    - 권장 위치: `Builds/LANPlaytest-Win64-YYYYMMDD-HHMM`
    - `Builds/`는 Git 추적 제외 대상

16. packaging 완료까지 대기.
    - 성공 기준: `BUILD SUCCESSFUL`
    - 실패 시 `Output Log`에서 최초 `Error:` 또는 `UnknownCookFailure` 위쪽의 asset/load/link error 확인

17. 결과 확인.
    - `Builds/<package-name>/Windows/Pragmata_2P.exe` 존재
    - `Builds/<package-name>/Windows/Pragmata_2P/Binaries/Win64/Pragmata_2P.exe` 존재

18. packaged game 실행 후 smoke check.
    - title WBP 표시
    - title start 후 lobby 진입
    - lobby WBP 표시
    - ready/start 후 gameplay map 진입
    - Hugh HUD 표시
    - aim crosshair 표시
    - Executor hacked mesh 전환 시 load 실패 없음

19. packaged runtime log 확인.
    - 없어야 할 문자열:
      - `SkipPackage: /Game/UI`
      - `SkipPackage: /Game/Blueprints/Player/Input`
      - `Failed to load gameplay HUD class`
      - `Failed to load Hugh crosshair class`

20. packaging 후 새 map이나 runtime asset을 추가했다면 바로 설정 갱신.
    - GameFlow map 변경: Packaging map list 갱신
    - runtime non-map asset 변경: `Content/CookLabels` PAL explicit asset 갱신
    - 새 input asset 추가: `IMC_*`와 개별 `IA_*` 모두 `PAL_CoreRuntime` 확인

## Command-line 재현

AI agent 또는 터미널에서 같은 Windows Development package 생성 시 사용.

1. Unreal Editor 종료 확인.
   - `Live Coding`이 켜진 Editor가 남아 있으면 build lock 가능
   - 기존 package 보존 시 `Builds/` 삭제 금지

2. PowerShell에서 archive 폴더명 결정.
   - 형식: `Builds/LANPlaytest-Win64-YYYYMMDD-HHMMSS`
   - 기존 package와 다른 새 timestamp 사용

3. `RunUAT BuildCookRun` 실행.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  -project="C:\Workspace\UnrealProjects\Pragmata_2P\Pragmata_2P.uproject" `
  -noP4 `
  -platform=Win64 `
  -clientconfig=Development `
  -serverconfig=Development `
  -build `
  -cook `
  -stage `
  -pak `
  -archive `
  -archivedirectory="C:\Workspace\UnrealProjects\Pragmata_2P\Builds\LANPlaytest-Win64-YYYYMMDD-HHMMSS" `
  -utf8output
```

4. 성공 기준 확인.
   - `BUILD SUCCESSFUL`
   - `AutomationTool exiting with ExitCode=0`
   - `Builds/<package-name>/Windows/Pragmata_2P.exe` 존재
   - `Builds/<package-name>/Windows/Pragmata_2P/Binaries/Win64/Pragmata_2P.exe` 존재

5. 실패 시 우선 확인.
   - `Unable to build while Live Coding is active`: Editor 종료 후 재실행
   - `UnknownCookFailure`: cook log에서 최초 asset/load/link error 확인
   - `SkipPackage: /Game/UI` 또는 input asset 누락: `Content/CookLabels` PAL 확인

6. 최근 재현 예시.
   - 기존 보존 package: `Builds/LANPlaytest-Win64-20260703-075248`
   - 새 package: `Builds/LANPlaytest-Win64-20260703-083247`
