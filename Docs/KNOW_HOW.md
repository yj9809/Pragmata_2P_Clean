# Know How

## Attack Montage Flying Movement

문제:

- 장거리 점프·비행 공격의 root motion은 Walking 상태에서 지면 처리와 충돌할 수 있음

현재 해법:

- `FMonsterAttackPattern.Use Flying Movement During Attack Montage`를 켜면 server가 montage 재생 성공 직후 `MOVE_Flying`으로 전환
- `Use Proximity Controlled Montage Loop`와 함께 켜면 자기 반복 `Loop` section에서 montage root motion을 잠시 무시하고 `Flying Loop Move Speed`(cm/s)로 전진
- 최대 전진 거리는 `Flying Loop Move Speed × Max Loop Duration`이며, `End Transition Distance`에 먼저 도달하면 그 시점에 `End Section Name`으로 전환
- montage 종료, 피격 취소, 사망 정리에서는 공격 시작 전 `MovementMode`와 `CustomMovementMode`를 복구
- Loop를 벗어나 End section에 진입하면 montage의 기존 root motion 모드를 다시 사용

검증 방법:

- `BP_Sentinel`의 해당 Attack Pattern에서 `Use Flying Movement During Attack Montage` 활성화
- montage `Start -> Loop -> End` 연결, `Loop`의 Next Section을 자기 자신으로 설정
- `Flying Loop Move Speed`와 `Max Loop Duration`의 곱에 맞는 최대 전진 거리 및 `End Transition Distance` 조기 전환 확인
- PIE에서 montage 재생 중 `MOVE_Flying`, End·피격 취소 뒤 공격 전 이동 모드·`MaxFlySpeed` 복구 확인
- listen server와 client에서 같은 전진 이동과 End section 전환 확인

AI agent가 만든 주요 흐름을 나중에 다시 읽고 빠르게 복구하기 위한 문서.
파일별 변경 목록이 아니라 subsystem별 동작 방식과 수정 기준을 기록한다.

새 문제를 해결하거나 기존 로직을 개선하면 관련 항목을 바로 갱신한다.
반복 디버깅 중 새 원인이나 해법이 확정되면 마지막에 몰아서 쓰지 않고 그때마다 반영한다.

## 문서 사용법

- 먼저 이 문서에서 흐름과 수정 기준 확인
- 설계 이유와 더 긴 근거는 `Docs/Architecture/architecture.md` 확인
- Editor/Blueprint에서 직접 만질 값은 `Docs/Operations/blueprint-tasks.md` 확인
- network/replication 디버그는 `Docs/Systems/Server/server.md`, `Docs/Architecture/replication-role-animation-fix.md` 확인

항목 형식:

- 문제
- 현재 해법
- 동작 흐름
- 핵심 위치
- 수정할 때 주의
- 검증 방법
- 관련 문서

## Title GameFlow

문제:

- Lobby가 title gate와 session UI를 동시에 맡으면 시작 입력, fade, session state가 섞임
- user가 game 시작 지점을 다시 이해하기 어려움
- map 전환값이 C++ 기본값 여러 곳에 흩어지면 Editor에서 흐름을 바꾸기 어려움

현재 해법:

- `/Game/Map/Title`이 title 전용 entrypoint
- `UPragmataGameFlowConfig` class default가 title 이후 흐름을 소유
- title 입력 후 fade-out, `LobbyMap` travel
- title controller는 현재 map이 `/Game/Map/Title`일 때만 UI/input/travel 처리
- lobby ready/start 후 host가 `LobbyGameMap?game=GameplayGameMode`로 server travel

동작 흐름:

```text
Title
-> APragmataTitlePlayerController
-> WBP_TitleScreen
-> fade
-> UPragmataGameFlowConfig
-> LobbyMap

LobbyMap
-> WBP_RoleLobby
-> LobbyGameMap
-> GameplayGameMode
```

핵심 위치:

- `UPragmataGameFlowConfig`, `UPragmataGameFlowSettings`
- `APragmataTitleGameMode`, `APragmataTitlePlayerController`, `UPragmataTitleScreenWidget`
- `APragmataLobbyGameMode`, `APragmataLobbyPlayerController`, `UPragmataRoleLobbyWidget`
- `PragmataSessionUtils::BuildGameTravelURL`
- 기본값: `/Game/Map/Title`, `/Game/Map/MatchLobby`, `/Game/Map/Demo`, `/Game/Blueprints/BP_PragGameMode`

수정할 때 주의:

- map/game mode 변경은 GameFlow Blueprint class defaults에서 조정
- GameFlow class 연결은 Project Settings > Pragmata Game Flow의 `GameFlowClass`
- MatchLobby는 press-any-button gate를 다시 소유하지 않음
- 특정 gameplay map 테스트는 해당 map을 직접 열고 listen/session flow로 시작
- title showcase actor는 playable pawn이 아님
- title camera가 없을 때 fallback camera는 개발 안전장치일 뿐 기본 구조가 아님
- `.omx` 이전 세션의 `TestArea`, `PlayerTestLevel` entrypoint 언급은 현재 구조가 아님

검증 방법:

- PIE 시작 시 Title map 진입
- mouse click, Enter, Space, gamepad face bottom으로 start
- fade 후 `LobbyMap` 진입
- ready/start 후 `LobbyGameMap` travel
- `Demo`, `PlayerTestLevel` 등 임의 map을 직접 열면 title GameFlow 자동 travel 없음

관련 문서:

- `Docs/Architecture/architecture.md`의 `Title Screen Flow`, `Game Flow`
- `Docs/Operations/blueprint-tasks.md`의 title/map/UI 항목

## LAN Packaging Maps

문제:

- `BP_GameFlow_Default`에서 map을 바꾸면 packaged build에 해당 map이 cook되지 않을 수 있음
- packaging cook은 Editor에서 보이는 map 전환 의도보다 `ProjectPackagingSettings.MapsToCook`를 우선 확인
- C++ soft load asset은 Editor에서 보여도 packaged build에 자동 포함되지 않을 수 있음
- level 안의 preview actor가 불량 animation asset을 직접 참조하면 gameplay에서 쓰지 않아도 cook blocker가 될 수 있음

발견 증상:

- `UWaistArmComponent::SetArmState`가 `LNK2019 unresolved external symbol`로 Game target link 실패
- `aqProf.dll`, `VtuneApi.dll`, `WinPixGpuCapturer.dll` load 실패가 먼저 보이지만 실제 blocker는 아님
- `PlayerTestLevel` cook 중 `wp0900_00_unreal_Anim_Armature_wp0900_00_Select_Start_` hash-change assertion 발생
- 같은 구간에서 `wp0900_00_unreal_Skeleton` missing bones, `Bone with no name detected` warning 반복

현재 해법:

- LAN play test package는 GameFlow에서 실제 도달 가능한 map만 `MapsToCook`에 명시
- 현재 map list:
  - `/Game/Map/Title`
  - `/Game/Map/MatchLobby`
  - `/Game/Map/LunarMap_T`
- `Demo`, `PlayerTestLevel`은 Editor 전용 테스트 map으로 package cook에서 제외
- runtime soft load 보장은 `/Game/CookLabels`의 `PrimaryAssetLabel`로 관리
- 현재 label:
  - `PAL_CoreRuntime`: GameFlow, GameMode, UI, Enhanced Input, lobby ring material
  - `PAL_LobbyPreview`: lobby preview mesh와 idle animation
  - `PAL_EnemyRuntime`: Executor hacked mesh와 보수적 AI/enemy hard-load asset
- PAL 운영 원칙:
  - 새 runtime non-map asset은 cook owner 확인 필수
  - `TSoftObjectPtr`, `TSoftClassPtr`, `FSoftObjectPath`, string `/Game/...` load 대상은 PAL explicit asset 후보
  - 새 runtime `IMC_*`, `IA_*`는 `PAL_CoreRuntime`에 각각 직접 추가
  - 이미 PAL에 들어간 `IMC_*`가 `IA_*`를 참조해도, 현재 프로젝트 정책은 개별 `IA_*`도 명시
  - map asset은 PAL이 아니라 `MapsToCook`에 추가
  - `/Game/Character`, `/Game/Enemy` 전체 directory cook 금지
- `PAL_CoreRuntime` explicit asset:
  - `BP_GameFlow_Default`, `BP_PragGameMode`
  - `WBP_TitleScreen`, `WBP_RoleLobby`, `WBP_HughHUD`, `WBP_DianaHUD`, `WBP_Crosshair`
  - `IMC_Title`, `IMC_Lobby`, `IMC_Hugh`, `IMC_Diana`
  - `IA_TitleStart`, `IA_LobbySelect`, `IA_LobbySkinSelect`, `IA_LobbyConfirm`
  - `IA_Move`, `IA_Look`, `IA_Attack`, `IA_Dodge`, `IA_Jump`, `IA_Aim`, `IA_Boost`, `IA_HeadVisorLightToggle`, `IA_Heal`, `IA_Scan`, `IA_Interact`
- `PAL_LobbyPreview` explicit asset:
  - `SM_Hugh`, `SM_Hugh_Scribble`, `SM_Diana`
  - `AS_Hugh_General_0100_Stand_Loop_`, `AS_Diana_General_0100_Stan_Loop`
- `PAL_EnemyRuntime` explicit asset:
  - `SM_Executor_Hacked`
  - `BB_Monster`, `SM_Executor`, `ABP_ExecutorMonster`, `BP_DamageText`
- `UWaistArmComponent::SetArmState`는 `UFUNCTION` wrapper가 link할 수 있게 header에서 `FORCEINLINE` 제거
- Shipping target은 `AssetTypeActions/*`처럼 Editor 모듈에만 있는 헤더를 runtime `.cpp`에서 include하지 않음. sound type이 필요하면 runtime `Sound/SoundBase.h`를 사용
- package output은 `Builds/` 아래 생성하고 Git 추적 제외

동작 흐름:

```text
BP_GameFlow_Default map 변경
-> Edit > Project Settings > Packaging
-> MapsToCook에 실제 package map list 반영
-> soft load asset 추가 시 /Game/CookLabels의 PAL explicit asset에 추가
-> Package Project 또는 BuildCookRun
-> Cook log에서 missing map, bad asset reference, link error 확인
```

핵심 위치:

- `Config/DefaultGame.ini`의 `[/Script/UnrealEd.ProjectPackagingSettings]`
- `Content/Blueprints/GameFlow/BP_GameFlow_Default`
- `Content/CookLabels/PAL_CoreRuntime`
- `Content/CookLabels/PAL_LobbyPreview`
- `Content/CookLabels/PAL_EnemyRuntime`
- `Source/Pragmata_2P/Characters/WaistArmComponent.h`
- `.gitignore`의 `Builds/*`

수정할 때 주의:

- `LobbyMap`과 `LobbyGameMap` 모두 cook list에 필요
- `/Game/Character`, `/Game/Enemy` 전체를 `DirectoriesToAlwaysCook`에 넣지 않음
- Character/Enemy animation은 PAL explicit asset으로 필요한 것만 추가
- `TSoftObjectPtr`, `TSoftClassPtr`, `FSoftObjectPath`, string path load 추가 시 PAL 누락 여부 확인
- runtime `IMC_*`, `IA_*` 추가 또는 이동 시 `PAL_CoreRuntime` explicit asset을 항상 재검증
- 새 runtime asset을 만든 뒤 PIE에서 보인다는 이유만으로 packaged cook 포함을 가정하지 않음
- map 전환 대상은 PAL이 아니라 `MapsToCook`에 추가
- PAL은 directory recursive label을 끄고 explicit asset만 사용
- package log의 `aqProf.dll`, `VtuneApi.dll`, `WinPixGpuCapturer.dll` load 실패는 Windows optional profiler DLL noise
- `Error_UnknownCookFailure`는 위쪽 cook log에서 최초 asset assertion 또는 load error를 먼저 확인
- standalone preview actor가 bad animation을 직접 참조하면 map에서 제거하거나 정상 animation으로 교체
- 단순 asset resave가 cook hash-change assertion을 해결하지 못하면 level reference graph를 먼저 확인
- `UFUNCTION` 선언에 `FORCEINLINE`을 붙이고 `.cpp` 구현만 두면 generated wrapper가 link할 외부 symbol을 찾지 못할 수 있음
- Shipping compile에서 `AssetTypeActions/*` missing include가 나오면 해당 Editor-only type의 실제 사용 여부를 먼저 확인하고, 미사용 include는 제거

검증 방법:

- `BuildCookRun ... -platform=Win64 -clientconfig=Shipping -build -cook -stage -pak -archive`
- `MapsToCook`에 `/Game/Map/Title`, `/Game/Map/MatchLobby`, `/Game/Map/Demo`, `/Game/Map/LunarMap_T` 포함 확인
- `BUILD SUCCESSFUL`, `AutomationTool exiting with ExitCode=0`
- `/Game/CookLabels`에 `PrimaryAssetLabel` 3개 존재 확인
- 각 label의 `Rules.CookRule = AlwaysCook`, `bIsRuntimeLabel = true`, explicit asset 목록 확인
- 새 runtime asset 또는 moved asset이 있으면 `PAL_CoreRuntime`, `PAL_LobbyPreview`, `PAL_EnemyRuntime` 중 하나에 포함됐는지 확인
- 새 runtime map이면 PAL이 아니라 `MapsToCook`에 포함됐는지 확인
- packaged log에 `SkipPackage: /Game/UI`, `SkipPackage: /Game/Blueprints/Player/Input` 없음
- 출력 폴더의 `Windows/Pragmata_2P.exe` 존재 확인

## Room Code Session

문제:

- host/join 버튼이 gameplay role 의미까지 갖기 시작하면 lobby 선택과 network action이 섞임
- LAN session 검색 타이밍 차이로 join이 너무 빨리 실패할 수 있음
- packaged build에서 host session connect string이 `172.x.x.x:0`으로 광고되면 client가 timeout까지 대기

현재 해법:

- 6자리 `RoomCode`를 session setting과 travel URL에 유지
- host는 room code를 먼저 생성하고 LAN listen session 생성
- join은 입력 code를 정규화한 뒤 LAN session을 retry 검색
- host/join은 network action이고, character role은 lobby 선택으로 분리
- host 최초 listen lobby 진입은 `ServerTravel`이 아니라 `OpenLevel(Map, "listen?...")`
- client는 resolved connect string이 `:0`이면 `ClientTravel` 전에 실패 처리

동작 흐름:

```text
Host
-> GenerateRoomCode
-> CreateSession
-> OpenLevel MatchLobby with listen?RoomCode=code

Join
-> NormalizeRoomCode
-> FindSessions retry
-> ClientTravel connect?RoomCode=code
```

핵심 위치:

- `UPragmataMultiplayerSessionSubsystem`
- `PragmataSessionUtils`
- `UPragmataRoleLobbyWidget`

수정할 때 주의:

- Host/Join 기본 경로는 `PreferredRole`을 보내지 않고, character 선택은 두 player 입장 후 lobby phase에서 시작
- 옛 `PreferredRole` URL option은 lobby 초기 선택값 하위 호환일 뿐 Demo spawn 근거가 아님
- room code는 인증 수단이 아니라 같은 LAN session 검색 키
- code 공간은 혼동 문자 제외 32자 alphabet의 6자리라 `32^6 = 1,073,741,824`
- 다른 활성 LAN session 1개와 충돌 확률은 약 10.7억 분의 1
- session advertise는 Hugh/Diana 둘 다 연결된 뒤 끔
- ambiguous code session은 자동 선택하지 않고 실패 처리
- host 최초 listen 전환에서 `World->ServerTravel("/Game/Map/MatchLobby?listen...")`를 쓰지 않음
- `OnlineSubsystemNull`은 session response를 만들 때 현재 `NetDriver` port를 읽으므로 listen `NetDriver`가 있어야 `:7777` 같은 실제 port가 광고됨
- `:0` connect string은 방화벽 문제가 아니라 listen port 미준비 상태
- 현재 LAN 범위에서는 수정 불필요, 공개 backend/대규모 room에서는 code 고유성 예약 필요

검증 방법:

- host room code 표시
- join code 입력 후 MatchLobby 참가
- packaged client log의 `Browse:` 주소가 `:0`이 아님
- host log에 game listen port 생성 확인
- 없는 code는 retry 후 실패 message
- 같은 code 중복 session은 ambiguous 실패

관련 문서:

- `Docs/Architecture/architecture.md`의 `Listen Server Session`
- `Docs/Systems/Server/server.md`의 `Session Flow`, `Travel Options`

## Role State

문제:

- lobby 선택, gameplay spawn, 향후 control swap이 같은 enum 하나에 묶이면 버그 원인 추적이 어려움
- host/join 순서와 character role을 혼동하기 쉬움

현재 해법:

| State | 의미 |
| --- | --- |
| `LobbySelectedRole` | lobby에서 고른 character 선택 |
| `AssignedRole` | gameplay spawn, team identity, handoff 기준 |
| `ActiveControlRole` | 현재 input/camera 기준, 향후 control swap 준비값 |

동작 흐름:

```text
Host/Join session
-> 두 player lobby 입장
-> PlayerState.LobbySelectedRole 기본 배정
-> A/D selection input
-> ready/start 검증
-> PlayerState.AssignedRole
-> Demo spawn
-> ActiveControlRole 기반 input/camera
```

핵심 위치:

- `APragmataPlayerState`
- `APragmataLobbyGameMode`
- `APragmataGameMode`
- `APragmataPlayerController`

수정할 때 주의:

- `AssignedRole`은 control swap 때도 기본 identity로 남김
- `ActiveControlRole` 변경은 pawn ownership 변경과 다름
- host가 항상 Hugh라는 가정 금지

검증 방법:

- Host Hugh / Client Diana
- Host Diana / Client Hugh
- 같은 role ready/start 거부
- role별 input mapping과 possession 확인

관련 문서:

- `Docs/Architecture/architecture.md`의 `Role preselect / network action 분리`
- `Docs/Systems/Server/server.md`의 `Role State`

## Lobby to Gameplay Role Handoff

문제:

- gameplay travel 때 player별 URL에는 role이 없음
- PIE non-seamless travel에서는 `PlayerState::CopyProperties`만 믿으면 role이 사라질 수 있음
- role resolution 실패가 Hugh fallback spawn으로 숨으면 Diana possession 버그처럼 보임

현재 해법:

- lobby start 직전에 선택/ready 상태를 서버에서 재검증
- `AssignedRole` 확정 후 subsystem에 handoff 저장
- session advertising on/off는 `UPragmataMultiplayerSessionSubsystem::SetSessionAdvertisingEnabled()` C++ 경로로만 제어하고 Blueprint API로 열지 않음
- gameplay GameMode는 여러 source를 순서대로 확인
- role이 끝까지 없으면 Hugh fallback을 쓰지 않고 spawn 차단
- `InitNewPlayer`의 lobby defer는 role 미확정(`pragmata_invalid_role`)만 허용하고, room code/class/slot 오류는 즉시 반환
- gameplay controller는 possession 직후 `FInputModeGameOnly`와 이동·시점 입력을 즉시 복구한다.
- MatchLobby spinner는 travel 직전 피드백만 담당하며 gameplay map에서 role pawn 등록을 기다리는 입력 잠금으로 이어지지 않는다.

동작 흐름:

```text
Lobby ready/start
-> AssignedRole 확정
-> RegisterLobbyRoleHandoff
-> ServerTravel LunarMap_T
-> InitNewPlayer / HandleStartingNewPlayer
-> role resolution
-> RestartPlayer
-> role pawn spawn
```

핵심 위치:

- `APragmataLobbyGameMode::RequestStartGame`
- `UPragmataMultiplayerSessionSubsystem::RegisterLobbyRoleHandoff`
- `APragmataGameMode`
- `APragmataGameState`

수정할 때 주의:

- role resolution 순서 변경 시 Host Diana / Client Hugh를 반드시 재검증
- `DefaultPawnClass`로 Hugh fallback을 되살리지 않음
- `LunarMap_T`는 단일 `PlayerStart` anchor와 Diana offset을 사용

검증 방법:

- Lobby에서 서로 다른 role 선택 후 ready/start
- `LunarMap_T`에서 Hugh는 `AHughCharacter`, Diana는 `ADianaOperatorPawn` possess
- `APragmataGameState`의 Hugh/Diana pointer와 connected flag 확인
- host/client가 `LunarMap_T`에서 각 pawn을 possess한 직후 이동과 시점 입력이 가능한지 확인

관련 문서:

- `Docs/Architecture/architecture.md`의 `Demo 역할 스폰 이슈`, `Lobby Session and Travel Role`
- `Docs/Architecture/replication-role-animation-fix.md`의 `Role handoff test`

## Role Lobby UI and Preview

문제:

- C++ fallback UI가 커지면 WBP layout과 runtime state가 어긋남
- lobby preview actor가 gameplay pawn처럼 오해될 수 있음

현재 해법:

- `WBP_RoleLobby`는 phase 전환 coordinator로 남고, `WBP_LobbyMatchmaking`과 `WBP_LobbyCharacterSelect`가 각 phase layout, label, visual affordance를 소유
- C++는 named widget binding과 session/lobby state 연결만 담당
- `WBP_RoleLobby`는 `WBP_HughHUD`처럼 root `CanvasPanel` 바로 아래에 화면 요소를 두고, full-screen/center/right Canvas slot anchor로 viewport 대응
- 전역 1920x1080 ScaleBox wrapper는 쓰지 않고, 필요한 child WBP 내부에서만 `ScaleBox`를 사용
- `UPragmataRoleLobbyWidget`의 `Trigger...View` 함수는 WBP 디자이너가 호출할 수 있는 명시적 view 전환 entry point이며, 호출 시 이전 lobby mode widgets를 먼저 숨긴 뒤 필요한 영역만 켬
- `RoomCodeText`, `StatusText`, `CopyButton`, `DestroyButton`은 matchmaking child WBP 내부 widget이므로 WaitingForPlayer/CharacterSelect에서 이들을 보일 때 `LobbyMatchmakingWidget` parent도 같이 켜야 함
- `UPragmataRoleLobbyWidget` button handler는 `HostLobbySession`/`JoinLobbySession`을 직접 호출하고, legacy `HostAsHugh`/`JoinAsDiana`는 deprecated wrapper로만 유지
- Host/Join은 preferred role 없이 session만 만들고, character 선택은 두 player가 모두 들어와 `CharacterSelect` 또는 `ReadyToStart` phase가 된 뒤에만 허용
- 두 번째 player가 들어오면 서버는 `PlayerJoining` phase를 먼저 복제해 host/client 모두 joining overlay를 보게 한 뒤 character select로 전환
- 역할 상태 text 색상은 C++가 상태 의미만 구분한다. 내 선택은 파란색, 상대 선택은 빨간색, 선택 충돌은 노란색, 빈 역할은 흰색
- role 선택은 A/D, Left/Right, `Gamepad_LeftX`가 맡고 skin 선택은 W/S, Up/Down, `Gamepad_LeftY`가 맡음
- `ULobbyCharacterSkinSet`이 role별 skin id, code label, UI label, skeletal mesh soft reference를 소유
- Hugh skin은 `DelphiSuit`(`기본 · 델피 우주복`)과 `ScribbleSuit`(`낙서 우주복`)를 순환하고 Diana는 현재 `DianaDefault` 단일 skin으로 고정
- `APragmataPlayerState::LobbySelectedSkinId`가 lobby UI, preview, Demo handoff의 skin 원천
- role이 바뀌면 서버가 해당 role의 기본 skin으로 되돌리고 ready를 해제
- `ALobbyPresentationDirector`가 MatchLobby camera orbit과 preview animation sequence 전환을 담당
- camera는 `(0,0,0)`을 orbit center로 삼고 낮은 `ScreenRightOffset`만 적용해 character가 화면 왼쪽 안쪽에 남도록 함
- Hugh -> Diana 전환은 clockwise, Diana -> Hugh 전환은 counter-clockwise로 angle을 보정
- Hugh/Diana focus 모두 두 preview character가 화면 왼쪽 영역 안에 들어오도록 더 넓은 radius/FOV를 사용
- level에 배치한 `SkeletalMeshActor`가 `LobbyPreview.Hugh`, `LobbyPreview.Diana` tag를 가지면 director가 그 mesh를 우선 사용
- legacy `ALobbyCharacterPreviewActor` C++ code는 제거됨
- selection ring과 1P/2P label은 lobby selection affordance로 사용하지 않음
- preview animation은 AnimBP 없이 Single Node Animation 사용
- focus animation은 start asset 1회 재생 후 loop asset으로 전환
- skin mesh 교체가 Single Node 상태를 초기화하면 dirty 상태만 기록하고, 모든 skin 갱신 뒤 `FocusRole`이 현재 target sequence를 두 preview에 한 번 재적용
- 같은 mesh 갱신은 animation dirty 상태를 만들지 않아 start animation을 재시작하지 않음
- animation set DataAsset이 있으면 우선 사용하고, 없으면 C++ fallback soft path를 사용
- skin set DataAsset이 있으면 우선 사용하고, 없으면 C++ fallback skin 정의를 사용
- preview skin mesh는 preview 표시 전에 `LoadAllMeshes()`로 미리 올리고, mesh load 실패 시 기존 mesh를 유지
- 두 player가 모이기 전에는 `ALobbyPresentationDirector`가 preview skeletal mesh를 숨기고 animation timer를 멈춤
- `PlayerJoining -> CharacterSelect` 전환 때 `WBP_RoleLobby`는 dimmer만 남긴 black empty screen을 잠깐 보여준 뒤 character select 화면으로 fade-in
- `StartingGame` phase에서는 `WBP_RoleLobby`가 모든 기존 UI를 숨기고 full-screen dimmer + spinner만 표시
- `ReadyToStart` phase에서 host ready button label은 `게임 시작`

동작 흐름:

```text
APragmataLobbyPlayerController
-> WBP_RoleLobby 표시
-> Host/Join session flow
-> two players joined
-> PlayerJoining overlay + spinner
-> black empty reveal transition
-> server-selected default role 표시
-> A/D input prediction
-> W/S input prediction으로 skin label + preview mesh 교체
-> ALobbyPresentationDirector preview 표시 + FocusRole
-> orbit camera 보간 + Hugh/Diana focus animation 전환
-> server PlayerState state 표시
-> both ready면 host button "게임 시작"
-> host start click
-> StartingGame replicated
-> 양쪽 full-screen spinner
-> delayed ServerTravel
```

핵심 위치:

- `UPragmataRoleLobbyWidget`
- `APragmataLobbyPlayerController`
- `APragmataLobbyGameMode`
- `ALobbyPresentationDirector`
- `ULobbyPreviewAnimationSet`
- `ULobbyCharacterSkinSet`
- `/Game/UI/Widget/WBP_RoleLobby`
- `/Game/UI/Widget/WBP_LobbyMatchmaking`
- `/Game/UI/Widget/WBP_LobbyCharacterSelect`
- `/Game/Blueprints/Player/Input/Common/IA_LobbySkinSelect`
- `/Game/Map/MatchLobby`

수정할 때 주의:

- C++에서 fallback UI 생성 금지
- `Trigger...View`는 visual layout을 만들지 않고 visibility/state만 바꾸게 유지
- child WBP 내부 widget을 직접 보이게 하는 phase에서는 해당 child WBP parent visibility를 먼저 확인
- `BindWidget` 이름 변경 시 WBP와 C++를 같이 갱신
- skin label은 `SkinLabelPanel`, `SkinNameText`, `SkinCodeLabelText` optional bind 이름을 사용
- skin label text는 `SkinCodeLabelScaleBox`, `SkinNameScaleBox` 아래에 두어 긴 localized label이 viewport 변화에서 넘치지 않게 유지
- root `WBP_RoleLobby`는 coordinator로 남기고 matchmaking/character selection child WBP가 visual layout을 소유
- `WBP_RoleLobby` 배치는 `WBP_HughHUD`의 root CanvasPanel 방식이 기준이며, 전역 design-size ScaleBox를 다시 추가하지 않음
- lobby child WBP의 한글 visible text는 `FontObject=None`인 기본 composite font fallback을 사용하고, room code처럼 ASCII 코드성 텍스트만 Oxanium/Orbitron 계열을 사용
- imported `FontFace`를 한글 TextBlock에 직접 지정하면 packaged/PIE에서 LastResort glyph box가 뜰 수 있으므로 사용 금지
- button은 default gray Slate brush 대신 tinted `RoundedBox` style을 사용
- child WBP root `ScaleBox`는 parent slot보다 커질 때만 축소되게 두고, root `WBP_RoleLobby`는 Canvas slot anchor와 engine DPI scaling에 맡김
- `/Game/Data/DA_LobbyCharacterSkinSet`, `/Game/UI/Widget/WBP_LobbyMatchmaking`, `/Game/UI/Widget/WBP_LobbyCharacterSelect`는 새 native C++ class를 포함한 Editor target build와 Editor 재시작 후 생성
- joining overlay 문구는 `SessionOverlayStatusText`가 소유하며 `SessionMatchDimmer`보다 높은 z-order를 유지
- preview actor는 gameplay role이나 possession의 source가 아님
- camera focus가 선택 affordance이므로 selection ring/1P/2P label을 되살리지 않음
- 두 player가 모이기 전에는 character preview, role status, role description을 노출하지 않음
- map에 regular `SkeletalMeshActor`를 둘 때 actor tag를 정확히 맞춤
- `LobbyPresentationDirector`가 없으면 local controller가 runtime fallback director를 생성하지만, 최종 composition은 MatchLobby map 배치 actor로 조정
- `LobbyPreview.Hugh`, `LobbyPreview.Diana` tag가 없으면 character mesh animation은 재생되지 않으므로 map actor tag부터 확인
- `ScreenRightOffset`을 크게 올리면 Hugh/Diana가 왼쪽 화면 밖으로 밀리므로 오른쪽 UI 공간은 WBP panel 배치로 확보
- soft load animation/skin/input을 추가하면 `/Game/CookLabels` PAL coverage를 확인
- `DA_LobbyCharacterSkinSet`와 child WBP asset은 새 C++ class build/restart 후 생성 가능
- actor `IdleAnimation`이 C++ role default가 아니면 명시 override로 우선
- actor `IdleAnimation`이 role default이면 map 배치 `MeshComponent` animation이 우선
- map 배치 `MeshComponent` animation도 없으면 C++ role default idle 사용

검증 방법:

- MatchLobby 최초 진입 시 Host/Join/room code 입력은 role 선택 없이 표시
- 두 player가 모이기 전 A/D 또는 좌우 입력으로 role이 바뀌지 않음
- 두 player가 모인 뒤 W/S, Up/Down, left stick Y로 Hugh skin이 `델피 우주복`/`낙서 우주복` 사이에서 바뀜
- Diana는 W/S 입력에도 `DianaDefault` 단일 skin 유지
- skin label과 preview skeletal mesh가 같은 frame에 바뀌고 원격 client도 replicated `LobbySelectedSkinId`로 같은 mesh를 봄
- ready 후 Demo에서 Hugh main mesh와 `DianaAvatarMesh`가 lobby 선택 skin과 일치
- 두 player가 모이기 전 Hugh/Diana preview mesh와 role status/description이 보이지 않음
- host가 두 번째 player를 받으면 code/copy/close session UI가 사라지고 `플레이어가 참가 중입니다...` + spinner만 보임
- joiner가 matching session을 찾으면 기존 UI가 사라지고 `세션을 찾았습니다. 참가 중입니다...` + spinner만 보임
- `PlayerJoining -> CharacterSelect` 전환 중에는 텍스트/스피너 없이 black empty screen만 보인 뒤 character select가 드러남
- Creating/Searching/Joining overlay 표시
- room code 표시 후 host overlay 숨김
- local player prediction과 remote PlayerState 표시 구분
- `MatchLobby` 진입 후 director camera가 로비 camera view target이 됨
- Hugh -> Diana는 clockwise, Diana -> Hugh는 counter-clockwise로 이동
- focus target별 start animation 1회 후 loop animation 유지
- 양쪽 ready 후 host button label이 `게임 시작`으로 바뀜
- host start click 후 host/client 모두 이전 UI 없이 spinner만 표시
- packaged LAN 검증 전 `PAL_CoreRuntime`에 `IA_LobbySkinSelect`, `WBP_LobbyMatchmaking`, `WBP_LobbyCharacterSelect`, `PAL_LobbyPreview`에 `SM_Hugh_Scribble` 포함 확인
- `DA_LobbyCharacterSkinSet` 생성 후에는 `PAL_LobbyPreview` explicit asset에도 추가하고 `Cook Rule=Always Cook`, `bIsRuntimeLabel=true` read-back 확인

관련 문서:

- `Docs/Operations/blueprint-tasks.md`의 `Role Lobby UI`, `Lobby Character Selection`
- `Docs/Architecture/architecture.md`의 `Role Lobby UI`, `Lobby Character Preview`
- `Docs/Intent/Lobby/LobbyPresentationDirector-FocusRole.md`
- `Docs/Intent/Lobby/PragmataLobbyGameMode-StartingGame.md`

## Hugh and Diana Control

문제:

- Diana는 Hugh mesh에 붙은 avatar와 별도 operator pawn을 동시에 사용
- input, camera, possession을 섞으면 Diana client가 Hugh처럼 보이거나 움직임이 막힘

현재 해법:

- Hugh player는 `AHughCharacter` possess
- Diana player는 `ADianaOperatorPawn` possess
- Hugh 등에 붙은 Diana skeletal mesh는 표현용 avatar

### Diana skill body/face animation slot

- `DianaAvatarMesh`는 `ABP_Diana`를 유지한다. skill animation에 `PlayAnimation()` 또는 single-node mode를 사용하지 않는다.
- `ABP_Diana`의 `DianaSkillBody` slot은 locomotion/mount pose 위 body animation을 재생한다.
- `DianaSkillBody` slot이 active인 동안 오른손 IK를 0으로 내려 Cleanse, OverDrive, Scan, 획득 animation의 손 pose를 보존하고 slot 종료 후 원래 IK 정책으로 복구한다.
- `DianaSkillFace` slot은 `Layered Blend per Bone`의 `Neck` branch로 합성한다. body/IK pose를 face sequence가 덮지 않게 한다.
- C++ entry: `UDianaAnimInstance::PlaySkillBodyAnimation()`, `PlaySkillFaceAnimation()`, `PlayIdleFaceAnimation()`.
- 새 soft-loaded Diana animation은 `/Game/CookLabels/PAL_CoreRuntime` explicit asset에 추가 후 asset save와 build로 확인.
- Diana 카메라 SpringArm은 operator pawn 소유이므로 Hugh capsule/main mesh가 `Camera` channel을 막으면 조준 시 arm이 Hugh 내부로 축소됨. Hugh의 capsule/main mesh는 `Camera`를 ignore하고 벽 등 world collision은 SpringArm이 계속 처리
- 현재 카메라 기준값: Hugh default arm 270, aim arm 240, Diana default arm 270, aim arm 200, 양쪽 socket offset `(0, 80, 70)`, Diana Hugh anchor offset `(0, 0, 0)`
- controller는 `ActiveControlRole`에 맞춰 input mapping과 camera rule 적용
- pawn은 `UInputAction` binding만 소유하고 mapping context 추가는 player controller 책임
- gameplay HUD와 aim 중 표시되는 shared crosshair는 pawn/BP controller가 아니라 owning local `APragmataPlayerController` 책임
- Hugh aim/run/jump hold/hover 표시 상태는 `HughAnimState.bIs*`가 원본
- `UHughAnimInstance`의 locomotion blend `MovementSpeed`/`Acceleration`은 실제 `CharacterMovement` velocity/acceleration을 읽음
- 이유: 입력 intent 기반 speed를 쓰면 키를 뗀 순간 AnimBP는 idle인데 capsule은 braking으로 계속 움직여 발 미끄러짐 발생
- Hugh owner는 local prediction으로 `HughAnimState`를 직접 갱신하고, 서버 복제는 `COND_SkipOwner`로 owner를 건너뜀
- `UPragmataHughMovementComponent::WantsTo*`는 movement prediction intent 전용이며, 표시 상태 저장소로 쓰지 않음

동작 흐름:

```text
AssignedRole
-> role pawn possess
-> PlayerState.ActiveControlRole
-> PlayerController local role mirror
-> Enhanced Input mapping
-> role별 camera/input 처리
```

핵심 위치:

- `APragmataPlayerController`
- `AHughCharacter`
- `UPragmataPlayerStatsComponent`
- `ADianaOperatorPawn`
- `APragmataPlayerState`

수정할 때 주의:

- Diana look은 controller rotation이 아니라 operator pawn local rotation 중심
- Diana yaw는 Hugh forward 기준 제한
- Diana 조준 프레이밍이 사라지면 arm length보다 먼저 Hugh capsule/main mesh의 `Camera` collision response를 확인
- Hugh movement input과 Diana camera/input을 한 pawn에 합치지 않음
- input asset 누락 fallback은 개발 안전장치이며 기본 구조가 아님
- legacy Hugh controller Blueprint asset과 native `AHughController` class는 삭제 완료
- HUD 생성 경로를 Blueprint controller EventGraph에 되살리지 않음
- Hugh에 raw `IsAiming/IsRunning/IsJumpInput/IsHovering` 캐시를 다시 만들지 않음
- AnimBP의 aim/run/hover 상태 getter는 `HughAnimState`를 읽고, locomotion blend speed는 실제 movement 값을 유지
- remote proxy 부작용은 `OnRep_HughAnimState()`에서만 재구성

HUD 책임:

- 목표: root gameplay HUD와 `WBP_Crosshair` 생성/viewport 추가를 local `APragmataPlayerController` 기준으로 한 번만 처리
- 역할별 root HUD asset: `/Game/UI/Widget/WBP_HughHUD`, `/Game/UI/Widget/WBP_DianaHUD`
- 공통 표시 위젯: `WBP_EnemyHackingWidget`, `WBP_MapHackingWidget`은 역할별 root HUD 안에 배치
- 목표 알림은 역할별 root HUD 안에 `WBP_ObjectiveAlertWidget` child를 배치하고, `UPragmataHUDWidget::ShowObjectiveAlert()`가 텍스트와 표시 시간만 전달한다.
- 대사 UI는 역할별 root HUD 안에 `WBP_DialogueWidget` child를 배치하고, `UPragmataHUDWidget::PlayDialogueLine()`이 한 줄 텍스트와 표시 시간만 전달한다.
- 이유: Demo gameplay `PlayerControllerClass`는 `/Script/Pragmata_2P.PragmataPlayerController`; Hugh/Diana 모두 같은 controller path 사용
- C++ 위치: `APragmataPlayerController::EnsureGameplayHUD`, `APragmataPlayerController::EnsureHughCrosshair`
- class 선택 위치: `APragmataPlayerController::TryGetGameplayHUDClassForAssignedRole`
- widget class source: `BP_HughCharacter`/`BP_DianaOperatorPawn` defaults의 `GameplayHUDClass`
- C++ 기본값: `/Game/UI/Widget/WBP_HughHUD.WBP_HughHUD_C`, `/Game/UI/Widget/WBP_DianaHUD.WBP_DianaHUD_C`
- 게임 로직은 새 목표 생성 시 owning player용 `APragmataPlayerController::Client_ShowObjectiveAlert()`, 양쪽 공통 알림용 `APragmataGameState::Multicast_ShowObjectiveAlert()`, 또는 캐릭터에 직접 둔 `ObjectiveAlertText` + `ShowObjectiveAlert()` 중 하나를 호출한다.
- 게임 로직은 대사 표시 시 특정 client용 `APragmataPlayerController::Client_PlayDialogueLine()` 또는 양쪽 공통 표시용 `APragmataGameState::Multicast_PlayDialogueLine()`을 호출한다. 대사 순서/트리거는 UI가 소유하지 않는다.
- Diana action voice는 `UDianaVoiceComponent`가 server에서 line을 선택하고 `APragmataGameState::Multicast_PlayDialogueLine()`으로 같은 subtitle을 전달한다. UI는 audio random, cooldown, queue를 소유하지 않는다.
- `/Game/Sound/DianaVoice/DA_DianaVoiceLineSet`은 action별 SoundWave pool과 cooldown의 단일 source다. `HackAim=10초`, `Cleanse=0초`, `OverDriveFire=0초`이며, 현재 voice 재생 중 새 request는 audio·subtitle 모두 억제한다.
- Cleanse voice는 skill 발사 시점이 아니라 server가 유효성을 확인한 precision hold 시작 또는 quick IA 입력 시점에 요청한다. release/timer 발사는 voice를 다시 요청하지 않는다.
- Diana voice source WAV는 15 category 30개만 사용한다. Caution의 `Dead_filament`, `cleanse skill upgrade`, `deletion skill`은 보류 또는 미사용이라 import·pool·trigger에 넣지 않는다.
- 최종 구조: root HUD 생성은 C++로 일원화. BP EventGraph에 별도 `Create Widget`이 남으면 HUD와 ViewModel binding이 중복됨
- crosshair class: `TSoftClassPtr<UUserWidget>`로 `/Game/UI/Widget/WBP_Crosshair.WBP_Crosshair_C` 참조
- 호출 위치: `BeginPlay`, `AcknowledgePossession`, `OnRep_PlayerState`
- 공유 stat source: local pawn이 아니라 `APragmataGameState`의 replicated `HughCharacter`/`DianaOperatorPawn` pointer 기준
- 공유 stat 바인딩: `APragmataPlayerController::RebindSharedHUDStats()` -> `UPragmataHUDWidget::BindSharedHUDStats()` -> `UPragmataHUDViewModel::BindSharedHUDStats()`
- 현재 공유 stat: Hugh `HealthComponent`, Hugh replicated stamina snapshot, Hugh `PlayerStatsComponent`, Diana `OverDriveComponent`
- Hugh weapon wheel ammo 바인딩: `UPragmataHUDWidget::BindSharedHUDStats()` -> `UWeaponWheelWidget::BindWeaponSource()` -> `AWeaponBase::OnAmmoChange`
- `/Game/UI/Widget/WBP_HughHUD` 안 weapon wheel child 이름은 `WBP_WeaponWheel` 유지. C++는 `ResolveWeaponWheelRoot()`에서 `GetWidgetFromName("WBP_WeaponWheel")` fallback으로 연결
- Hugh `PlayerStatsComponent`: `Lunafilament`, `PlayerDamageMultiplier`, `RepairCartridge`, `MaxRepairCartridge`, `RepairCartridgeRecoveryAmount`
- `Lunafilament`는 Hugh `PlayerStatsComponent`의 서버 authoritative 총액이 원본. `OnLunafilamentChanged(NewTotal)`는 실제 총액, `OnLunafilamentGained(Delta, OldTotal, NewTotal)`는 획득 연출용
- UI는 `UPragmataHUDViewModel::OnLunafilamentGainedForUI`를 구독해 1초 merge window 동안 delta를 합산 표시. `UPragmataHUDViewModel::Lunafilament`는 초기화/소비/강제 보정용이며 animated count에 직접 바인딩하지 않음
- Hugh `PlayerStatsComponent`: `Lunafilament`, `PlayerDamageMultiplier`, `RepairCartridge`, `MaxRepairCartridge`, `RepairCartridgeRecoveryAmount`, `SuitUpgradeLevel`, `WeaponUpgradeLevel`
- `Lunafilament` gameplay 값은 `UPragmataPlayerStatsComponent::AddLunafilament()`에서 즉시 서버 확정/복제하고, 획득 연출 지연은 `ULunaFilamentWidget` 표시 계층만 담당
- `ULunaFilamentWidget`은 새 총액이 현재 표시값보다 크면 `GainAmountText`에 `+획득량`을 띄우고 `GainPopupAnimation` 종료 후 `AmountText`를 새 총액으로 갱신
- `UOverDriveGaugeWidget` 안 정화 UI는 선택적 `PurificationPanel`, `PurificationDot`, `CoolDownPanel`, `CoolDownImage`로 표시. 게임 로직은 정화 스킬 획득 후 `SetPurificationPanelVisible(true)`, 사용 가능 여부 변화 때 `SetPurificationSkillAvailable(bool)`, cooldown 중 0~1 값을 `SetPurificationCooldownPercent()`로 전달
- `PurificationPanel`은 스킬 획득 전 `Collapsed`로 숨김. 내부 `PurificationDot`/`CoolDownPanel` 교체는 `Hidden`으로 숨겨 `RELOAD` TextBlock과 material image의 layout 재계산 튐을 줄임
- 정화 cooldown material은 `Percent=0` 비어 있음, `Percent=1` 가득 참 기준. UMG material의 UV Y 기준으로 `UV.y >= 1 - Percent` 영역만 보이게 만들면 원 내부가 아래에서 위로 차오름
- 강화 UI source: HUD와 독립된 `WBP_UpgradeMenuWidget`을 `UPragmataUpgradeMenuWidget` 기반으로 만들면 위젯이 `NativeConstruct()`에서 `APragmataGameState::GetHughCharacter()`를 직접 구독
- 강화 입력: WBP가 커서/hold UI를 처리하고, hold 완료 시 `UPragmataUpgradeMenuWidget::RequestSelectedUpgrade()` 또는 `RequestSelectedRefund()` 호출
- 강화 RPC: `APragmataPlayerController::RequestUpgrade()`/`RequestRefundUpgrade()` -> server RPC -> `APragmataGameState::GetHughCharacter()` -> Hugh `PlayerStatsComponent`
- 독립 생성 주의: GameState의 Hugh pointer가 늦게 복제되면 `OnHughCharacterChanged`에서 자동 재바인딩. 위젯을 다시 열 때는 `RefreshSharedUpgradeStats()` 호출 가능
- 강화 타입: `Suit`, `Weapon`만 활성. `Hacking`은 enum에 있지만 현재 비용/레벨 0, 강화 불가
- 강화 비용: 단계당 `LunafilamentCostPerUpgrade=100`, 환급 시 같은 금액을 `AddLunafilament()`로 반환
- 슈트 강화: `SuitUpgradeLevel` 1단계마다 `MaxHPPerSuitUpgrade`만큼 Hugh `HealthComponent::SetMaxHPFromUpgrade()` 적용
- 무기 강화: `WeaponUpgradeLevel` 1단계마다 `DamageMultiplierPerWeaponUpgrade`를 더해 `PlayerDamageMultiplier = 1 + level * step`
- 강화 preview는 `PlayerStatsComponent`의 `MaxHPPerSuitUpgrade`, `DamageMultiplierPerWeaponUpgrade`를 읽어 Blueprint tuning과 같은 값을 표시. GameState 재바인딩 전에는 selection/hold/Hugh delegate를 모두 해제
- 사망 리스폰은 `APragmataGameMode::ExecuteTeamRespawn()`만 death 해제를 소유. checkpoint 보급의 HP delegate는 사망 대기 timer를 앞당기지 않으며, callback에서 death state reset 뒤 resource refill 실행
- 리스폰 callback은 checkpoint session 유무와 무관하게 Diana action lock을 해제하고 Hugh/Diana 위치·회전 및 Diana control rotation을 마지막 checkpoint `RespawnAnchor`로 복구
- `RepairCartridge` 사용은 `ConsumeRepairCartridge()`가 서버 기본 회복량으로 Hugh `HealthComponent::RecoverHP()`를 호출하고, 실제 회복 성공 시에만 1개 차감
- Hugh heal HUD source: `AHughCharacter::OnHealAvailabilityChanged`와 `OnHealHoldChanged`가 `UPragmataHUDViewModel`의 `RecoveryItemCount`, `RecoveryHoldProgress`, `bRecoveryHolding`, `bRecoveryProhibited`, `bCanUseHeal`을 갱신
- `bRecoveryProhibited`는 `!bCanUseHeal` 표시 alias이고, UI가 직접 heal 가능 상태를 봐야 하면 `UPragmataHUDViewModel::CanUseHeal()`을 사용
- Hugh 기본 HP는 `AHughCharacter`가 자기 `HealthComponent`에만 1500으로 지정하며, `UPragmataHealthComponent` 전역 기본값은 enemy 영향 때문에 유지
- `PlayerDamageMultiplier`는 Hugh weapon damage에만 좁게 적용. melee/player damage source 확장 시 같은 component 값을 사용
- 조건: `IsLocalController()` true, `AssignedRole`과 possessed pawn이 Hugh/Diana로 일치, 기존 widget instance 없음, widget class load 성공
- 저장: `UPROPERTY(Transient) TObjectPtr<UPragmataHUDWidget> GameplayHUD`
- crosshair 저장: `UPROPERTY(Transient) TObjectPtr<UUserWidget> HughCrosshairWidget`
- crosshair 표시: 생성 직후 `Hidden`, 각 local player의 aim callback에서 `Visible`/`Hidden` 토글
- role pawn pointer가 HUD 생성 뒤 늦게 복제되면 GameState delegate가 shared stat을 다시 바인딩
- 실패 처리: missing class 또는 create 실패 시 `UE_LOG` warning/error
- 삭제 완료: legacy Hugh controller Blueprint asset 제거
- 중복 방지: pawn BP EventGraph가 root HUD를 직접 생성하지 않는지 확인

검증 방법:

- role별 possess actor 확인
- Hugh movement/aim/run/hover 입력 확인
- Diana look/scan/hack 입력 확인
- remote 화면에서 Hugh aim/hover/run animation 확인
- Standalone Demo에서 Hugh root HUD가 `WBP_HughHUD`인지 확인
- Standalone Demo 또는 2-player PIE에서 Diana root HUD가 `WBP_DianaHUD`인지 확인
- Hugh/Diana root HUD 안 child 이름이 `WBP_DialogueWidget`이고 class가 `UDialogueWidget` 기반인지 확인
- Standalone Demo에서 Hugh crosshair가 시작 시 숨겨지고 aim 중에만 표시되는지 확인
- 2-player listen server Demo에서 host/client 각각 root HUD 1개만 생성 확인
- Hugh/Diana aim 시작/종료 후 각자 화면의 crosshair만 표시/숨김 확인
- `WBP_EnemyHackingWidget`, `WBP_MapHackingWidget`이 양쪽 root HUD에서 같은 replicated target/component에 bind/unbind되는지 확인
- Hugh damage 후 Hugh/Diana HUD의 HP bar/text 동시 갱신 확인
- Hugh `Lunafilament` 증가 시 `+획득량` 표시가 먼저 뜨고, 팝업 종료 후 총액이 새 값으로 반영되는지 확인
- 강화 UI에서 루나필라멘트가 100 미만이면 비용 텍스트를 빨간색으로 표시하고 강화 hold 완료가 실패하는지 확인
- 슈트 강화 5단계에서 강화 버튼이 숨고 환급만 표시되는지 확인
- 무기 강화 후 weapon damage가 `BaseDamage * PlayerDamageMultiplier`로 증가하는지 확인
- 강화 환급 시 레벨이 1 감소하고 루나필라멘트 100이 반환되는지 확인
- Hugh `RepairCartridge` 증감 후 Hugh/Diana HUD의 recovery item count 동시 갱신 확인
- Hugh `Lunafilament` 증가 후 Hugh/Diana HUD ViewModel의 총액과 `OnLunafilamentGainedForUI`가 동시에 갱신되는지 확인
- 1초 안에 Lunafilament가 여러 번 증가하면 UI가 `+획득량`을 합산 표시하고, animation/timer 종료 후 latest `NewTotal`로 표시 총액을 반영하는지 확인
- Hugh 사격/재장전 후 weapon wheel `AmmoText`가 `current / max`로 즉시 갱신되는지 확인
- `WBP_OverDriveGauge`의 `PurificationPanel`은 정화 스킬 획득 전 숨김, 획득 후 표시. 사용 가능 상태에서는 `PurificationDot`만 보이고, 사용 불가 상태에서는 `CoolDownPanel` 안 `CoolDownImage` material이 `Percent=0/0.5/1`에서 아래부터 위로 차오르는지 확인. 전환 순간 `RELOAD` TextBlock 위치가 튀면 dot/cooldown 상호 숨김이 `Hidden`인지 확인
- damaged Hugh가 `ConsumeRepairCartridge()`로 300 HP 회복하고, full HP에서는 cartridge를 소모하지 않는지 확인
- `PlayerDamageMultiplier`가 Hugh weapon damage에 반영되고 음수 값은 0으로 clamp되는지 확인
- Hugh dash/jump/hover stamina 변화와 locked 색상이 Hugh/Diana HUD에 동시 반영되는지 확인
- Diana enemy hack 완료 후 OverDrive gauge가 Hugh/Diana HUD에 동시 반영되는지 확인
- Host Diana / Client Hugh에서도 client Hugh stamina 변화가 host Diana HUD에 반영되는지 확인
- legacy controller Blueprint asset 삭제 후 native HUD와 이중 생성 없음 확인

관련 문서:

- `Docs/Systems/Server/server.md`의 `Hugh Replication`, `Diana Replication`
- `Docs/Architecture/replication-role-animation-fix.md`

### Boss HP HUD

현재 구조:

- boss HP는 보스 머리 위 `WidgetComponent`가 아니라 역할별 root HUD 안의 고정 child widget
- root HUD asset: `/Game/UI/Widget/WBP_HughHUD`, `/Game/UI/Widget/WBP_DianaHUD`
- C++ widget class: `UBossHPWidget`
- boss HP ViewModel: `UBossHPViewModel`
- GameState source: `APragmataGameState::ActiveBossTarget`
- 서버 호출: `APragmataGameState::SetActiveBossTarget(BossActor)`
- HUD 전달: `APragmataPlayerController::HandleActiveBossTargetChanged()` -> `UPragmataHUDWidget::HandleActiveBossTargetChanged()`
- health source: boss actor의 `UPragmataHealthComponent`
- 표시 갱신: `UBossHPViewModel`이 `UPragmataHealthComponent::OnHealthChanged`, `OnDeath`를 구독하고 `UBossHPWidget`은 ViewModel UI event만 구독
- groggy source는 아직 확정하지 않았다. 현재는 `UBossHPViewModel::SetBossGroggyValue()` 또는 `SetBossGroggyPercent()`로 표시값을 넣는다.

WBP 규칙:

- `WBP_BossHPWidget`를 `UBossHPWidget` parent class로 생성
- `WBP_BossHPWidget` View Models panel에 `BossHPViewModel`을 추가하고 이름을 정확히 `BossHPViewModel`, Creation Type을 `Create Instance`로 설정
- 내부 HP bar child는 `HPBar` 또는 `BossHPBar` 이름 사용
- HP bar child는 기존 `UEnemyHPBarWidget` 기반 WBP 재사용 가능
- 내부 groggy gauge child는 `BossGroggyBar` 이름 사용
- groggy gauge child는 기존 `UEnemyGroggyGaugeWidget` 기반 WBP 재사용 가능
- 선택 텍스트 이름: `BossNameText`, `BossHPText`
- `WBP_HughHUD`와 `WBP_DianaHUD` 안에 child widget 이름을 `WBP_BossHPWidget`으로 배치
- 위치와 크기는 WBP Canvas/Overlay anchor에서 설정하고 C++에서 강제하지 않음

검증:

- 2-player PIE listen server에서 서버가 `SetActiveBossTarget(BossActor)` 호출
- Hugh/Diana 양쪽 HUD 상단 boss HP 표시 확인
- boss damage 후 양쪽 boss HP bar 동시 감소 확인
- `BossHPViewModel.SetBossGroggyValue()` 또는 `SetBossGroggyPercent()` 호출 후 `BossGroggyBar` material 갱신 확인
- boss death 또는 `SetActiveBossTarget(nullptr)` 후 양쪽 표시 숨김 확인

## Hugh Weapon Debug Crash

문제:

- `AHughCharacter::PrintDebugMsg()`가 `Tick()`마다 실행
- debug ammo/cooldown 출력에서 `CurrentWeapon` 계열 getter를 null guard 없이 호출
- `EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000488` 또는 `0x0000000000000490`

원인:

- `CurrentWeapon`은 `PostInitializeComponents()`에서 server authority가 `DefaultWeaponClass`를 spawn해야 세팅
- client는 weapon actor pointer가 replicate되기 전까지 `CurrentWeapon == nullptr`
- `DefaultWeaponClass`가 비었거나 `SpawnActor`가 실패해도 `CurrentWeapon == nullptr`
- crash address `0x488`/`0x490`은 null object에서 `AWeaponBase` 멤버 오프셋을 읽으려 한 형태
- 실제 crash 지점은 `PrintDebugMsg()`의 debug 출력이며, 사격 로직 자체는 이미 `!CurrentWeapon` guard 보유
- 같은 null 구간에서 조준 시작이나 `SetAimingState()`가 weapon type을 읽어도 입력성 crash 가능

현재 해법:

- `PrintDebugMsg()`의 ammo/cooldown 출력은 같은 `CurrentWeapon` 유효성 확인 후 `GetCurAmmo()`/`GetIsCoolingDown()` 호출
- weapon이 아직 없으면 `Ammo: None`, `IsCoolingDown: None` 출력
- 조준 시작과 로봇팔 상태 반영은 `CurrentWeapon`이 있을 때만 weapon type 조회
- debug HUD는 gameplay state를 확정하지 않고 관찰만 해야 함

수정할 때 주의:

- debug 출력, HUD 표시, ViewModel bind 같은 표시 계층도 replicated actor pointer를 바로 역참조하지 않음
- server spawn 직후와 client `OnRep_CurrentWeapon()` 사이에는 정상적으로 null 구간 존재
- `DefaultWeaponClass` assignment 누락은 별도 Editor 설정 문제이므로 crash 대신 `None` 표시와 warning으로 추적
- `CurrentWeapon` 기반 새 기능을 추가할 때는 `OnRep_CurrentWeapon()`, `EquipWeapon()`, client 초기 tick 순서를 함께 확인

검증 방법:

- Standalone에서 Hugh spawn 후 debug HUD에 `Ammo`/`IsCoolingDown` 또는 `None` 표시 확인
- 2-player PIE listen server에서 client 첫 tick 중 crash 없음 확인
- `BP_HughCharacter` defaults의 `DefaultWeaponClass`가 비어 있을 때도 `PrintDebugMsg()`가 crash하지 않음 확인
- weapon replication 도착 후 ammo 값이 정상 표시되는지 확인

## Gameplay Class Boundaries and Enemy Targets

문제:

- AI 세션 기록에 `CharacterBase`, `PlayerCharacter`, `EnemyCharacter` 같은 과거/추정 이름이 섞여 있음
- Walker만 scan/hack 대상이라고 보면 Executor leaf와 공통 monster interface 경계를 놓치기 쉬움
- Diana operator pawn을 일반 playable character로 합치면 role possession 구조가 다시 흐려짐

현재 해법:

- `APlayableCharacter`는 Hugh처럼 physical playable body가 필요한 class의 공통 기반
- `AHughCharacter`는 몸, camera, movement, Diana avatar attachment, paired hit react의 중심
- `ADianaOperatorPawn`은 Diana player가 possess하는 camera/input/scan/hack operator pawn
- `AMonsterBase`는 damage, hack, scan interface와 공통 AI/health/team 상태를 소유
- enemy leaf는 현재 `AWalkerMonster`와 `AExecutorMonster`가 있고, 각 leaf가 scan 표시 ID를 정함

동작 흐름:

```text
AssignedRole
-> Hugh면 APlayableCharacter 계열 AHughCharacter possess
-> Diana면 ADianaOperatorPawn possess
-> Diana scan/hack trace
-> IPragmataScannable / IPragmataHackable
-> AMonsterBase common validation/state
-> Walker 또는 Executor leaf display/id
```

핵심 위치:

- `APlayableCharacter`
- `AHughCharacter`
- `ADianaOperatorPawn`
- `AMonsterBase`
- `AWalkerMonster`
- `AExecutorMonster`
- `IPragmataDamageable`, `IPragmataHackable`, `IPragmataScannable`

수정할 때 주의:

- 새 코드나 문서에 `CharacterBase`, `PlayerCharacter`, `EnemyCharacter` 전제를 되살리지 않음
- Diana를 walking `ACharacter`로 합치지 않고 operator pawn 경계를 유지
- scan/hack 대상 추가 시 `AMonsterBase` interface contract와 leaf display/id를 같이 확인
- `AExecutorMonster`의 constructor asset load pattern은 현재 구현 사실일 뿐 새 enemy leaf의 기본 패턴으로 복사하지 않음
- enemy leaf를 추가하면 `Docs/Architecture/architecture.md` class tree와 `Docs/Operations/blueprint-tasks.md` asset 연결 항목도 함께 확인

검증 방법:

- Walker와 Executor가 scan UI에서 각 display text로 표시되는지 확인
- hack 성공 시 `AMonsterBase`의 hack/damage state와 OverDrive 증가가 서버에서 처리되는지 확인
- 새 enemy leaf 추가 시 interface 구현, AI controller/BT, mesh/AnimBP assignment 경로를 분리 검증
- 오래된 `.omx` 제안이 현재 source class 이름과 맞는지 `rg`로 재확인

관련 문서:

- `Docs/Systems/Damage/damage-flow.md`
- `Docs/Architecture/architecture.md`의 `Damage Flow`
- `Docs/Architecture/architecture.md`의 player/monster class 구조
- `Docs/Operations/blueprint-tasks.md`의 enemy/asset 연결 항목

## Enemy HP and Groggy UI

문제:

- Enemy HP와 groggy gauge는 같은 위치에 보이지만 서로 다른 WBP와 material 상태를 가져야 함
- C++이 layout을 만들기 시작하면 WBP visual 작업과 충돌함
- material scalar parameter 이름이 C++에 강하게 고정되면 material 수정 때 WBP 연결이 깨지기 쉬움

현재 해법:

- `WBP_EnemyHPBar` parent class는 `UEnemyHPBarWidget`
- groggy 전용 WBP parent class는 `UEnemyGroggyGaugeWidget`
- 두 WBP를 묶는 상위 enemy HP WBP parent class는 `UEnemyHPWidget`
- enemy 머리 위 표시는 `UEnemyHPWidgetComponent`를 enemy BP에 추가해 처리
- C++은 `Percent` scalar parameter만 갱신하고, bar 크기, segment 모양, 색, 배치는 WBP/material이 소유
- `UEnemyHPWidget`은 `UPragmataHealthComponent`의 `OnHealthChanged`, `OnDeath` delegate에 bind해 HP bar를 갱신
- `UEnemyHPWidgetComponent`는 owner actor의 `UPragmataHealthComponent`를 찾아 `UEnemyHPWidget`에 자동 bind
- `AMonsterBase::TargetActor`가 설정되면 모든 peer에서 머리 위 HP bar를 표시하고, Target 해제 시 숨김
- Blackboard는 서버 전용이므로 `TargetActor`의 `OnRep_TargetActor`가 복제된 target 상태를 widget component에 반영
- `ASentinelMonster`는 같은 Target 설정 시 `ActiveBossTarget`에도 자신을 등록해 HUD의 `UBossHPWidget`을 표시하고, Target 해제·사망 시 해제
- groggy는 아직 공통 component가 없으므로 `SetGroggyValue(Current, Max)` 또는 `SetGroggyPercent(Percent)`를 외부에서 호출

동작 흐름:

```text
Enemy actor
-> UEnemyHPWidgetComponent
-> owner UPragmataHealthComponent
-> UEnemyHPWidget::BindToHealth
-> UEnemyHPBarWidget::SetHealthValue
-> HP material Percent

Monster AI target assigned
-> AMonsterBase::TargetActor replication
-> OnRep_TargetActor
-> UEnemyHPWidgetComponent::SetOwnerTargetedVisible
-> UEnemyHPWidget::SetEnemyHPVisible

Groggy source
-> UEnemyHPWidget::SetGroggyValue
-> UEnemyGroggyGaugeWidget::SetGroggyValue
-> groggy material Percent
```

접속 위치:

- `UEnemyHPWidget`
- `UEnemyHPBarWidget`
- `UEnemyGroggyGaugeWidget`
- `UEnemyHPWidgetComponent`
- `/Game/UI/Widget/Enemy/WBP_EnemyHPBar`
- `/Game/UI/Widget/Enemy/WBP_EnemyHPWidget` 또는 새 `WBP_EnemyHP`
- `/Game/UI/Material/Enemy/M_EnemyHP_Segmented`
- `/Game/UI/Material/Enemy/M_EnemyGroggy_Segmented`

수정 주의:

- `WBP_EnemyHPBar` 내부 Image 이름은 `HPBar`
- groggy WBP 내부 Image 이름은 `GroggyBar` 또는 `GroggyGauge`
- 상위 WBP의 child widget 이름은 HP 쪽 `HPBar` 또는 `EnemyHPBar`, groggy 쪽 `GroggyBar` 또는 `GroggyGauge`
- material parameter 이름을 바꾸면 각 WBP Details의 `PercentParameterName`도 같이 수정
- C++에서 size, padding, anchor를 강제하지 않음
- runtime에서는 target이 없으면 머리 위 HP bar를 숨김. `UEnemyHPWidgetComponent`는 widget 생성 전 target 상태도 보관하므로 BeginPlay/OnRep 순서와 무관하게 같은 상태를 적용
- enemy 머리 위 위치, socket, relative offset은 enemy BP의 `UEnemyHPWidgetComponent`에서 조정
- `UEnemyHPWidgetComponent`의 Widget Class는 상위 enemy HP WBP로 지정
- 죽은 적 표시 유지가 필요하면 상위 WBP의 `bHideWhenDead`를 false로 설정
- `UEnemyHPWidgetComponent`(및 다른 Screen space `UWidgetComponent` 서브클래스)에서 `PrimaryComponentTick.bCanEverTick`을 false로 끄지 않음: Screen space는 `UWidgetComponent::TickComponent()` -> `UpdateWidgetOnScreen()` -> `AddWidgetToScreen()` 경로로만 game layer에 등록되고 `OnRegister()`/`InitWidget()`은 game world에서 이 경로를 타지 않으므로, tick을 끄면 Widget object는 생성되지만 화면에는 영원히 추가되지 않음 (`ADamageText`의 plain `WidgetComponent`가 tick 기본값 유지로 정상 렌더링되는 대조군)

검증 방법:

- WBP parent class 재지정 후 Compile 성공 확인
- enemy BP에 `UEnemyHPWidgetComponent` 추가 후 Widget Class가 상위 enemy HP WBP인지 확인
- PIE에서 target이 없는 enemy의 머리 위 HP bar가 숨겨지는지 확인
- PIE에서 perception 또는 Territory target 할당 직후 enemy 머리 위 HP bar가 표시되는지 확인
- PIE에서 Territory first-entry sequence monster는 sequence 종료 후 target이 설정될 때만 HP bar가 표시되는지 확인
- PIE에서 target 해제와 enemy 사망 직후 HP bar가 숨겨지는지 확인
- PIE에서 Sentinel은 target 설정 시 `BossHPWidget`이 표시되고, target 해제·사망 시 숨겨지는지 확인
- PIE에서 enemy가 damage를 받을 때 머리 위 HP bar가 감소하는지 확인
- groggy 테스트 입력으로 `SetGroggyPercent(0.0/0.5/1.0)` 호출 후 주황 bar material이 갱신되는지 확인
- `Percent` parameter 누락 시 bar가 움직이지 않으므로 material parameter spelling 확인

관련 문서:

- `Docs/Systems/Hacking/enemy-hacking-integration.md`
- `Docs/Operations/blueprint-tasks.md`

## Monster Territory Entry and ReturnHome

### Territory Combat Lock

- `bLockMapUntilMonstersCleared`를 켠 Territory는 첫 전투 진입 직후 `CombatLockTargets`의 `TerritoryCombatLockable` 구현 actor에 `SetTerritoryCombatLocked(true)`를 호출
- Sequence 지연과 Runtime Spawn이 끝난 뒤 `ActiveMonsters`의 생존 상태를 확인하므로, 연출 대기 중 monster가 아직 없다고 잠금이 먼저 풀리지 않음
- `UPragmataHealthComponent::OnDeath`에서 즉시 해제를 확인하고 `OnDestroyed`에서도 다시 확인해 death animation lifespan과 관계없이 마지막 monster 처치 시 봉쇄 해제
- `ADoorBase`, `AElevator`은 `OnTerritoryCombatLockStateChanged` Blueprint event에서 잠금 메시·차단벽·머티리얼을 표시. C++은 시각 컴포넌트를 생성하지 않음
- `ABossRouteElevator`는 기존 Stage 이동과 puzzle 재활성화를 유지하며, `RequiredCombatActors` 감시는 시작하지 않고 Territory의 `false` 호출에서 `NotifyCombatCleared()`로 다음 Stage 진행

문제:

- 공격 종료 후 cooldown 중 monster가 Territory 밖으로 나가면 `ReturnHome`과 `NoAttack`이 경계에서 반복 전환
- 경계 overlap 즉시 `bInsideTerritory = true`가 되면 `ReturnHome`의 self abort와 `NoAttack` 이동이 같은 경계에서 반복
- player 첫 진입 때 map gimmick, runtime monster spawn, 등장 sequence를 한 번만 시작해야 함
- 일부 Territory는 player 진입만으로 전투를 시작하지 않고 map gimmick이 직접 승인한 뒤 작동해야 함
- 엘리베이터, 문, 퍼즐, 알람처럼 외부 map gimmick 완료 뒤 Territory 첫 전투 flow를 열어야 함

현재 해법:

- `AMonsterAIController::RunAI()`는 controller 생성자 hard-load가 아니라 monster `BTAsset`이 선언한 Blackboard를 schema source로 사용
- `BBKEY_TARGET`은 `AActor*` 계약으로 유지하고 BT service/decorator에서 `APawn*`으로 좁히지 않음
- `BT_Executor`의 `ReturnHome` Blackboard Decorator는 `bInsideTerritory Is Not Set` 유지
- `Observer Aborts = Both`를 유지해 안전한 재진입 후 Chase 중간 전환 허용
- Territory 이탈 monster를 `ReturningMonsters`에 기록하고 `bInsideTerritory = false` 즉시 반영
- 경계 overlap만으로 복귀 완료 처리하지 않고 `MonsterReentryMargin`만큼 안쪽 진입 후 true 반영
- monster 출입은 루트 Capsule overlap만 인정하고 머리·몸통·다리 collision event는 무시
- 배치 monster의 AIController가 Territory BeginPlay보다 늦게 생기면 pending 목록에 두고 server Tick에서 `SetTerritory` 재시도
- server가 첫 target faction의 player-controlled Pawn 진입을 `bFirstPlayerEntryTriggered`로 한 번만 확정
- `OnPlayerEnteredTerritory`는 player가 영역에 들어온 사실만 broadcast하고, 전투 시작 여부와 분리
- `bRequireMapGimmickActivation`이 켜진 Territory는 `ActivateTerritoryTrigger()` 호출 전까지 첫 진입 flow를 보류
- 기존 active monster는 player 재진입만으로 target을 재설정하지 않고 해당 actor perception cache를 지운 뒤 새 Sight 또는 Damage stimulus가 들어와야 target 획득
- `OnFirstPlayerEntered`는 활성화 조건까지 통과해 첫 전투 시작이 확정될 때 1회 broadcast
- `FirstEntrySequence`가 있으면 sequence 완료 뒤 `RuntimeSpawns`를 생성
- `FirstEntrySequence`가 없으면 첫 전투 시작 시 `RuntimeSpawns`를 즉시 생성
- spawned monster는 `FMonsterTerritorySpawnInfo.AppearanceMontageOverride`가 있으면 해당 몽타주를, 없으면 Monster BP의 `SpawnAppearanceMontage`를 재생
- runtime spawn 항목에서 `bUseFallingAppearance`를 켜면 `SpawnPoint`를 착지 지점으로 쓰고 `FallingSpawnZOffset`만큼 위에서 생성
- 하강 appearance는 montage root motion 이동을 무시하고 `CharacterMovement`의 `MOVE_Falling` 중력으로 실제 낙하
- `FallingEndTransitionDistance` 안쪽에서 바닥이 감지되거나 `Landed()`가 호출되면 `FallingAppearanceEndSectionName` section으로 전환
- 지연 시작 대상 배치 monster는 `PlacedMonsterOverrides`에 같은 monster와 `AppearanceMontageOverride`가 있으면 해당 몽타주를, 없으면 Monster BP의 `SpawnAppearanceMontage`를 재생
- appearance가 끝난 monster는 AI Perception을 기다리지 않고 현재 Territory 안의 player를 target으로 지정한 뒤 BT 시작
- 비하강 appearance는 종료 delegate가 누락되거나 section link로 끝나지 않아도 몽타주 길이 뒤 fallback으로 상태를 정리하고 BT 시작
- Territory가 Target을 지정한 뒤 `AMonsterAIController::RestartBehaviorAfterTargetAssigned()`로 BT 루트 조건을 즉시 재평가해 Chase 진입을 보장
- `FirstEntrySequence`가 지정된 경우 reliable multicast로 각 peer에서 local sequence player 생성·재생
- `FirstEntrySequenceDelay`가 0보다 크면 첫 진입 확정 뒤 해당 시간 동안 spawn, 배치 monster 시작, sequence 재생을 모두 보류
- delay가 끝난 뒤에도 순서는 `FirstEntrySequence` 재생이 먼저이고, sequence 완료 후 `RuntimeSpawns`와 배치 monster 시작이 진행
- runtime 생성 `FirstEntrySequence` player는 종료 시 `ForceKeepState`를 사용해 마지막 Transform key 상태를 유지
- `Use Territory As Sequence Transform Origin`은 absolute transform track을 Territory 위치·회전 기준으로 평가
- map gimmick BP는 Territory reference를 들고 `ActivateTerritoryTrigger()`를 직접 호출
- `APoweredResponBox`는 문 개방 조건을 통과한 server 시점과 실제 `OpenDoor()` 시점에 `TerritoriesToActivateOnOpen`의 Territory를 한 번 활성화
- `ADoorBase::ProximityTrigger`는 player overlap만 감지하고 weapon `CharacterBody` trace와 Watcher `Projectile` trace는 막지 않음
- `ActivateTerritoryTrigger()`는 이미 활성화된 Territory라도 첫 전투가 아직이면 `TriggerBox` overlap actor를 다시 찾아 `CurrentTarget`으로 사용
- 기존 `ExternalTriggers`/`TriggerExternalEvent` 경로는 제거하고 첫 전투 시작 게이트로 통합

동작 흐름:

```text
bInsideTerritory = false
-> ReturnHome이 전투 분기 중단
-> Move To HomePos
-> 경계 재진입은 ReturnHome 유지
-> MonsterReentryMargin 안쪽 진입
-> bInsideTerritory = true
-> ReturnHome self abort 후 Chase 재평가

첫 player 진입
-> OnPlayerEnteredTerritory server broadcast
-> bRequireMapGimmickActivation이 켜져 있고 아직 비활성 상태면 여기서 대기
-> ActivateTerritoryTrigger 호출 또는 기본 활성 상태 통과
-> OnFirstPlayerEntered server broadcast
-> FirstEntrySequence가 있으면 peer별 재생
-> sequence가 끝나거나 sequence가 없으면 RuntimeSpawns server 생성
-> FirstEntrySequenceMonsters에 들어간 배치 monster, bRequireMapGimmickActivation이 켜진 배치 monster, appearance 보유 배치 monster도 이 시점에 시작 대상 포함
-> spawn 배열 항목 또는 PlacedMonsterOverrides의 AppearanceMontageOverride, 없으면 monster별 SpawnAppearanceMontage가 있으면 재생
-> appearance 완료 또는 montage 없음
-> 비하강 appearance가 끝나지 않으면 montage 길이 뒤 fallback으로 appearance 정리
-> RunAI 후 현재 Territory player target 지정
-> 이후 재진입에서는 첫 진입 flow 생략
-> 기존 active monster는 기존 perception cache 삭제
-> 새 perception 또는 damage 감지 전까지 Target 없음 유지
```

핵심 위치:

- `Content/AI/BT_Executor.uasset`의 `ReturnHome` Decorator
- `AMonsterTerritoryActor::HandleBeginOverlap`, `HandleEndOverlap`, `UpdateReturningMonsters`
- `AMonsterTerritoryActor::ResolvePendingControllerTerritories`
- `AMonsterTerritoryActor::ActivateTerritoryTrigger`, `TriggerFirstPlayerEntry`, `StartFirstEntryGameplay`
- `AMonsterTerritoryActor::BeginMonsterEncounter`, `StartMonsterAIWithCurrentTarget`, `SpawnRuntimeMonsters`
- `AMonsterTerritoryActor::AssignCurrentTargetToMonster`, `CreateTerritorySequencePlayer`
- `AMonsterBase::SetTerritoryTarget`
- `AMonsterTerritoryActor::MonsterReentryMargin`
- `AMonsterBase::StartCommonAttackCooldown`, `RefreshAttackAvailability`
- `AMonsterBase::PlaySpawnAppearance`, `UpdateFallingSpawnAppearance`, `HandleSpawnAppearanceMontageEnded`

수정할 때 주의:

- `ReturnHome`의 `Both`는 안전한 안쪽 재진입 후 Chase 전환에 필요
- `MonsterReentryMargin`이 너무 작으면 경계 반복 가능, 너무 크면 Chase 전환 지연
- cooldown은 `NoAttack` 선택 조건이며 반복 전환의 직접 원인은 경계에서 즉시 뒤집히는 Territory 상태
- `FMonsterCombatData.bUseRandomCommonAttackCooldown`을 켜면 `CommonAttackCooldownMin`~`CommonAttackCooldownMax`에서 server가 공격 종료마다 새 cooldown을 뽑고, 끄면 기존 `CommonAttackCooldown` 고정값을 사용
- 추가 body collision은 벽·공격 판정에 사용해도 Territory의 `bInsideTerritory`를 변경하지 않음
- `OnPlayerEnteredTerritory`는 player 진입 알림용이고, 전투 시작 승인과 별개
- `OnFirstPlayerEntered`는 server에서만 broadcast하므로 map gimmick의 authoritative 상태를 listener에서 처리
- `bRequireMapGimmickActivation`이 꺼져도 `FirstEntrySequenceMonsters`에 들어갔거나 appearance montage/override가 있는 배치 monster는 BeginPlay/possess 뒤 BT를 멈추고 첫 진입 flow 뒤 시작
- `bRequireMapGimmickActivation`이 꺼지고 FirstEntrySequenceMonsters와 appearance가 모두 없는 배치 monster는 기존처럼 BeginPlay/possess 뒤 Patrol을 바로 시작
- `bRequireMapGimmickActivation`이 켜진 배치 monster는 `ActivateTerritoryTrigger()` 뒤 첫 전투 시작 시 appearance와 target 지정 흐름을 사용
- `bRequireMapGimmickActivation`이 꺼진 Territory도 문 열림 시점에 `ActivateTerritoryTrigger()`가 호출되면 현재 overlap player를 찾아 첫 전투 flow를 시작할 수 있음
- Player 진영 monster가 event를 오발하지 않도록 첫 진입 조건은 player-controlled Pawn으로 제한
- `RuntimeSpawns`는 더 이상 BeginPlay에서 생성하지 않으며 첫 player 진입 시 한 번만 생성
- first entry sequence player 생성에 실패하면 runtime spawn은 즉시 실행해 전투 진행을 보장
- 외부 map gimmick이 전투 시작을 늦출 때는 별도 event 배열이 아니라 `ActivateTerritoryTrigger()`만 사용
- runtime spawned monster target 지정은 `MonsterAIController::SetTarget`을 통과하므로 Territory 밖 player는 target으로 잡히지 않음
- 지연 시작 대상 배치 monster는 appearance가 있으면 재생 후 BT를 시작하고, appearance가 없으면 즉시 BT와 target 지정
- fallback은 비하강 appearance에만 적용하고, 하강 appearance는 착지 감지와 End section 전환 흐름을 유지
- 배치 monster는 Territory 재진입 BeginOverlap에서 기존 successful stimulus를 재사용하지 않고 해당 actor perception cache를 삭제
- runtime spawn appearance는 spawn 배열 항목의 `AppearanceMontageOverride`로 Territory별 override 가능
- `AppearanceMontageOverride`가 비어 있으면 Monster BP의 `SpawnAppearanceMontage`를 사용
- 배치 monster appearance는 `PlacedMonsterOverrides`에서 Territory별 override 가능
- 시퀀서에 쓰이는 배치 monster가 appearance 없이 sequence 종료 뒤 시작해야 하면 `FirstEntrySequenceMonsters`에 추가
- 기존 `PlacedMonsters`는 유지하고, appearance override가 필요한 monster만 `PlacedMonsterOverrides`에 추가
- 하강 appearance용 montage는 `Start -> Loop -> End` section 구조를 두고 `Loop -> Loop`는 유지하되 code가 착지 조건에서 `End`로 점프
- 하강 appearance에서 실제 Z 이동은 root motion이 아니라 capsule movement와 중력이 담당하므로 loop animation은 제자리 낙하 pose로 제작
- 재사용 sequence의 camera는 Spawnable과 Absolute transform track을 사용하고 Territory 기준 상대 좌표로 제작
- 기존 월드 좌표 sequence는 `Use Territory As Sequence Transform Origin`을 끄거나 transform key를 원점 기준으로 다시 작성
- Transform Origin은 Relative/Additive transform section과 다른 level의 actor binding을 바꾸지 않음
- appearance Level Sequence에서 root motion을 actor 이동으로 적용하려면 Skeletal Animation section의 `Swap Root Bone`을 `Actor`로 설정
- animation section이 370프레임인데 sequence playback end가 300프레임이면 마지막 root motion이 잘려 위치 보정이나 순간 이동처럼 보일 수 있음
- `LS_ExecutorAppear`는 `BP_Executor` animation section 끝과 sequence playback end를 370프레임으로 맞춤
- `FirstEntrySequence`는 map actor의 hard reference이므로 해당 map cook에 포함되고 별도 PAL 등록 불필요

검증 방법:

- 공격 직후 cooldown 상태에서 monster를 Territory 밖으로 유도
- 재진입 경계에서는 ReturnHome이 유지되는지 확인
- 설정한 안쪽 여유 거리 통과 후 HomePos 도착 전 Chase 전환 확인
- `ReturnHome`과 `NoAttack`의 프레임 단위 반복 전환이 사라지는지 확인
- 머리·몸통·다리 collision만 경계를 넘나들 때 Patrol이 abort되지 않는지 확인
- 루트 Capsule이 경계를 벗어날 때만 ReturnHome으로 전환되는지 확인
- cooldown 종료 후 정상 공격 전환 확인
- 첫 player 진입에서 `OnPlayerEnteredTerritory`가 broadcast되는지 확인
- `OnFirstPlayerEntered`는 전투 시작 확정 시 한 번만 broadcast되고 재진입에서 반복되지 않는지 확인
- `bRequireMapGimmickActivation`이 켜진 Territory는 player가 먼저 들어와도 `ActivateTerritoryTrigger()` 전에는 spawn/sequence/BT 시작이 없는지 확인
- `ActivateTerritoryTrigger()` 뒤 player가 안에 있거나 다시 진입하면 첫 전투 flow가 시작되는지 확인
- `PoweredResponBox` 문이 열린 순간 player가 Territory 안에 이미 있으면 appearance 보유 배치 monster가 appearance 뒤 RunBT로 넘어가는지 확인
- `BP_PoweredResponBox_*` 내부에서 GripGun을 쏴도 `ProximityTrigger`가 Hit Actor로 찍히지 않는지 확인
- `RuntimeSpawns`가 첫 진입 전에는 없고 진입 후 server/client에 생성되는지 확인
- `FirstEntrySequence`가 있으면 sequence 완료 뒤 runtime spawn이 한 번만 생성되는지 확인
- runtime spawned monster가 appearance 완료 또는 montage 없음 이후 바로 Target을 갖고 Chase로 전환되는지 확인
- `FirstEntrySequenceMonsters`에 들어간 배치 monster는 sequence 완료 뒤 Blackboard Target이 player로 지정되고 첫 BT 분기가 Patrol이 아니라 Chase로 재평가되는지 확인
- 같은 sequence를 위치·회전이 다른 두 Territory에서 재생해 camera 상대 구도가 유지되는지 확인
- root motion appearance sequence는 playback end와 animation section end가 일치하고 `Swap Root Bone=Actor`인지 확인
- appearance montage가 있는 monster만 이동·공격 잠금 후 정상 복구되는지 확인
- 비하강 appearance override 몽타주가 section link로 끝나지 않아도 몽타주 길이 이후 RunBT가 시작되는지 확인
- `bUseFallingAppearance`가 켜진 runtime spawn은 `SpawnPoint` 바닥 위치 위에서 생성되고 낙하 후 `End` section으로 넘어가는지 확인
- 하강 appearance 중 root motion이 켜진 montage라도 캐릭터 실제 위치가 animation root가 아니라 중력으로 내려오는지 확인
- 같은 monster BP를 서로 다른 Territory spawn 배열에 넣고 `AppearanceMontageOverride`를 다르게 지정했을 때 각 Territory별 몽타주가 재생되는지 확인
- 같은 배치 monster를 Territory의 `PlacedMonsterOverrides`에 넣고 `AppearanceMontageOverride`를 지정했을 때 Monster BP 기본값 대신 override 몽타주가 재생되는지 확인
- listen server와 client에서 `FirstEntrySequence`가 함께 재생되는지 확인
- FirstEntrySequenceMonsters와 appearance가 모두 없는 기본 배치 monster는 첫 진입만으로 기존 Patrol/perception 흐름이 깨지지 않는지 확인
- `FirstEntrySequenceMonsters`에 들어간 배치 monster는 appearance가 없어도 sequence 완료 뒤 Target이 지정되는지 확인
- appearance가 있는 기본 배치 monster는 게임 시작 직후 BT가 정지되고 첫 진입 appearance 완료 뒤 Target이 지정되는지 확인
- 활성화형 배치 monster는 appearance 완료 또는 montage 없음 이후 Target이 지정되는지 확인

## Damage Flow and Melee Hit Notify

문제:

- gameplay damage가 Unreal damage event와 project interface로 갈라지면 health 적용 경로가 헷갈림
- `Apply*` 이름은 caller/receiver 방향이 흐림
- melee 공격은 animation의 실제 타격 구간과 damage 판정이 맞아야 함
- Hugh 피격 반응은 실제 health 감소 후에만 실행되어야 하며 Diana lock은 큰 피격에서만 필요

현재 해법:

- caller 함수는 `Deal*`, receiver 함수는 `Receive*`
- type/class 이름은 `Pragmata` prefix 유지
- 공통 진입점: `UPragmataDamageLibrary::DealDamageToActor`
- `DealDamageToActor`는 target actor authority에서만 `ReceiveDamage`를 호출
- faction 판정 source는 `SourceActor` 우선, 없으면 `DamageCauser` fallback
- 양쪽 actor에 `UPragmataFactionComponent`가 있으면 같은 진영/neutral 조합은 공통 진입점에서 차단
- 공통 수신점: `IPragmataDamageable::ReceiveDamage`
- 최종 체력 변경: `UPragmataHealthComponent::ReceiveDamage`
- server `BeginPlay`에서 BP/C++로 지정된 `MaxHealth`를 `CurrentHealth`에 반영해 모든 actor가 최대 체력으로 시작
- Unreal `ApplyDamage`, `ApplyPointDamage`, `ApplyRadialDamage`는 gameplay damage 경로로 사용하지 않음
- `TakeDamage` bridge는 제거했고, source가 `FPragmataDamageSpec`을 직접 만든 뒤 `ReceiveDamage` 경로로 전달
- gun damage는 server fire trace 결과를 `FPragmataDamageSpec`으로 전달
- Watcher 원거리 공격은 `UBTTask_Watcher_AttackTarget`의 Projectile trace 결과를 `AWatcherMonster::FireBullet()`에 전달
- Projectile trace가 target pawn을 block하지 않아 miss가 나면 장애물이 없는 것으로 보고 target 위치 `FHitResult`를 만들어 `FireBullet()` 호출
- monster는 적용 성공한 `Weapon` 피해의 player pawn이 assigned Territory 안에 있을 때만 자기 `AIPerceptionComponent`에 `AISense_Damage` stimulus를 직접 등록
- 직접 등록 전 `UAIPerceptionSystem::RegisterSenseClass(UAISense_Damage)`와 `ConfigureSense(DamageConfig)`로 유효한 SenseID와 stimulus age 배열을 보장
- Territory 밖 player, non-Weapon 피해, 사망 처리된 monster는 damage stimulus를 보고하지 않음
- client gun 입력은 `Server_RequestFire()` intent 하나만 전송하고 cooldown, ammo, damage는 server transaction에서 함께 확정
- `AGripGun::PullTrigger()`는 non-authority 호출을 즉시 거부하고, 서버만 `CommitServerShot()`에서 ammo 소모·reload 전환·trace·damage를 확정
- `AHughCharacter::PostInitializeComponents()`는 기존 `DefaultWeaponClass` spawn 흐름을 유지하고, BP_HughCharacter에 `AttackWeaponClass`/`TacticalWeaponClass`가 할당된 경우에만 각각 `WeaponInventory[1]`, `WeaponInventory[2]`에 등록
- `IA_ChangeWeapon` 값 `1~4`는 기존 `OnStartWeaponChange()`에서 `EWeaponType` index `0~3`으로 변환
- `IA_CycleWeapon`은 mouse wheel 방향만 받아 `Primary -> Tactical -> Defense -> Attack` 순서로 순환하고, 반대 방향은 역순으로 순환한다
- `WeaponInventory`는 복제되고, client slot 선택은 `Server_RequestWeaponChange()`로 서버 `CurrentWeapon`까지 확정한다
- 비조준 상태에서 Attack/Tactical slot을 선택하면 `RefreshStoredWeaponPresentation()`이 최근 선택한 왼쪽 등 weapon만 보이고, Primary로 돌아가도 최근 Attack/Tactical weapon은 왼쪽 등에 남긴다
- 비어 있거나 ammo가 0인 non-primary slot은 선택 불가이며, 입력해도 현재 유효 `CurrentWeapon`과 UI 선택을 유지한다
- weapon wheel은 `WeaponInventory` 전체를 읽어 시작 직후 Attack/Tactical slot을 채우고, non-primary slot은 weapon actor와 ammo가 있을 때만 표시한다
- Attack/ShotGun과 Tactical/StasisNet은 `LeftBackWeaponSocketName`, Defense는 `RightBackWeaponSocketName`을 수납 기준으로 사용
- ShotGun은 spawn 직후 inventory index 계산 전에 `WeaponType == Attack`이어야 하므로 constructor와 `PostInitializeComponents()`에서 type을 보정한다
- Defense 오른쪽 등 draw/store montage가 비어 있으면 기존 왼쪽 등 montage로 fallback되므로 BP에서 오른쪽 montage를 할당해야 완전한 오른쪽 수납 연출이 나온다
- StasisNet은 Tactical index `2` 슬롯이며 `BP_HughCharacter`의 `TacticalWeaponClass`에 BP_StasisNet을 할당해야 시작 inventory에 들어간다
- `AStasisNet::PullTrigger()`는 같은 server fire gate를 쓰되 projectile 없이 HUD 중앙 reticle 지점을 먼저 찾고 `MuzzleSocketName`에서 그 지점으로 쏜 trace 명중 지점에 `AStasisNetField`를 생성
- `AStasisNet` 조준 preview는 local Hugh가 aim 중이고 Tactical/StasisNet을 장착했을 때만 표시해 미장착/비조준 상태 debug line 노출을 막음
- preview 기준 Hugh는 owner, instigator, attach parent 순서로 찾고, `BeginPlay()`에서 tick을 다시 켜 merge/BP default 차이로 debug preview가 꺼지는 상황을 줄임
- `MuzzleSocketName`은 socket 또는 existing bone 이름을 받을 수 있어 StasisNet Skeleton에 socket 추가가 막혀도 `muzzle` 계열 bone으로 시작점을 테스트 가능
- `AStasisNetField`는 10초 동안 debug sphere로 영역을 표시하고, overlap monster에 1초 간격 weapon dot damage와 최대 5초 stun 적용
- StasisNet dot damage는 weak point bone을 쓰지 않고 `AMonsterBase::IsHacked()` 기준으로 `BaseDamage` 또는 `HackedDamage` 선택
- Stasis stun은 `AMonsterBase::bStasisStunned`로 복제하며 source별 timer를 분리해 field 소멸 시 남은 stun을 즉시 해제
- `AWeaponBase::PullTrigger()`는 서버 권한과 ammo/cooldown gate를 통과한 경우에만 `true` 반환
- impact VFX는 서버 trace 결과를 `MulticastSpawnImpactEffect()`로 전달해 gameplay damage와 시각 효과 경계를 분리
- enemy damage text는 server가 health clamp 전 최종 계산 대미지와 위치를 정하고 `MulticastSpawnDamageText()`로 각 peer가 로컬 표시 actor를 생성
- `BeginServerReload()`는 replicated `bIsReloading`과 server timer를 시작
- `OnRep_IsReloading()`은 각 peer에서 reload montage presentation만 재생
- `FinishServerReload()`만 ammo 1발과 reload 종료를 확정하며 montage callback은 gameplay state를 바꾸지 않음
- 범용 public `RefillAmmo()` 경로는 두지 않고 weapon별 서버 reload helper가 ammo state를 소유
- `ReplenishAmmo()`와 auto replenish helper는 private 서버 helper로 유지하고, one-shot timer chain으로 max ammo까지 1발씩 예약
- `UWeaponDataAsset::GetDefaultCurrentAmmo()`는 UI 초기 획득값을 `MaxAmmo` 범위 안으로 clamp해 data default가 ammo 표시 계약을 넘지 않게 함
- 환경 위험물은 `AEnvironmentalHazard`가 server overlap만 확정한다. 대상은 항상 overlap의 `OtherActor`를 쓰고, `SweepResult`는 `bFromSweep`과 actor 일치가 확인될 때만 impact 정보로 사용한다
- ShotGun, StasisNet, DecoyGun의 탄약 고갈은 server `OnAmmoDepletion`이 `HughAnimState` action serial을 증가시켜 owner와 simulated proxy montage를 재생한다. `ANS_DropWeapon`은 server `DropWeapon()`만 실제 inventory 변경
- Hugh 조준·hover·착지의 `PlaySound2D`/`SpawnSound2D`는 `IsLocallyControlled()`에서만 실행한다. world state와 분리된 개인 2D SFX를 listen host의 원격 player simulation에서 만들지 않는다
- 휘두르는 melee/critical damage는 `UAnimNotifyState_AttackHitWindow`에서 server socket sweep으로 판정
- 순간 범위 melee damage는 `UAnimNotify_AttackHitOverlap`에서 server socket box overlap으로 판정
- `AttackHitOverlap`과 `AttackHitWindow` damage는 runtime owner의 `IPragmataDamageSource`에서 조회
- `AMonsterBase`는 선택된 attack pattern의 `bUsePatternAttackDamage`가 켜져 있으면 `PatternAttackDamage`를 현재 공격 damage로 제공
- pattern damage override가 꺼져 있거나 선택된 pattern이 없으면 기존 `CombatData.AttackDamage`를 fallback으로 사용
- 근접 종료형 공격은 `EndSectionName`으로 몽타주의 기존 종료 section 지정
- loop 진입은 현재 section의 next section이 자기 자신인지 runtime에서 자동 판정
- server는 loop 중 target 거리·유실·최대 지속 시간을 검사해 end section으로 즉시 전환
- loop 활성화와 end 전환은 multicast로 전달해 server/client가 같은 montage 경로 사용
- AI 공격은 server에서 몽타주를 재생하기 직전에 target의 수평 방향으로 즉시 정렬
- 단, 뒤쪽 조건 pattern은 즉시 정렬과 BT `Simple Parallel`의 `Turn To Target` task 회전을 생략
- monster 치명타는 `HealthComponent::ReceiveDamage` 호출 전에 공격·이동·피격 상황에 맞는 `DeathAnimation`을 고정
- `OnDeath -> HandleDeath -> SetDead`에서 AI·이동·공격 상태를 정리하고 모든 peer의 활성 montage를 중단
- Mesh만 공중에 뜨는 공격은 `DeathMontageBlendOutTime` 동안 현재 montage 포즈에서 Death sequence로 전환
- 치명타 직전 선택된 animation별 ragdoll 시간을 `CurrentDeathRagdollDelay`로 고정해 모든 peer에 전달
- `bEnableDeathRagdoll`을 끈 monster는 사망 animation만 유지하고 mesh physics로 전환하지 않음
- 각 peer는 `CurrentDeathRagdollDelay` 뒤 Capsule collision을 끄고 Mesh를 `Ragdoll` physics로 전환해 바닥까지 낙하
- `IsDead()`는 상태 조회만 담당하며 사망 side effect는 실행하지 않음
- monster는 hack 전 non-Hack damage를 기본 `0.2`배로 줄이고 damage text는 health clamp 전 최종 계산 대미지 표시
- weapon trace는 전체 `FHitResult`를 `FPragmataDamageSpec.ImpactHit`으로 전달해 Physics Asset body의 `BoneName` 유지
- monster Capsule과 BP 보조 body collision은 weapon `CharacterBody` trace를 무시하고 Skeletal Mesh만 Block
- 따라서 중앙의 머리·몸통도 Capsule의 `BoneName=None`에 가려지지 않고 실제 Physics Asset body가 hit
- 해킹 유지 상태에서 weapon impact bone이 BP `HackedWeakPoints` 항목과 일치하면 해당 `DamageMultiplier` 적용
- 몬스터별 hack 전 방어 배율은 monster BP에서 `Override Unhacked Damage Taken Multiplier` 체크 시 `UnhackedDamageTakenMultiplier`로 대체 가능
- 몬스터별 weapon 피해 상성은 monster BP의 `WeaponDamageTakenMultiplier`로 설정하며, hack 전 방어와 약점 배율 이후 마지막에 곱함
- 특정 weapon만 표와 어긋나면 `PerWeaponDamageTakenMultipliers`에 weapon class와 별도 배율을 넣고, 이 값은 공통 `WeaponDamageTakenMultiplier` 대신 사용됨
- per-weapon 항목의 `Use Hacked State Damage Taken Multipliers`를 켜면 StasisNet처럼 default/hacked damage가 나뉜 weapon을 monster hack 상태별로 따로 보정 가능
- 해킹 전이거나 non-Weapon 피해, impact가 없는 피해, 미등록 bone은 약점 배율을 적용하지 않음
- 모든 damage는 `SourceActor` 위치를 monster 정면 벡터와 비교해 `Front/Back` 방향 계산
- `SourceActor`가 없으면 `DamageCauser`를 사용하고 `ImpactDirection`은 `공격자 -> Monster`로 저장
- monster health가 처음으로 50%를 통과하면 그 피해 직전 공격 pattern·해킹 상태·Front/Back을 HitReact section으로 고정
- 공격 pattern 전용 Front/Back section을 우선하고 비어 있으면 Normal/Hacked 공통 Front/Back section으로 fallback
- 현재 50%를 통과시킨 피해가 `DamageKind::Hack`일 때만 Hacked Front/Back section 선택
- 이전에 해킹됐더라도 총격 등 non-Hack 피해로 50%를 통과하면 Normal Front/Back section 선택
- 반피 HitReact는 공격 montage를 완전히 취소하고 종료될 때까지 새 공격 선택을 잠금
- server가 확정한 montage section을 multicast해 모든 peer에서 한 번만 재생
- Hugh는 `ReceiveDamage`에서 `Super` 호출 전후 health를 비교해 실제 감소량으로 HitReact 강도를 계산
- Hugh HitReact 기준은 `Small <= 0.04`, `Medium <= 0.10`, `Large > 0.10`
- Hugh `Small/Medium` 피격은 Hugh montage만 재생하고, `Large/Death/ForcedPair`만 Diana paired cue를 보냄

GripGun reload timer 쉽게 보기:

1. GripGun 탄약이 0이 되면 `BeginServerReload()`가 `bIsReloading = true`와 server timer를 시작
2. 복제된 reload state를 받은 각 peer는 Hugh reload montage를 로컬 재생
3. montage가 끝나거나 중단돼도 gameplay timer는 유지
4. server timer가 끝나면 `FinishServerReload()`가 `bIsReloading = false`, `CurAmmunition = 1`을 확정
5. 이후 auto replenish가 max ammo까지 다시 진행

이전 회귀 원인:

- 문제의 본질은 `AnimNotify`가 아니라 montage 완료 callback에 gameplay reload 완료가 의존한 구조
- server mesh가 montage를 갱신하지 않거나 callback이 누락되면 reload lock이 남을 수 있었음
- 결과는 0발 고정 또는 1발까지만 보충 후 정지

현재 기준:

- reload gameplay state는 GripGun이 소유
- Hugh는 reload animation presentation만 담당
- reload 완료는 server timer의 `FinishServerReload()` 하나로 수렴
- weapon switch와 montage interrupt는 timer를 취소하지 않음
- death, respawn, Actor destruction은 reload timer를 명시적으로 정리
- `AnimNotifyState`는 타격 판정처럼 일정 구간이 필요한 gameplay window에 사용하고, GripGun 탄약 보충 완료 조건으로 쓰지 않음

동작 흐름:

```text
Gun trace
-> AGripGun::PullTrigger server authority gate
-> CommitServerShot ammo/reload/trace 확정
-> DealDamageToHitActor
-> FPragmataDamageSpec 생성
-> ImpactHit.BoneName 전달
-> DealDamageToActor
-> ReceiveDamage
-> bHacked + Weapon + 등록 bone이면 약점 배율 적용
-> HealthComponent::ReceiveDamage
-> 생존 + player가 Territory 안에 있으면 AISense_Damage stimulus 직접 등록
-> OnTargetDetected
-> TrySetTargetIfPerceived
-> MonsterAIController::SetTarget

Melee montage
-> AnimNotifyState_AttackHitWindow
-> socket sweep
-> FPragmataDamageSpec 생성
-> DealDamageToActor
-> ReceiveDamage

Impact montage
-> AnimNotify_AttackHitOverlap
-> socket box overlap
-> FPragmataDamageSpec 생성
-> DealDamageToActor
-> ReceiveDamage

Watcher attack
-> UBTTask_Watcher_AttackTarget
-> Projectile trace
-> target hit이면 해당 HitResult로 FireBullet
-> trace miss이면 target 위치 synthetic HitResult로 FireBullet
-> 다른 actor hit이면 발사 차단 로그
-> FireBullet에서 FPragmataDamageSpec 생성
-> DealDamageToActor

Diana hack
-> ApplySuccessfulHack
-> FPragmataDamageSpec 생성
-> ReceiveDamage (DamageKind == Hack은 hack 전 기본 0.2배 감쇠 제외)
-> 최초 hacked 진입이면 bHacked = true + duration timer 시작
-> 이미 hacked 상태면 damage/보상만 적용하고 기존 timer 유지

Proximity loop montage
-> montage asset에 설정된 self-loop section 반복
-> server가 매 tick target 거리와 최대 시간 검사
-> 근접·target 유실·시간 초과 시 EndSectionName으로 즉시 전환

AI attack facing
-> AttackByAI가 montage 재생 직전에 target 방향으로 즉시 회전
-> 뒤쪽 조건 pattern은 현재 방향을 유지하고 BT Turn To Target task 회전을 생략
-> 일반 pattern은 montage 재생 중 BT Turn To Target이 계속 회전 추적

Monster death
-> 치명타 직전 DeathAnimation 확정
-> HealthComponent가 bIsDead 설정 후 OnDeath 전달
-> server는 BT·이동·공격 runtime 정리
-> 각 peer는 multicast로 재생된 montage 중단
-> AnimBP Death 상태의 Sequence Player가 선택된 animation 재생
-> 선택된 animation의 ragdoll delay 뒤 Mesh physics가 마지막 포즈부터 낙하 처리

Monster half-health hit react
-> damage 적용 전 50% crossing과 현재 attack pattern 확인
-> attack pattern 또는 Normal/Hacked의 Front/Back section 확정
-> damage 적용 후 생존했을 때 기존 공격 montage와 gameplay attack state 종료
-> server/client에서 공통 HitReact montage의 확정 section 1회 재생
-> montage 종료 후 cooldown 상태를 다시 계산해 다음 공격 선택 허용

Sentinel phase transition sequence
-> Sentinel이 `Play Phase Transition Sequence`를 켰고 sequence asset이 있으면 반피 HitReact 종료 뒤 NetMulticast로 각 peer에서 재생
-> sequence binding tag(기본 `Boss`)를 현재 Sentinel actor로 교체
-> sequence 종료 전에는 HitReact 공격 잠금을 유지하고, 종료 뒤 공통 cooldown 상태를 다시 계산

Hugh hit react
-> AHughCharacter::ReceiveDamage_Implementation
-> PreviousHealth 저장
-> Super::ReceiveDamage_Implementation
-> ActualDamage = PreviousHealth - CurrentHealth
-> Hugh HitReact montage 직접 재생
-> Large/Death/ForcedPair이면 Diana paired cue 전송
```

핵심 위치:

- `FPragmataDamageSpec`
- `UPragmataDamageLibrary`
- `IPragmataDamageable`
- `IPragmataDamageSource`
- `UPragmataHealthComponent`
- `AWeaponBase`
- `AMonsterBase`
- `UMonsterAnimInstance`
- `AHughCharacter::ReceiveDamage_Implementation`
- `AHughCharacter::ResolveHitReactStrength`
- `AHughCharacter::ResolveHitReactDirection`
- `UAnimNotifyState_AttackHitWindow`
- `UAnimNotify_AttackHitOverlap`
- `AEnvironmentalHazard`

수정할 때 주의:

- damage 함수명에 `Apply`, `Try`, 불필요한 `Pragmata`를 다시 넣지 않음
- Unreal damage API를 main gameplay path로 되돌리지 않음
- 새 damage source는 source 쪽에서 `FPragmataDamageSpec` 생성 후 `DealDamageToActor` 호출
- `AGripGun::PullTrigger()` 안에서 client-side ammo, reload, damage 판정을 다시 열지 않음
- GripGun 재장전 완료는 server timer의 `FinishServerReload()`로만 처리하고 montage callback에서 탄약이나 reload state를 변경하지 않음
- monster BP의 `Hacked Weak Points` 배열에는 Physics Asset Body가 연결된 `Bone Name`과 1 이상 `Damage Multiplier` 지정
- monster BP의 `Weapon Damage Taken Multiplier`는 총기 피해만 받는 공통 배율이며 0은 총기 면역, 1은 기본값, 1보다 크면 총기 취약
- monster BP의 `Per Weapon Damage Taken Multipliers`는 weapon class별 override 배열이며, 예를 들어 Walker의 GripGun 기준 공통 배율은 유지하고 StasisNet만 낮출 때 사용
- StasisNet 항목에서 `Use Hacked State Damage Taken Multipliers`를 켜면 Default 배율은 기본 도트딜 표에, Hacked 배율은 hack 도트딜 표에 맞출 수 있음
- 약점 이름은 Physics Asset Tree의 body 표시 bone 또는 실제 weapon `HitResult.BoneName`과 정확히 일치시킴
- `BoneName`이 비어 있으면 가까운 bone을 추정하지 않고 일반 부위로 처리
- 총격 로그의 `Component`가 monster Skeletal Mesh이고 `BoneName`이 Physics Asset body bone과 일치하는지 먼저 확인
- 로그가 `CollisionCylinder, BoneName=None`이면 monster 공통 weapon collision 응답이 적용된 새 C++로 실행 중인지 확인
- 약점 material 색상 피드백은 현재 damage 판정에 포함하지 않고 별도 후속 단계에서 연결
- melee notify는 server authority에서만 damage 확정
- notify window마다 hit actor set을 유지해 같은 공격 window 중복 피해 방지
- overlap notify는 한 번의 query에서 actor별 1회만 damage 전달
- overlap과 hit window notify owner가 `IPragmataDamageSource`를 구현하지 않거나 damage가 0 이하면 판정 중단
- overlap box는 socket 회전을 따르며 `HitBoxExtent` X/Y/Z를 montage Details에서 조정
- laser와 molten asphalt 같은 접촉형 위험물은 BP에서 `AEnvironmentalHazard` 상속 후 `HazardTrigger` 크기와 시각 컴포넌트만 다르게 구성
- montage preview viewport는 overlap의 box와 hit window의 sweep sphere를 각각 runtime 크기로 표시
- `AttackHitWindow.HitRadius`는 runtime sphere sweep과 editor preview sphere에 함께 적용
- `AttackHitWindow.AttackSocketOffset`은 socket local space 기준이며 runtime sweep과 editor preview 위치에 함께 적용
- `bPreviewOnlyNearNotifyTime` 선택 시 point notify는 trigger 주변, state notify는 window 전후 `PreviewTimeTolerance` 동안만 표시
- montage에 notify를 배치하는 작업은 Editor 수동 작업
- 반피 HitReact montage에는 Normal/Hacked Front/Back과 필요한 attack pattern Front/Back section을 각각 생성
- 각 HitReact section의 `Next Section`은 비워 한 section 재생 후 다른 피격 animation으로 이어지지 않게 설정
- `FMonsterAttackPattern.HalfHealthHitReactFrontSectionName`과 `HalfHealthHitReactBackSectionName`이 비면 현재 Normal/Hacked section 사용
- `SourceActor` 기준 정면 반구는 Front, 후면 반구는 Back section 사용
- 반피 HitReact 중에는 `CanAttack()`을 false로 유지하고 montage 종료 후 `RefreshAttackAvailability()` 호출
- 한 번 50%를 통과하면 회복 후 다시 50% 아래로 내려가도 HitReact 반복 재생 금지
- `IsDead()` 같은 query 함수에 montage 중단이나 AI 정리 side effect를 넣지 않음
- 사망 시 `FinishAttackByAI()`를 호출하지 않아 attack cooldown과 정상 완료 delegate 실행 방지
- 공격별 `FMonsterAttackPattern.DeathAnimation`은 각 monster BP의 pattern에 직접 지정
- Default·Locomotion·Hack·Critical 사망은 monster BP별 `FMonsterDeathAnimationSet`에 지정
- 각 상태의 `Use <State> Direction Variants`를 끄면 단일 animation, 켜면 `Front/Back Animation` 지정
- `Front/Back Animation`은 쓰러지는 방향이 아니라 공격이 들어온 면을 기준으로 매핑
- `DamageKind == Melee` 치명타는 현재 공격·이동 상태보다 우선해 `Critical Animation` 선택
- `ABP_ExecutorMonster`의 Death 상태는 Sequence Player의 Sequence pin에 `DeathAnimation` 연결
- `DeathMontageBlendOutTime`과 `ToDeath -> Death` transition crossfade를 비슷한 값으로 설정
- `FMonsterDeathAnimationSet`은 각 Animation 바로 뒤의 같은 이름 `RagdollDelay`로 개별 전환 시간 지정
- `FMonsterAttackPattern`은 공격별 `DeathAnimation`과 `DeathRagdollDelay`를 한 쌍으로 지정
- monster BP의 `Enable Death Ragdoll`로 해당 monster 전체의 ragdoll 전환 사용 여부 지정
- animation별 `RagdollDelay`가 `-1`이면 Monster 공통 `DeathRagdollDelay` 사용, `0`이면 즉시 전환
- 실제 ragdoll 시간은 사망 animation의 마지막 정상 프레임에 맞추고 actor 제거 시간인 `DeadEventDelayTime`보다 짧게 유지
- ragdoll에는 Skeletal Mesh와 호환되는 `Physics Asset`이 필요하며 Mesh는 Pawn collision을 무시
- Walker의 `SM_Walker`는 `SM_Walker_PhysicsAsset`을 기본 Physics Asset으로 사용
- Physics Asset은 `SM_Walker`의 hard reference로 cook되므로 별도 PAL explicit asset에 추가하지 않음
- 근접 loop 코드는 section을 생성하지 않으며 montage에 이미 존재하는 이름만 사용
- section 이름이나 `EndSectionName`이 없으면 원본 montage 기본 연결로 재생
- 원본 montage의 `Start -> Loop -> End` 연결은 runtime 설정 실패에 대비한 fallback으로 유지
- Start부터 End까지 이어지는 montage-level NotifyState는 loop 자기 반복 중에도 활성 범위를 유지

검증 방법:

- monster BP의 `MaxHealth`를 100보다 크게 변경한 뒤 PIE에서 `CurrentHealth == MaxHealth`인지 확인
- unhacked monster weapon hit: damage `0.2 또는 UnhackedDamageTakenMultiplier override * weapon 공통/개별 배율`배
- hacked monster weapon hit: weapon 공통/개별 배율이 적용된 full damage
- per-weapon hack state split hit: default 상태는 `DefaultDamageTakenMultiplier`, hacked 상태는 `HackedDamageTakenMultiplier` 적용
- hacked monster registered weak-point weapon hit: `DamageMultiplier * weapon 공통/개별 배율`이 적용된 damage
- unhacked monster registered bone hit: 약점 배율 없이 기존 방어 배율과 weapon 공통/개별 배율 적용
- hacked monster unregistered bone hit: 일반 full damage
- overkill hit: 남은 health가 아니라 방어·약점 배율까지 반영된 전체 공격 대미지 표시
- hack damage: full damage 적용, 최초 hacked 진입이면 `bHacked` true와 duration timer 시작
- repeat hack damage: 이미 hacked 상태면 damage/보상만 적용하고 기존 timer 유지
- melee notify window: target당 1회 damage
- melee notify window: `HitRadius`를 변경하면 runtime sweep과 editor sphere가 같은 크기로 변경
- melee notify window: `AttackSocketOffset` XYZ를 변경하면 socket 회전을 따라 runtime과 preview 위치가 함께 변경
- melee notify window: damage는 notify 고정값이 아니라 source interface에서 조회하며 monster는 선택된 pattern override 또는 `CombatData.AttackDamage`를 반환
- melee overlap notify: 타격 프레임의 범위 안 target당 1회 damage
- 공격 중 사망: 공격 montage가 중단되고 해당 pattern의 `DeathAnimation`으로 전환
- 공격 중 반피: 해당 pattern의 Front/Back HitReact section 재생 후 기존 공격을 이어가지 않고 새 공격 재선택
- 일반/해킹 상태 반피: 공격 방향에 맞는 Normal/Hacked Front/Back section 1회 재생
- 반피를 건너 바로 사망: HitReact를 생략하고 Death animation만 재생
- listen server와 client에서 같은 반피 HitReact section 재생 확인
- Locomotion 사망: Idle/Move 구분 없이 같은 sequence 선택
- listen server와 client에서 같은 `DeathAnimation` 재생 확인
- 공중 포즈 사망: animation 뒤 ragdoll로 전환해 바닥에 안착하고 player 이동을 막지 않는지 확인
- Walker 사망: Output Log에 `Physics Asset is missing` 경고가 없고 ragdoll body가 정상 생성되는지 확인
- 근접 loop montage: 접근 중 `Loop -> Loop`, 근접 시 즉시 `End -> StandUp`
- target 유실 또는 최대 loop 시간 초과 시 End로 빠져 공격 task 종료
- listen server와 client에서 같은 section 순서와 공격 종료 시점 확인
- damage text: health 실제 감소량 표시

추가 attack pattern 조건:

- `FMonsterAttackPattern.bRequireTargetBehind`가 켜진 pattern은 target이 몬스터 뒤쪽 cone 안에 있을 때만 선택 후보에 포함
- 뒤쪽 cone 폭은 `TargetBehindAngleDegrees`로 조정하고, 거리는 기존 `MinimumAttackRange`/`AttackRange`/`AttackRadius` 조건을 그대로 사용
- 뒤쪽 조건 pattern은 공격 시작 직전 target 바라보기를 생략하고, BT `Turn To Target` task도 선택된 pattern을 확인해 회전하지 않으므로 현재 바라보는 방향 기준 후방 이동 animation을 그대로 사용

## Look Input Contract

문제:

- `IA_Look`가 `Boolean`이면 `Mouse2D` 입력이 작은 움직임에도 카메라 spin처럼 보일 수 있음
- C++는 `FVector2D` look 값을 기대하므로 InputAction asset type과 runtime contract가 맞아야 함
- PIE/package에서 mouse가 viewport 밖으로 빠지면 `IA_Look`가 register되지 않음
- `Title`/`Lobby`의 `GameAndUI` input mode와 visible cursor가 gameplay까지 남으면 mouse capture가 풀림
- GameInput으로 들어온 DualSense right stick은 idle 상태에서도 작은 축 값이 남을 수 있어 명시적 deadzone이 없으면 view가 한쪽으로 흐를 수 있음

현재 해법:

- `/Game/Blueprints/Player/Input/Common/IA_Look`의 value type은 `Axis2D`
- Hugh는 `AHughCharacter::OnTriggerLook()`에서 `FVector2D`를 controller yaw/pitch로 적용
- Diana는 같은 look action을 `ADianaOperatorPawn` camera rotation intent로 적용하되 pitch는 C++에서 `Mouse2D.Y`를 반전하고 Hugh 일반 look과 같은 `CameraYawSpeed=5.0`, `CameraPitchSpeed=5.0`을 사용
- `IMC_Hugh`, `IMC_Diana`의 `IA_Look` `Gamepad_Right2D` mapping에만 `InputModifierDeadZone`을 둔다. 설정은 `LowerThreshold=0.30`, `UpperThreshold=1.0`, `Type=Axial`
- `IMC_Hugh`, `IMC_Diana`, `IMC_Lobby`, `IMC_Title`이 key 배치를 소유하고 C++은 `InputAction`만 load/bind
- gameplay controller 진입 시 `APragmataPlayerController::ApplyGameplayInputMode()`에서 `FInputModeGameOnly`, `bShowMouseCursor=false`, look/move ignore 해제를 적용
- `BeginPlay`, `OnPossess`, `AcknowledgePossession`, `OnRep_PlayerState` 모두 gameplay input mode를 다시 보정

동작 흐름:

```text
IA_Look Axis2D
-> gameplay PlayerController GameOnly input mode
-> IMC_Hugh 또는 IMC_Diana
-> Gamepad_Right2D만 Axial DeadZone 0.30 적용
-> FInputActionValue.Get<FVector2D>()
-> Hugh controller rotation 또는 Diana operator pawn rotation
```

핵심 위치:

- `/Game/Blueprints/Player/Input/Common/IA_Look`
- `/Game/Blueprints/Player/Input/Hugh/IMC_Hugh`
- `/Game/Blueprints/Player/Input/Diana/IMC_Diana`
- `AHughCharacter::OnTriggerLook`
- `ADianaOperatorPawn::OnTriggerLook`
- `APragmataPlayerController::ApplyGameplayInputMode`
- `APragmataPlayerController::ApplyRoleInputAndCamera`

수정할 때 주의:

- camera spin이나 look 입력 이상은 C++ 보정 전에 InputAction `ValueType`부터 확인
- mouse가 창 밖으로 빠지거나 `IA_Look`가 아예 register되지 않으면 input mode/cursor capture부터 확인
- Hugh와 Diana가 같은 `IA_Look` asset을 공유하므로 변경 시 양쪽 입력을 모두 검증
- Diana 세로 look 반전은 shared `IA_Look` asset이 아니라 `ADianaOperatorPawn::AddCameraInput()`에서만 처리
- gamepad drift는 `Gamepad_Right2D` mapping modifier로 처리한다. `Mouse2D`에는 deadzone을 붙이지 않고, `input.GlobalAxisConfigMode=1` 같은 전역 축 CVar도 사용하지 않는다
- input asset 이동 시 C++ soft path와 PAL explicit asset을 같이 갱신
- asset 저장 작업은 target asset만 바뀌었는지 확인

검증 방법:

- `IA_Look.uasset` binary text에 `EInputActionValueType::Axis2D` 또는 `ValueType Axis2D` 확인
- UnreelMCP `input.mapping.list`로 `IMC_Hugh` index `11`, `IMC_Diana` index `1`의 `Gamepad_Right2D` mapping에만 `InputModifierDeadZone`이 있는지 확인
- `BP_DianaOperatorPawn` CDO 또는 C++ default에서 `CameraYawSpeed=5.0`, `CameraPitchSpeed=5.0` 확인
- PIE에서 DualSense idle drift 없음, right stick look 동작, mouse look 즉시 반응을 같이 확인

## Input Device Prompt State

문제:

- UI는 마지막 입력 장치에 따라 keyboard/mouse prompt와 gamepad prompt를 즉시 바꿔야 한다.
- UI 쪽에서 input backend를 직접 판단하면 Widget마다 기준이 갈라진다.

현재 해법:

- `APragmataPlayerController`가 `InputKey`에서 마지막 의미 있는 local input을 보고 `bIsGamepadInput`을 갱신한다.
- UI는 `bIsGamepadInput` 변수 또는 `IsGamepadInput()` getter만 읽는다.
- Windows DualSense 경로는 UE 5.8 deprecated `RawInput` 대신 `GameInputWindows` 플러그인을 사용한다.
- `Config/Windows/WindowsInput.ini`에서 GameInput gamepad 처리를 켜고, HID family 중심으로 제한해 XInput 중복 입력 위험을 줄인다.

흐름:

1. keyboard/mouse 또는 gamepad input event가 owning `APragmataPlayerController`로 들어온다.
2. release, touch, simulated input, 작은 analog drift는 무시한다.
3. `Params.IsGamepad()` 결과를 `bIsGamepadInput`에 저장한다.
4. Widget Blueprint는 변수 또는 getter를 읽어 icon set만 교체한다.

주의:

- 이 값은 local UI prompt용이며 복제하지 않는다.
- PlayStation glyph와 Xbox glyph를 구분하는 별도 platform/glyph state는 아직 없다.
- GameInput backend가 실제 DualSense key event를 올리는지는 장치 연결 상태에서 수동 확인해야 한다.
- PIE에서 mouse look이 작은 이동량에 비례해 움직이는지 확인
- PIE에서 gameplay viewport 클릭 후 mouse가 창 밖으로 빠지지 않는지 확인
- Diana client에서 mouse up/down이 기대 방향과 일치하는지 확인

관련 문서:

- `Docs/Architecture/architecture.md`의 `Input Flow`
- `Docs/Systems/Server/server.md`의 role별 input/replication 확인 항목

## Hugh HeadVisor Spotlight

문제:

- `SK_Hugh` socket은 attach 위치만 제공하고 runtime light component를 직접 소유하지 않음
- visor light toggle은 Hugh 소유자 입력과 remote 화면 표시를 함께 맞춰야 함
- visor light toggle은 실제 Enhanced Input action/mapping asset으로 보여야 함

현재 해법:

- `AHughCharacter`가 `HeadVisorSpotlight` `USpotLightComponent`를 생성
- 기본 attach socket은 Hugh body mesh의 `HeadVisor_LightSocket`
- socket이 없으면 mesh root fallback attach 후 warning log 출력
- `bHeadVisorSpotlightEnabled`를 서버 replicated bool로 유지
- owner 입력은 local 예측 적용 후 `Server_SetHeadVisorSpotlightEnabled()`로 서버 확정
- `/Game/Blueprints/Player/Input/Hugh/IA_HeadVisorLightToggle`는 Boolean Input Action
- `IMC_Hugh`가 `IA_HeadVisorLightToggle`을 `T`, `Gamepad_LeftShoulder`에 매핑
- C++은 `ResolveInputActions()`에서 해당 IA asset을 load한 뒤 `SetupPlayerInputComponent()`에서 bind

핵심 위치:

- `AHughCharacter::EnsureHeadVisorSpotlightAttachment`
- `AHughCharacter::ResolveInputActions`
- `AHughCharacter::OnToggleHeadVisorSpotlight`
- `AHughCharacter::OnRep_HeadVisorSpotlightEnabled`
- `Content/Blueprints/Player/Input/Hugh/IA_HeadVisorLightToggle.uasset`
- `Content/Blueprints/Player/Input/Hugh/IMC_Hugh.uasset`
- `BP_HughCharacter > Mesh`
- `Content/Character/Hugh/SK_Hugh.uasset`

수정할 때 주의:

- socket transform 조정은 `SK_Hugh` 또는 `BP_HughCharacter` viewport에서 확인
- left shoulder가 다른 Hugh action과 충돌하면 `IMC_Hugh` mapping 정책을 먼저 확인
- light cone 방향은 socket attach 직후 relative yaw `-90` 오프셋 재적용

검증 방법:

- PIE에서 Hugh spawn 후 visor light가 `HeadVisor_LightSocket`을 따라감
- `T` 입력으로 on/off 전환
- gamepad left shoulder 입력으로 on/off 전환
- 2-player listen server에서 remote 화면도 같은 on/off 상태 확인
- socket 저장 전이면 `Missing socket: HeadVisor_LightSocket` warning 확인

## Hugh Heal Hold Input

문제:

- Hugh 회복 기능은 `UPragmataPlayerStatsComponent::ConsumeRepairCartridge()`로 존재하지만, 실제 Hugh 입력과 HUD hold progress source가 없으면 플레이어가 사용할 수 없음
- heal은 수량/HP/이동 상태 blocker에 따라 UI에서 사용 불가 상태를 보여야 하므로 단순 input bind만으로 충분하지 않음

현재 해법:

- `/Game/Blueprints/Player/Input/Hugh/IA_Heal`은 Boolean Input Action
- `IMC_Hugh`가 `IA_Heal`을 `Q`, `Gamepad_FaceButton_Right`에 매핑
- C++은 `AHughCharacter::ResolveInputActions()`에서 `IA_Heal` fallback load 후 `SetupPlayerInputComponent()`에서 `Started`, `Completed`, `Canceled`를 bind
- local Hugh는 hold 시간만 예측하고, 1초 threshold 도달 시 `Server_RequestHeal()`로 서버에 사용 intent를 보냄
- hold가 계속 유지되면 `HealRepeatIntervalSeconds`마다 재요청
- 서버는 `CalculateHealAvailability()`로 cartridge, HP, death, reload, dash, run, falling, hovering blocker를 재검증한 뒤 `ConsumeRepairCartridge()` 호출
- heal 가능 상태는 `bCanUseHeal`로 복제하고 `OnHealAvailabilityChanged`로 HUD ViewModel에 전달
- circular progress는 local 입력 UI 값이므로 복제하지 않고 `OnHealHoldChanged`로 local ViewModel만 갱신
- `PAL_CoreRuntime`에는 `IMC_Hugh`와 개별 `IA_Heal`이 모두 explicit asset으로 들어감

동작 흐름:

```text
IA_Heal Started
-> local hold timer 0부터 시작
-> 1초 도달
-> Server_RequestHeal
-> server CalculateHealAvailability
-> ConsumeRepairCartridge
-> Health/RepairCartridge/bCanUseHeal replication
-> UPragmataHUDViewModel recovery fields 갱신
```

핵심 위치:

- `AHughCharacter::OnStartHeal`
- `AHughCharacter::UpdateHealHold`
- `AHughCharacter::Server_RequestHeal`
- `AHughCharacter::CanUseHeal`
- `UPragmataPlayerStatsComponent::GetRemainingHealCartridges`
- `UPragmataHUDViewModel::CanUseHeal`
- `Content/Blueprints/Player/Input/Hugh/IA_Heal.uasset`
- `Content/Blueprints/Player/Input/Hugh/IMC_Hugh.uasset`
- `Content/CookLabels/PAL_CoreRuntime.uasset`

수정할 때 주의:

- full HP는 heal 사용 불가 상태로 취급
- dodge/boost 계열 blocker는 현재 `IsDashing`과 `HughAnimState.bIsRunning`을 모두 차단
- airborne blocker는 `CharacterMovement->IsFalling()`과 `HughAnimState.bIsHovering`을 모두 차단
- UI 위젯은 heal을 직접 호출하지 말고 ViewModel의 `CanUseHeal`, `RecoveryHoldProgress`, `RecoveryItemCount`를 읽음
- 새 Enhanced Input asset을 추가하면 `PAL_CoreRuntime` explicit asset을 같이 갱신

검증 방법:

- Hugh로 `Q` 1초 hold 시 1회 회복
- `Q` hold 유지 시 1초마다 반복 회복, cartridge 0에서 progress reset
- `Gamepad_FaceButton_Right`도 같은 동작
- full HP, cartridge 0, reload, dash/run, falling/hovering 중 `CanUseHeal=false`
- Hugh host/Diana client와 Diana host/Hugh client에서 HUD count와 가능 상태 동기화 확인

## Global Power and Map Lights

문제:

- `ALightBase`는 `ULightComponent::SetVisibility()`를 로컬에서 적용
- listen server에서 전원 장치를 켜면 서버 화면은 갱신되지만 remote client는 전원 변경 신호를 받지 못할 수 있음
- LightComponent visibility 자체를 서버에서 바꾼다고 client 표시가 자동으로 맞지 않음

현재 해법:

- `AInteractionPowerSupply`는 서버에서만 `APragmataGameState::SetGlobalPowerState(true)` 호출
- `APragmataGameState::bGlobalPowerOn`은 `ReplicatedUsing=OnRep_GlobalPowerOn`으로 복제
- 서버는 `SetGlobalPowerState()`에서 `OnPowerStateChanged`를 broadcast
- client는 `OnRep_GlobalPowerOn()`에서 같은 `OnPowerStateChanged`를 broadcast
- `ALightBase`와 `APoweredResponBox`는 GameState delegate를 구독하고 각자 로컬 표시/잠금 상태를 갱신

동작 흐름:

```text
AInteractionPowerSupply::Interact_Implementation
-> APragmataGameState::SetGlobalPowerState(true)
-> server OnPowerStateChanged broadcast
-> bGlobalPowerOn replication
-> client OnRep_GlobalPowerOn
-> client OnPowerStateChanged broadcast
-> ALightBase::HandlePowerStateChanged
```

핵심 위치:

- `APragmataGameState::SetGlobalPowerState`
- `APragmataGameState::OnRep_GlobalPowerOn`
- `ALightBase::HandlePowerStateChanged`
- `APoweredResponBox::HandlePowerStateChanged`
- `AInteractionPowerSupply::Interact_Implementation`

수정할 때 주의:

- client에서 `SetGlobalPowerState()`를 직접 실행하지 않음
- 새 전원 반응 actor는 GameState의 `OnPowerStateChanged`를 구독하고 BeginPlay에서 `GetGlobalPowerState()`로 초기 동기화
- 개별 LightComponent visibility를 gameplay state로 취급하지 않고, 복제된 전원 bool에서 로컬 표시를 재생성

검증 방법:

- 2-player listen server에서 Diana가 전원 노드 완료
- listen server 화면과 remote client 화면 모두 맵 조명 켜짐 확인
- 전원 의존 문/상자도 client에서 같은 잠금 상태로 동작하는지 확인

## Item Box Interactable

현재 구조:

- `AItemBoxInteractable`은 `AAnimatedPropInteraction`을 상속해 공통 animated prop 상호작용 경로를 사용
- 상호작용은 서버에서만 확정하고 `bIsOpened`로 1회만 처리
- 플레이어 보상/아이템 지급은 `Interact_Implementation()`의 TODO 위치에서 `Interactor` 기준으로 연결
- `AnimatedMesh`에 skeletal mesh를 지정하고 `InteractionAnimation`에 열림 animation asset 지정
- `InteractionAnimationSerial`을 복제해 listen server와 client가 같은 열림 애니메이션을 로컬 재생
- `OnInteractionActivated` delegate는 상자 열림 확정 후 broadcast
- POI metadata는 `ItemBox`, Diana interaction 거리는 `750cm`
- prompt는 Diana local player가 직접 조준한 actor에 붙은 `WidgetComponent`에서만 표시
- 실제 열림도 서버에서 `Interactor`가 Diana role인지 다시 확인한 뒤 확정

동작 흐름:

```text
IA_Interact
-> AItemBoxInteractable::Interact_Implementation
-> Diana role 검증
-> server bIsOpened 확정
-> TODO player item/reward function
-> AAnimatedPropInteraction::Interact_Implementation
-> PlayInteractionAnimation
-> InteractionAnimationSerial replication
-> client PlayInteractionAnimation
```

수정 시 주의:

- 아이템 지급 로직은 client가 직접 확정하지 않고 서버 상호작용 경로에 연결
- animation asset은 C++에서 hard load하지 않고 BP/Editor에서 지정
- 이미 열린 상자는 다시 상호작용해도 보상과 애니메이션을 반복하지 않음
- ItemBox 전용 animation member를 다시 만들지 않고 `AAnimatedPropInteraction`의 공통 field를 사용
- Hugh 화면에서 prompt가 보이거나 Hugh interaction 경로로 열리면 `AItemBoxInteractable::ShouldCreateInteractionPromptWidget()`와 `IsDianaInteractor()`를 먼저 확인
- `LeftClick` 안내가 화면 중앙에 뜨면 `APropInteractionBase::EnsureHoldKeyPromptWidget()`가 prop의 `WBP_HoldKeyPrompt` `WidgetComponent`만 찾고 `AddToViewport()`를 호출하지 않는지 확인
- ItemBox 계열 BP의 key prompt는 `ScanIconComponent`가 아닌 별도 `WidgetComponent`여야 함
- static mesh만 있는 상자라면 skeletal mesh와 animation asset 또는 별도 timeline 구조가 필요

검증 방법:

- BP 자식 생성 후 `AnimatedMesh`, `InteractionAnimation`, `InteractionAnimationPlayRate` 지정
- listen server에서 상호작용 시 server 화면에서 상자 열림 확인
- client 화면에서도 같은 상자 열림 애니메이션 확인
- 두 번째 상호작용에서 보상 TODO 위치와 애니메이션이 반복되지 않는지 확인
- Hugh 화면에는 ItemBox click prompt가 보이지 않고 Diana 화면에서 직접 조준할 때만 `LeftClick` prompt가 표시되는지 확인
- Diana가 조준만 해서는 열리지 않고 `IA_Interact` 입력 후에만 열리는지 확인

## Animated Prop Interaction

현재 구조:

- `AAnimatedPropInteraction`은 `APropInteractionBase`를 상속
- `AnimatedMesh`에 skeletal mesh를 지정하고 `InteractionAnimation`에 상호작용 애니메이션 지정
- `APropInteractionBase::EnsureHoldKeyPromptWidget()`는 `ScanIconComponent`를 제외하고 `WBP_HoldKeyPrompt` 계열 `WidgetComponent`를 찾아 초기에는 숨김
- `UDianaNodeInteractionComponent::SetFocusedNodeActor()`가 `ShouldShowInteractionPromptOnDianaFocus()`를 허용한 prop을 직접 조준할 때 `ShowInteractionPromptWidget()` 호출로 prompt 표시
- focus가 actor를 벗어나거나 node interaction aim gate가 풀리면 `HideInteractionPromptWidget()` 호출로 prompt 숨김
- `BP_ItemBox`, `BP_DeadFilament_Box`는 `InteractionPromptKeyTexture`의 `/Game/UI/Texture/Input/LeftClick`으로 prop prompt의 기존 `QButton` texture를 교체
- `ShouldCreateInteractionPromptWidget()` override로 특정 prop의 local player role별 prompt 노출을 막을 수 있음
- `AElevator`, `ACheckPoint`는 별도 override 없이 부모의 상호작용 애니메이션/위젯 흐름을 그대로 사용
- `Interact_Implementation()`은 서버에서 애니메이션을 재생하고 `InteractionAnimationSerial`을 복제
- client는 `OnRep_InteractionAnimationSerial()`에서 같은 애니메이션을 로컬 재생
- 애니메이션 재생 후 `Super::Interact_Implementation(Interactor)`를 호출해 기존 `OnInteractionActivated` 흐름 유지

동작 흐름:

```text
Interaction input
-> AAnimatedPropInteraction::Interact_Implementation
-> server PlayInteractionAnimation
-> InteractionAnimationSerial replication
-> client PlayInteractionAnimation
-> Super::Interact_Implementation
-> OnInteractionActivated broadcast
```

수정 시 주의:

- 일반 상호작용 delegate를 써야 하면 `Super::Interact_Implementation()` 호출을 유지
- animation asset은 C++ hard load 없이 BP/Editor에서 지정
- static mesh만 있는 prop은 skeletal animation을 재생할 수 없으므로 timeline 또는 별도 static mesh 방식 필요
- 반복 상호작용을 막아야 하면 별도 `bHasInteracted` 상태와 복제 설계 추가
- role별 prompt 제한은 개별 WBP visibility hack이 아니라 prop class override로 처리

검증 방법:

- BP 자식 생성 후 `AnimatedMesh`, `InteractionAnimation`, `InteractionAnimationPlayRate` 지정
- listen server에서 상호작용 시 서버 화면 애니메이션 확인
- client 화면에서도 같은 애니메이션 재생 확인
- `OnInteractionActivated`를 구독하는 actor가 있으면 기존처럼 호출되는지 확인

## Dead Filament Special Interaction

현재 구조:

- `ISpecialInteractableInterface`도 함수명은 `Interact(AActor* Interactor)`로 통일
- 일반/특수 상호작용 구분은 함수명이 아니라 interface type으로 구분
- `ADeadFilamentObstacle`은 `AActor`와 `ISpecialInteractableInterface`를 구현
- `DeadFilamentMesh`에는 BP/Editor에서 준비된 static mesh 지정
- `Interact_Implementation()`은 서버 권한과 정화 스킬 실행 중인 Diana interactor만 통과시킨 뒤 `Destroy()` 호출
- `CanBeCleansedBy()`는 `Interactor`의 `UDianaCleanseComponent::IsServerCleanseExecuting()` 상태를 확인해 일반 상호작용 제거를 막음
- 실제 targeting, cooldown, range, cone, LOS 검증은 `UDianaCleanseComponent`의 server RPC에서 처리

동작 흐름:

```text
Diana IA_Cleanse input
-> UDianaCleanseComponent mode별 server RPC
-> server validates Diana role, action lock, hack UI state, cooldown, mode별 aim source, range, 60도 cone, LOS
-> ISpecialInteractableInterface::Execute_Interact(Target, Interactor)
-> ADeadFilamentObstacle::Interact_Implementation
-> CanBeCleansedBy
-> Destroy
-> actor removal replication
```

수정 시 주의:

- 일반 상호작용 키와 섞지 않음
- client에서 장애물을 직접 `Destroy()`하지 않음
- `ADeadFilamentObstacle::Interact_Implementation()`에서 Cleanse component guard를 제거하지 않음
- 정화 스킬 판정은 화면 중앙 pixel radius가 아니라 camera forward와 target 방향의 3D angle로 유지
- 메시/머티리얼은 C++ hard load 없이 BP에서 지정

검증 방법:

- BP 자식 생성 후 `DeadFilamentMesh`에 준비된 static mesh 지정
- Diana `E` 또는 `Gamepad_LeftShoulder` 입력으로 3000cm, 60도 cone, LOS 안 장애물 제거 확인
- 3000cm 밖, cone 밖, LOS 차단 대상이 제거되지 않는지 확인
- 일반 `Interact()` 경로로는 제거되지 않는지 확인

## Player Teleport Trigger

현재 구조:

- `APlayerTeleportTrigger`는 메시 없이 `TeleportTrigger` BoxComponent만 사용
- `TargetLocation`은 BP/레벨 배치 actor Details에서 지정
- server overlap에서 player-controlled pawn만 `TeleportTo`로 이동
- 몬스터나 일반 actor overlap은 무시

사용 방법:

- `APlayerTeleportTrigger`를 상속한 BP 생성
- BP 또는 배치 actor에서 `TeleportTrigger` 크기 조정
- `TargetLocation`에 이동할 월드 좌표 입력
- listen server/client에서 플레이어 overlap 후 같은 위치로 이동 확인

## Axis Movable Prop

현재 구조:

- `AAxisMovableProp`은 `AActor`로 이동 행동만 담당
- `InteractionActor` ChildActorComponent의 `Child Actor Class`에 `APropInteractionBase` 계열 class를 지정
- BeginPlay에서 ChildActor를 꺼내 delegate 구독
- `APropInteractionBase::OnInteractionActivated` delegate를 구독
- 일반 상호작용은 `APropInteractionBase::Interact_Implementation()`에서 delegate broadcast
- 퍼즐 상호작용은 server `APuzzleInteractable::CompletePuzzle()`에서 delegate broadcast
- `AAxisMovableProp`은 `APropInteractionBase`나 `IPuzzleTargetInterface`를 구현하지 않음
- BP 자식 생성 후 `SceneRoot` 아래에 `StaticMeshComponent` 또는 `SkeletalMeshComponent` 추가
- `AElevator`, `ABoxMovableProp`은 `AAxisMovableProp`을 상속한 용도별 leaf class
- `APingPongAxisMovableProp`은 `AAxisMovableProp`을 상속해 server timer로 목표 상태를 자동 왕복 토글

현재 흐름:

```text
Diana/Hugh interaction
-> InteractionActor child actor::Interact_Implementation
-> APropInteractionBase::OnInteractionActivated
-> AAxisMovableProp::HandleInteractionActivated
-> server SetMoved
-> bIsAtTarget replication
-> client OnRep_IsAtTarget
```

자동 왕복:

```text
APingPongAxisMovableProp::BeginPlay
-> server StartAutoMove
-> SetMoved(!IsMoved())
-> 부모 이동/복제 경로
-> MoveDuration + WaitAtEndsDuration 뒤 재토글
```

문제:

- 맵 장치를 수직/좌우 등 한 축으로 이동시키는 프롭 필요
- 이동 방향과 거리는 Editor에서 actor별로 조절 필요
- 사용자 상호작용 액터와 실제 이동 액터 분리 필요
- listen server와 client가 같은 목표 위치와 이동 상태를 봐야 함

핵심 위치:

- `AAxisMovableProp`
- `ABoxMovableProp`
- `APingPongAxisMovableProp`
- `APropInteractionBase`
- `APuzzleInteractable::CompletePuzzle`

수정 시 주의:

- `MoveDirection`은 0 벡터면 `UpVector` fallback
- 이동 기준점은 `BeginPlay()`의 actor 위치
- 배치 후 열린 상태로 시작하려면 `bStartMoved=true`
- 자동 왕복은 `MoveDuration`을 한쪽 이동 시간으로 사용하고 `WaitAtEndsDuration`만큼 끝점에서 대기
- runtime에서 방향/거리까지 바꾸는 기능이 필요하면 서버 값 복제와 재보간 타이밍을 같이 검토

검증 방법:

- BP 자식 생성 후 `SceneRoot` 아래에 `StaticMeshComponent` 또는 `SkeletalMeshComponent` 추가
- BP 뷰포트의 `InteractionActor` ChildActorComponent 선택
- `Child Actor Class`에 `APropInteractionBase` 자식 class 지정
- 퍼즐 완료로 움직일 경우 `Child Actor Class`에 puzzle node actor class 지정
- `MoveDirection=(0,0,1)`, `MoveDistance=300`으로 수직 이동 확인
- 2-player listen server에서 상호작용 후 양쪽 화면 위치 일치 확인
- `MoveDuration=0`이면 즉시 이동 확인
- `APingPongAxisMovableProp` BP에서 `bAutoMoveOnBeginPlay=true`, `MoveDuration=2`, `WaitAtEndsDuration=1`로 왕복 확인

## Boss Route Elevator

현재 구조:

- `ABossRouteElevator`는 보스전 이동 구간의 stage progression actor
- `APuzzleInteractable::TargetActors`에 elevator actor를 넣으면 puzzle 완료 시 `OnPuzzleTriggered()` 호출
- elevator 아래 child actor로 붙은 `APuzzleInteractable`은 BeginPlay에서 자동 수집하고 완료 delegate 구독
- `Stages` 배열 순서대로 `MoveDistance`, `MoveDuration`, `bRequiresCombatClear` 처리
- 이동은 server tick에서 계산하고 actor movement replication으로 client에 전달
- 전투가 필요한 stage는 도착 후 `WaitingForCombat` 상태로 멈춤
- 전투 stage의 `RequiredCombatActors` 아래 `AMonsterBase`가 모두 사망하거나 destroy되면 자동으로 `NotifyCombatCleared()` 호출
- `ControlledPuzzleActors`에 중앙 puzzle node를 넣으면 이동/전투 중 `SetPuzzleInteractionEnabled(false)`로 상호작용 잠금
- 전투 클리어 후 다음 stage가 남아 있으면 `ResetPuzzleForNextAttempt()`로 완료 상태를 지우고 완료 애니메이션 역재생

현재 흐름:

```text
PuzzleInteractable::CompletePuzzle
-> TargetActors
-> ABossRouteElevator::OnPuzzleTriggered
-> Moving
-> OnStageMovementFinished
-> WaitingForCombat
-> RequiredCombatActors 내부 Monster OnDeath/OnDestroyed 감시
-> NotifyCombatCleared
-> ResetPuzzleForNextAttempt
-> 다음 stage 대기 또는 Completed
```

Blueprint 연결:

- `BP_BossRouteElevator` 생성 후 `ElevatorMesh`에 보유 mesh 지정
- 중앙 puzzle node의 `TargetActors`에 `BP_BossRouteElevator` 배치 actor 지정
- 중앙 puzzle node를 elevator child actor로 붙인 경우 `TargetActors` 생략 가능
- world에 따로 둔 puzzle node를 쓰는 경우 `ControlledPuzzleActors`에 같은 중앙 puzzle node 지정
- stage 1, 2는 `bRequiresCombatClear=true`
- stage 1, 2의 `RequiredCombatActors`에 배치된 enemy actor 또는 enemy를 담은 box/parent actor 지정
- 마지막 stage는 보스전 맵 도착용이면 `bRequiresCombatClear=false`
- `OnCombatStarted(StageIndex)`에서 스폰 박스 열기 또는 wave 시작
- 지정된 enemy 또는 지정된 parent actor 아래 enemy가 모두 사망하면 puzzle 자동 재활성화
- puzzle 재활성화 직전 완료 애니메이션 역재생으로 다시 잠금 상태 표시
- `OnPuzzleAvailabilityChanged` 또는 puzzle의 `OnPuzzleInteractionEnabledChanged`에서 잠금 표시 갱신

수정 시 주의:

- `MonsterAIController`, `MonsterBase`, `MonsterTerritoryActor`는 이 흐름에서 직접 수정하지 않음
- enemy 사망 즉시 진행해야 하므로 `UPragmataHealthComponent::OnDeath`와 `OnDestroyed`를 함께 감시
- world의 `Allow Repeated Puzzle Hacking`과 `Max Puzzle Hack Count`를 BeginPlay에 child puzzle actor로 전달
- 반복 해킹 puzzle만 남은 횟수가 있을 때 `ResetPuzzleForNextAttempt()`로 재무장
- 실제 사용 가능 여부는 `bInteractionEnabled`, `bIsSolved`, 남은 해킹 횟수로 함께 제어
- 다음 해킹을 시작하려면 `UMapHackingPuzzleComponent`도 `CancelHack()`으로 `Idle` 상태로 되돌림

## Hugh WaistArm and Diana Mount Sockets

문제:

- Diana avatar, Diana IK target, Hugh waist arm이 서로 다른 mesh/socket owner를 사용
- `WaistArmMesh`를 잘못 reparent하면 Blueprint override가 깨지고 mesh invisible 또는 `Template Mismatch`가 발생
- `HipCarrySocket` 누락 시 waist arm attach가 root fallback으로 가면 위치 기준이 흔들림

현재 해법:

- `UWaistArmComponent`가 `WaistArmMesh`를 child component로 소유
- `AHughCharacter::AttachWaistArm()`은 `WaistArmComponent`를 Hugh mesh `HipCarrySocket`에 attach
- `HipCarrySocket`이 없으면 `Hip` bone fallback을 사용하고 로그로 원인을 남김
- Diana avatar mesh는 평상시 Hugh body `DianaMountSocket_root`에 attach한다. Hugh death 시 world transform을 유지해 detach하여 death animation의 root 이동을 허용하고, checkpoint respawn reset은 `SetDianaAvatarDeathAttachment(false)`로 normal socket에 재결합한다.
- Diana paddle IK sockets는 `WaistArmMesh`, hand/side handle sockets는 Hugh body mesh에서 조회

동작 흐름:

```text
AHughCharacter
-> WaistArmComponent
-> WaistArmMesh child 유지
-> HipCarrySocket attach
-> DianaAnimInstance socket lookup
-> ABP_Diana IK target
```

핵심 위치:

- `AHughCharacter::AttachWaistArm`
- `UWaistArmComponent::EnsureWaistArmMeshAttachment`
- `UDianaAnimInstance::UpdateIKTargets`
- `BP_HughCharacter > WaistArmComponent > WaistArmMesh`
- `SK_Hugh`, `SK_Hugh_WaistArm`

수정할 때 주의:

- `WaistArmMesh`를 Hugh mesh에 직접 붙이지 않음
- `WaistArmMesh` skeletal mesh assignment는 `BP_HughCharacter` override와 함께 확인
- `HipCarrySocket`은 Hugh body skeleton 쪽 socket
- paddle socket은 `SK_Hugh_WaistArm`, handle socket은 `SK_Hugh` 기준으로 확인

검증 방법:

- PIE log에 `No SkeletalMesh for Component(WaistArmMesh)` 없음
- `Missing socket or bone: HipCarrySocket` 없음
- Diana avatar가 Hugh 등에 고정
- Diana toe/hand IK alpha가 socket 존재 여부에 맞게 0/1 전환

관련 문서:

- `Docs/Architecture/architecture.md`의 `WaistArm attachment 보정`, `Diana Mount Socket / IK`
- `Docs/Operations/blueprint-tasks.md`의 `Diana Mount Socket / IK`

## Diana Scan, Hack, and Node Interaction

문제:

- client가 camera origin/direction이나 target intent를 그대로 보내면 hack/scan 판정 조작 위험
- Diana가 Hugh와 너무 멀거나 화면 중앙 밖 대상을 scan/hack/node interaction할 수 있음
- monster hack, map hack, puzzle node complete가 같은 Diana 입력 경계 안에서 섞일 수 있음

현재 해법:

- client input은 RPC 요청만 보냄
- 서버가 Diana authoritative camera intent를 다시 계산
- POI area scan은 Diana assigned role, action lock, cooldown, enemy combat state를 서버에서 검증
- `IPragmataScannable` 구현체는 display metadata를 반환하고, `FPragmataScanResult.TargetKind`로 `ItemBox`와 `MapHackNode`를 구분
- `FPragmataScanResult.DianaInteractionDistance`는 같은 POI 종류 metadata로 Diana interaction 거리도 전달
- POI 후보는 각 world의 `APragmataGameState` registry에서 수집하며, scan/interaction 시점에 월드 전체 actor 순회를 하지 않음
- POI registry는 현재 POI 수만 순회하므로 scan radius query와 Diana focus refresh 비용이 월드 actor 수에 비례하지 않음
- scan 반경은 `5000cm`, 표시 시간은 `5초`, 종료 후 cooldown은 `1초`
- scan은 Diana `Roaming` + non-combat 전용이며 `IsEnemyCombatActive()` 또는 `OperatorMode=Combat` 상태에서는 marker와 pulse를 모두 실행하지 않음
- POI reveal은 `Distance / ScanRadius * ScanDuration`으로 계산해 가까운 marker부터 표시
- POI marker는 Hugh와 Diana owning client에 `APragmataPlayerController::Client_ShowPOIScanResults()`로 전달하고 `APropInteractionBase.ScanIconComponent`의 Screen-space `WBP_ScanIconWidget`을 표시
- `UScanIconWidget`은 object-owned icon/distance 표시를 담당하며 HUD overlay, edge clamp, marker pooling을 사용하지 않음
- scannable 조건은 `APropInteractionBase::bCanBeScanned`가 source다. `APuzzleInteractable`/`AItemBoxInteractable`처럼 true인 actor만 GameState registry에 등록되고 Diana scan 결과로 client에 전달된다.
- scan icon은 `TargetActor`의 Screen-space `UWidgetComponent`로 표시하고, 마지막 wave reveal 뒤 `ScanDuration`만큼 유지한 뒤 공통 timer로 소등한다.
- scan 결과에는 `TargetActor`와 scan 시점 `TargetWorldLocation`을 함께 담아 client의 actor reference resolve 지연을 진단할 수 있게 한다.
- scan 성공 시 POI 개수와 무관하게 Hugh/Diana owning client에 `APragmataPlayerController::Client_PlayDianaScanPulse()`가 전달되어 1.5초, 이전 강도의 50% blue fullscreen post-process pulse를 재생
- scan pulse는 material blendable을 HUD처럼 덮지 않고 local `APostProcessVolume` color grading/bloom override로 red/green을 낮추고 blue gain/offset을 올려 씬 표면을 파랗게 밝힘
- scan pulse는 local-only 시각 효과이며 POI 판정, interaction 가능 여부, hack source로 쓰지 않음
- scan 진단 로그는 입력 도달, 서버 거부 사유, registry 후보 수, 반경 내 후보 수, 유효 POI 수를 분리하고, 반경 안 scannable 0개 상태는 local screen/log PrintString으로 표시
- opened `AItemBoxInteractable`과 solved 1회성 `APuzzleInteractable`은 invalid scan 결과로 숨김
- `APuzzleInteractable`과 `AInteractionPowerSupply`는 같은 `MapHackNode` POI로 취급
- `MapHackNode` interaction 거리는 `1500cm`, `ItemBox` interaction 거리는 `750cm`
- enemy hack은 Hugh anchor distance, line of sight, interface를 서버에서 검증하고 Hugh 방향은 보지 않음
- paired montage lock 중 scan/hack 거부
- monster hack 시작은 별도 hack 시작 버튼 없이 aim 중 reticle trace로 얻은 target actor를 자동 요청
- reticle line trace가 enemy collision을 직접 못 잡으면 reticle 주변 `ECC_Pawn`/`ECC_WorldDynamic` sweep에서 가장 중앙에 가까운 hackable actor를 후보로 사용
- line/sweep hit가 child actor에 맞아도 attached parent가 hackable이면 같은 target으로 해석
- sweep 후보도 visibility LOS를 통과해야 서버 거절 RPC 반복을 줄임
- enemy hack active 중 reticle 후보가 active target에서 0.15초 이상 벗어나면 Diana가 서버 cancel을 요청해 양쪽 HUD를 닫음
- cancel 요청은 active target당 1회만 보내고, 서버는 expected target이 현재 active target과 다르면 늦은 cancel로 보고 무시
- enemy hack ViewModel은 이전 cancel/complete animation finish가 새 active puzzle의 `bIsActive`를 false로 내리지 않도록 현재 bound puzzle state를 확인
- 같은 enemy 재시작도 Hugh HUD가 다시 열리도록 `ActiveEnemyHackTarget`과 별도 serial 복제를 함께 사용
- Hugh HUD처럼 이미 시작된 puzzle에 나중에 bind되는 화면은 ViewModel 동기화 때 `StartedTrigger`를 다시 토글해 표시를 복구
- enemy hack은 즉시 damage가 아니라 `UEnemyHackingPuzzleComponent` 시작 후 완료 시 `GetProjectedDamage()`와 `GetResolvedHackDurationSeconds()`를 서버 gameplay effect로 전달
- enemy hack grid에는 세션마다 `Decode` 노드 1개를 서버가 빈 칸에 배치하고, 시작 커서 `(0,0)`은 방문 판정이 불가능하므로 제외
- `UEnemyHackingPuzzleComponent::IsDecodeNodeVisited()`는 현재 경로 위에 `Decode` 노드가 있는지 나타내는 replicated live flag다. 백트래킹으로 노드에서 벗어나면 다시 false가 되며, 완료 시점 값이 이번 해킹의 decode 성공 여부
- `Decode` 노드의 실제 방어력 감소 적용은 gameplay 쪽에서 `IsDecodeNodeVisited()`를 읽어 결정하고, `WBP_HackCell`과 decode texture는 표시만 담당
- 완료 시 `UDianaHackComponent`는 damage/OverDrive를 먼저 처리하고 `ActiveEnemyHackTarget` null 복제를 `CompletedHackHudHoldSeconds`만큼 지연해 success animation 시간을 보장
- `UEnemyHackingWidget`은 ViewModel의 `OnHackCompletedWithDamage`를 받아 `OnHackCompleted()` BP event를 호출해 WBP 성공 애니메이션을 시작
- enemy hack 왼쪽 게이지는 `DoorGaugeRatio`에 바인딩한다. 완료 전에는 `GetEarnedHackDuration()`의 profile 예상값을 15초 상한으로 환산해 차오르고, 완료 후에는 `AMonsterBase::AppliedHackDuration`과 `HackedEndServerTime` 복제값으로 남은 시간을 계산해 줄어든다. UI-only timer는 gameplay timeout을 결정하지 않는다.
- 완료 후 노란색 처리는 `UEnemyHackingPuzzleViewModel::GetHackCompletedHighlight()` 또는 `GetCompletedTrigger()`를 WBP material parameter에 연결한다. C++는 완료 신호만 제공하고 소재/애니메이션은 WBP가 소유한다.
- hacked monster도 enemy hack puzzle 재시작 대상이며, repeat 완료는 enemy hack damage와 Diana OverDrive/success signal을 지급
- repeat 완료는 기존 `bHacked` timer를 reset/extend하지 않고, hacked-state 진입 효과는 최초 `false -> true` 전환 때만 실행
- hacked timer는 Watcher `6/15초`, Walker `2.06/6.71/15초`, Executor `1.10/4.55/9.10/15초`의 파란 노드 tier를 사용하며 초과 node는 마지막 tier로 clamp
- Sentinel boss는 `1.0/2.5/5.5/10.0/15.0초` tier를 사용해 4개 이상 파란 노드에서만 15초 약점 window를 제공
- `IPragmataHackable`의 actor 인자는 target이 아니라 `HackInstigator`이며, target actor는 interface 구현체 자신
- `bIsAiming`은 replicated bool로 유지하고 camera FOV/spring arm length를 local tick에서 보간
- `APragmataPlayerController` runtime duplicate fallback도 `RightMouseButton=IA_Aim`, `Tab=IA_Scan` 기준 유지
- `IA_Scan`은 `ADianaOperatorPawn::OnStartScan()`을 통해 POI marker scan만 실행
- `IA_Interact`는 `/Game/Blueprints/Player/Input/Diana/IA_Interact`, `LeftMouseButton`과 `Gamepad_RightTrigger`로 `IMC_Diana`에 매핑
- `IA_Cleanse`는 `/Game/Blueprints/Player/Input/Diana/IA_Cleanse`, `E`와 `Gamepad_LeftShoulder`로 `IMC_Diana`에 매핑
- `IA_Cleanse`는 `Started`, `Completed`, `Canceled`에 binding하고 scan/hack/node interaction target cache를 사용하지 않음
- `IA_Aim` hold 중 `IA_Cleanse` hold는 정밀 조준 mode로 preview를 표시하고 `IA_Cleanse` release 때 Diana camera 기준으로 실행
- `IA_Aim` 없이 `IA_Cleanse` press는 빠른 실행 mode로 Hugh camera/조준 방향 preview를 `0.5초` 표시한 뒤 release와 무관하게 자동 실행
- 정화 스킬은 cooldown `5초`, range `3000cm`, half-angle `30도`인 60도 3D cone 판정으로 실행
- 정화 스킬은 `UDianaNodeInteractionComponent`를 직접 호출하지 않고 같은 authority camera 재계산 패턴만 재사용한다. 각도 판정은 화면 중앙 pixel radius가 아니라 `acos(dot(CameraDirection, ToTarget)) <= 30도` 기준
- 정화 스킬은 enemy hack 또는 map hack UI active 중 거부해 기존 hacking 입력 흐름과 섞지 않음
- 정화 스킬 대상은 cone/LOS 안의 `ADeadFilamentObstacle`과 `AMonsterBase::bIsTainted == true`인 오염된 enemy이며, obstacle은 server `Destroy()`, enemy는 `CleanseTaint()`로 `bIsTainted=false` 처리
- `bIsTainted`는 `SM_Sentinel`, `SM_Walker_Tainted`, `SM_Executor_Tainted`의 taint material과 enemy hack grid의 Corrupted(보라색) node를 함께 제어한다. 정화 후 시작하는 grid에는 보라색 node가 없고, 지정 material slot은 `M_Invisible`로 유지된다.
- 정화 스킬 preview는 각 client의 Diana pawn replica에서 `M_CleanseConePreview` decal로 표시하며, mode별 source actor 아래 surface hit를 apex로 삼고 aim direction을 surface plane에 투영한 방향으로 3000cm/60도 fan을 펼침
- 정밀 조준 preview source는 Diana pawn이고, 빠른 실행 preview source는 Hugh character
- decal center는 `surface hit + forward * CleanseRange * 0.5`이고, Unreal decal extent 기준 `Y = CleanseRange * tan(CleanseHalfAngleDegrees)`, `Z = CleanseRange * 0.5`로 둠
- preview material은 decal UV가 아니라 `AbsoluteWorldPosition`과 dynamic material parameter의 apex/forward/right/range 값을 써서 player 위치에서 fan mask를 시작
- preview는 cooldown 중, enemy/map hack active, Diana action lock, 표면 hit 실패, material load 실패 때 숨기며 server cone/LOS 판정을 대체하지 않음
- `UDianaCleanseComponent::bCanUseCleanse`는 정화 스킬 cooldown readiness만 복제한다. cooldown 중 `false`, 5초 만료 후 `true`
- `UDianaCleanseComponent::GetCleanseCooldownProgress()`는 `elapsed / max` 기준이다. 사용 직후 `0.0`, cooldown 완료 시 `1.0`
- root HUD는 `UPragmataHUDViewModel`이 `UDianaCleanseComponent`를 구독하고 FieldNotify `bCanUseCleanse`, `CleanseCooldownProgress`로 UI에 전달한다. Widget Blueprint는 gameplay component를 직접 polling하지 않음
- enemy hack은 Diana `OperatorMode`와 무관하게 aim 중 reticle target으로 `UDianaHackComponent` enemy hack puzzle을 자동 시작
- `MapHackNode` interaction은 Diana assigned role의 `Roaming + non-combat + Aim`이고 `AreDianaActionsLocked() == false`인 상태에서 focus가 잡히면 aim-only로 node `Interact()`를 요청
- `ItemBox` 같은 non-MapHackNode POI interaction은 같은 gate를 통과한 뒤 `IA_Interact` 입력이 들어올 때만 node `Interact()`를 호출
- `APuzzleInteractable::Interact_Implementation()`은 `UMapHackingPuzzleComponent`가 있으면 map hack puzzle을 시작하고, `AItemBoxInteractable::Interact_Implementation()`은 Diana role만 item box open을 확정
- 해킹 UI active 중 방향 입력은 `W/A/S/D`와 gamepad face button `Top/Left/Bottom/Right`를 `EHackDirection`으로 변환
- aim 해제는 active enemy/map hack cancel 요청
- node interaction 후보는 local/server GameState POI registry에서 수집하고 `TargetKind`/`DianaInteractionDistance` metadata로 허용
- Diana local focus는 client world registry를 쓰고, 실제 interaction RPC는 server world registry로 같은 후보를 다시 판정
- node interaction 거리는 Hugh anchor 위치 기준 POI별 metadata를 사용하고, 화면 중앙 판정은 Diana camera ray가 실제 POI surface에 hit했는지로 결정
- `BP_ItemBox`와 `BP_CheckPoint`의 `AnimatedMesh`는 `BlockAll + QueryAndPhysics + Visibility Block`을 유지한다. prop collision이 player 이동을 막는 동시에 Diana interaction surface가 된다.
- `BP_BossRouteElevator` 안의 `ElevatorMesh` hit는 target이 아니며, `BP_PuzzleNode_Elevator` actor 또는 그 child surface hit만 map hack 후보가 됨
- `APuzzleInteractable`에 `UMapHackingPuzzleComponent`가 없으면 기존 `Interact_Implementation()`의 `CompletePuzzle()` 경로 유지
- `APuzzleInteractable`의 `PuzzleMesh`와 `CompleteAnimation`을 지정하면 `CompletePuzzle()`에서 완료 애니메이션 재생
- 완료 애니메이션은 `CompleteAnimationSerial` 복제로 client에서도 로컬 재생
- `AInteractionPowerSupply` 같은 non-puzzle interactable은 기존 `Interact()` 경로 유지
- 맵 해킹 퍼즐은 `APuzzleInteractable` 계열 노드에 `UMapHackingPuzzleComponent`를 붙이고, 완료 시 `CompletePuzzle()` -> `TargetActors` 경로로 문/장치를 트리거
- `APuzzleLockedDoor`는 완료 개수/필요 개수/lock mirror를 복제하고 `OnRep_PuzzleDoorProgress()`에서 BP progress event를 갱신
- `ADoorBase`는 서버가 계산한 `bReplicatedShouldBeOpen`을 복제하고 client는 `OnRep_ShouldBeOpen()`에서 문 열림/닫힘 애니메이션을 로컬 재생
- enemy/map 해킹 시작/방향/취소는 Diana 소유 actor의 Server RPC에서 호출하며, HUD는 `APragmataGameState`의 active actor 복제값을 보고 표시
- hacking ViewModel의 legacy Start/Stop 함수는 deprecated지만 기존 Blueprint 호환을 위해 bind/unbind와 start/cancel을 함께 수행
- 새 HUD 경로는 active actor 복제값을 보고 `BindToComponent()` / `UnbindFromComponent()`를 직접 호출
- enemy combat state는 monster AI target 획득/해제 기준으로 `APragmataGameState::bEnemyCombatActive`에 복제
- monster authoritative death는 `AMonsterBase::SetDead()`에서 Blueprint별 `LunafilamentReward`를 shared Hugh stats에 1회 지급
- checkpoint는 Diana `IA_Interact` click에서 local hit actor와 최신 camera direction을 reliable RPC로 보내고, server가 같은 direction으로 camera ray·거리·POI 등록을 재검증한 뒤 일치할 때 즉시 활성화한다. 입력 release는 취소 RPC를 보내지 않으며, 10초 death timer 뒤 active checkpoint 또는 PlayerStart에서 existing Hugh/Diana pawn을 reset
- combat state는 HUD event로 계속 전달한다. Diana의 `bEnemyCombatActive`와 `OperatorMode::Combat`은 tense idle 전환에 쓰지 않아 combat 중에도 normal follow idle을 유지
- `ActiveEnemyHackTarget`은 root HUD C++ 기본 경로가 `WBP_EnemyHackingWidget`의 `UEnemyHackingPuzzleViewModel::SetHackTarget()`으로 연결
- `ActiveMapPuzzleActor`는 root HUD `OnActiveMapPuzzleActorChanged(PuzzleActor)` BP event에서 `UHackingPuzzleViewModel::BindToComponent()`로 연결
- node hint/result는 Diana local top-left on-screen log만 사용

Executor/Walker hack visual:

- `AMonsterBase::bHacked`는 replicated hack 기준 상태이며, 최초 hack 적용 직후와 5초 timer 만료 시 `HandleHackStateChanged()`를 호출
- `AExecutorMonster`는 mesh를 `SM_Executor`로 유지하고, `bHacked` 상태에 따라 material slot 8/9/16만 교체
- hacked material은 `ch6200_00_BlueEmissive_01_NoStamp_Hacked`, `ch6200_00_Inside_01_NoStamp_Hacked`, `ch6200_00_Inside_02_NoStamp_Hacked`
- `AWalkerMonster`는 mesh를 `SM_Walker`/`SM_Walker_Tainted`로 유지하고, `bHacked` 상태에 따라 `ch5100_00_Inside`, `ch5100_90_Parts_Parts_Mat`, `ch5100_90_Frame_Body`, `ch5100_90_Frame_Head` slot name만 찾아 교체한다. Tainted mesh의 `DF`/`DF2` slot은 변경하지 않는다
- Walker hacked material은 `M_Walker_Inside_Hacked_Emissive`, `M_Walker_Parts_Parts_Mat_Hacked_Emissive`, `M_Walker_Frame_Body_Hacked_Emissive`, `M_Walker_Frame_Head_Hacked_Emissive`
- `bHacked == false`로 돌아오면 BeginPlay 이후 cache한 normal material을 같은 slot에 복원
- 두 Walker mesh의 `ch5100_00_Weapon_NoStamp` slot 기본 material은 `M_Invisible`이다. attack montage local 재생에 성공하면 원본 weapon material을 적용하고 montage 정상 종료·중단에서 다시 숨긴다
- weapon 표시는 damage authority와 분리된 local presentation이다. Walker는 각 peer의 `OnMontageStarted`에서 `AttackPatterns` montage만 필터링하고, `OnMontageEnded`와 `EndPlay`에서 숨김 상태를 복원한다
- `UAnimNotifyState_AttackHitWindow`는 socket sweep damage 구간만 소유하며 weapon material을 변경하지 않는다. 따라서 weapon은 짧은 hit window가 아니라 wind-up과 recovery를 포함한 공격 montage 전체에서 보인다
- normal/Tainted mesh의 slot index가 다르므로 weapon도 index를 하드코딩하지 않고 slot name으로 찾는다. 이 전환은 hack material slot 네 개와 Tainted `DF`/`DF2`를 건드리지 않는다
- 원본 weapon material과 `M_Invisible`은 C++ soft-load 대상이므로 `PAL_EnemyRuntime`의 `AlwaysCook` explicit asset으로 유지한다
- `SM_Walker_Hacked`는 material slot 비교용 reference mesh였으므로 runtime/복구 대상에서 제거
- Walker hacked material 4개는 C++ soft-load 대상이므로 packaging 전 `PAL_EnemyRuntime` explicit asset 포함 여부 확인 필요
- `HackDurationSeconds` 기본값은 5초이며, repeat hack 완료는 기존 duration을 연장하지 않음
- `AS_Executor_Hack_Open_Loop`는 원본 `AS_Executor_Add_2100_Hack_Body_Open`의 0번 프레임을 base pose로 쓰는 local-space additive single-frame open pose
- `ABP_ExecutorMonster`는 `DefaultSlot` 이후 포즈를 `ExecutorFinalPoseCache`로 저장하고, normal pose와 `Apply Additive` 결과를 `Blend Poses by bool`로 전환
- `Blend Poses by bool`의 active value는 `Event BlueprintUpdateAnimation -> TryGetPawnOwner -> Cast To MonsterBase -> bHacked -> bIsHacked`로 갱신
- hacked true로 전환될 때 blend time은 `0.5초`, additive 자체 alpha는 `1.0`
- flap bone 수가 많으므로 per-bone slot/filter를 만들지 않고, 공격 montage용 `DefaultSlot`은 그대로 유지
- C++ 쪽에는 AnimBP/Blueprint 조회용 `AMonsterBase::IsHacked()` getter를 둔다. material swap과 ABP overlay는 같은 replicated `bHacked` 상태를 기준으로 움직인다.

재타겟 깜빡임 원인/수정:

- 증상: 첫 enemy hack UI는 Hugh/Diana 양쪽에 정상 표시, 이후 crosshair 이탈 후 재조준 또는 aim 해제 후 재입력 시 UI가 1프레임 수준으로 뜬 뒤 닫힘
- 1차 원인: active enemy hack 중 reticle trace/sweep이 한 tick 흔들리면 Diana가 즉시 cancel RPC를 보내 새 puzzle을 닫을 수 있었음
- 1차 수정: reticle target lost가 `0.15초` 지속될 때만 expected target cancel RPC 전송, 서버는 expected target이 현재 active target과 다르면 stale cancel로 무시
- 잔여 원인: WBP cancel/complete animation finish callback이 이전 세션에서 예약된 뒤 새 target bind와 새 Active 복제 뒤에 도착해 `bIsActive=false`를 적용
- 최종 수정: `UEnemyHackingPuzzleViewModel`의 cancel/complete animation finish와 cancel delegate 처리에서 현재 bound puzzle state가 `Active`면 stale UI callback으로 보고 숨김/취소 연출 무시
- 주의: 정상 cancel은 `UEnemyHackingPuzzleComponent::CancelHack()`이 state를 `Idle`로 바꾼 뒤 broadcast하므로 위 가드에 막히지 않음

완료 애니메이션 경로:

- `UEnemyHackingPuzzleComponent::CompleteHack()`가 `OnHackCompleted` broadcast
- 서버 `UDianaHackComponent::HandleEnemyHackCompleted()`가 damage/OverDrive 1회 적용
- 양쪽 HUD의 `UEnemyHackingPuzzleViewModel::HandleHackCompleted()`가 final grid/cursor snapshot을 먼저 반영
- 이후 `HackCompletedHighlight=true`, `OnHackCompletedWithDamage` broadcast, `CompletedTrigger` 토글
- `UEnemyHackingWidget::HandleHackCompleted()`가 완료 후 countdown을 시작하고 `OnHackCompleted()` BP event 호출
- hold timer가 끝나면 서버가 `ActiveEnemyHackTarget=nullptr` 복제, 양쪽 HUD 해제

동작 흐름:

```text
Diana input
-> IA_Scan: RequestScan -> server RPC
-> Aim + IA_Cleanse hold/release: Diana camera 기준 정밀 정화
-> IA_Cleanse press without Aim: Hugh camera/조준 방향 기준 빠른 정화
-> Aim + Roaming + non-combat + MapHackNode focus: RequestNodeInteraction -> server RPC -> Interact_Implementation
-> IA_Interact + Roaming + non-combat + Aim + non-MapHackNode POI focus: RequestNodeInteraction -> server RPC -> Interact_Implementation
-> Aim + enemy reticle: FindHackTargetAtReticle -> RequestHack(target) -> server RPC -> enemy puzzle start
-> Hack UI active: W/A/S/D 또는 gamepad face button -> server RPC -> ProcessDirectionInput
-> Enemy puzzle complete: damage/OverDrive -> OnHackCompleted BP event -> HUD hold -> target clear
-> Aim release: force cancel server RPC
-> Enemy reticle target lost 0.15초: expected target cancel server RPC
-> authoritative camera origin/direction
-> POI scan은 server GameState registry 후보를 거리순으로 marker 표시, POI interaction은 local/server registry와 종류별 거리 metadata 검증, enemy hack은 Hugh distance/LOS 검증
-> POI marker, enemy hack damage, 또는 map puzzle CompletePuzzle
```

핵심 위치:

- `ADianaOperatorPawn`
- `UDianaScanComponent`
- `UDianaCleanseComponent`
- `UDianaHackComponent`
- `UDianaNodeInteractionComponent`
- `UMapHackingPuzzleComponent`
- `UHackingPuzzleViewModel`
- `UEnemyHackingPuzzleComponent`
- `UEnemyHackingPuzzleViewModel`
- `AInteractionPowerSupply`
- `APuzzleInteractable`
- `APragmataGameState`
- `APragmataPlayerController`
- `UScanIconWidget`

수정할 때 주의:

- client-sent camera 값을 gameplay 판정에 직접 쓰지 않음
- 정화 스킬도 client input은 intent만 보내고 서버에서 Diana role, lock, active hack state, mode별 origin/direction을 다시 계산
- 정밀 조준은 Diana aim 상태를 요구하고, 빠른 실행은 Hugh camera/조준 방향을 사용
- 정화 스킬 range는 무한히 늘리지 않음. 기본 `CleanseRange=3000cm`를 유지하고 tuning은 component default로 조정
- 정화 스킬 cone은 `CleanseHalfAngleDegrees=30도`를 중심으로 계산하므로 full angle을 바꿀 때 half-angle 값을 함께 확인
- 정화 스킬 preview도 같은 range/half-angle 값을 쓰므로 gameplay 값을 바꾸면 decal extent와 material fan mask가 함께 맞는지 확인
- preview는 `Multicast_SetCleansePreviewMode()`로 양쪽 client에 켜고, server cooldown 소비가 확정되면 `Multicast_StartCleanseCooldown()`으로 양쪽 client 숨김 시간을 맞춤
- cooldown UI는 replicated `bCanUseCleanse`와 server time 기반 progress getter를 `UPragmataHUDViewModel`로 동기화한다. UI 추가 시 ViewModel binding을 우선 갱신
- node interaction target actor도 client에서 받지 않고 서버가 다시 찾음
- POI scan result 표시와 POI interaction 실행을 같은 입력으로 묶지 않음. scan은 marker만 띄우고 `MapHackNode`는 aim-only, `ItemBox` 같은 non-MapHackNode POI는 `IA_Interact`가 담당
- scan metadata는 `IPragmataScannable::GetPragmataScanResult` 하나로 유지하고 POI별 별도 getter를 만들지 않음
- actor-owned `ScanIconComponent`는 visible 상태를 유지하고 실제 icon은 `UScanIconWidget::SetScanIconVisible()`로만 숨기거나 표시. component를 숨기면 내부 widget visibility를 켜도 렌더링되지 않음
- POI scan/interaction 후보 수집에 `TActorIterator`나 `GetAllActors`를 쓰지 않음
- gameplay 분류는 string `Actor Tags`가 아니라 `EPragmataScanTargetKind`와 typed metadata를 사용
- registry 등록은 server-only로 막지 않음. client registry는 focus 후보 cache이고 server registry는 RPC 재판정용 authority cache
- `TActorIterator`가 필요한 title/lobby/fallback 계열 코드는 POI scan/interaction 성능 경로와 분리
- enemy hack 완료 후 damage와 OverDrive 증가까지 서버에서 처리
- 완료 animation은 `OnHackCompleted()` BP event에서 시작하고, animation 길이가 바뀌면 `CompletedHackHudHoldSeconds` 기본값도 같이 조정
- 완료 trigger 전에 final grid/cursor snapshot을 반영해 Diana client가 마지막 trail 복제를 놓치지 않게 함
- 완료 hold timer는 target 일치 확인 후 null 처리해 새 hack session을 늦게 닫지 않음
- reticle 이탈 cancel은 expected target을 같이 보내 새 hack session을 늦은 cancel RPC로 닫지 않음
- cancel/complete animation finish callback은 현재 bound puzzle이 Active면 무시해 재타겟 UI 깜빡임을 막음
- enemy hack 서버 RPC는 Diana role, aim, Hugh 거리, LOS를 다시 검증하고 Diana OperatorMode/Combat state에는 의존하지 않음
- `AreDianaActionsLocked()` 조건을 scan/hack 양쪽에 유지
- POI scan result는 현재 hack source가 아님
- scan result cache 기반 enemy hack을 추가할 때도 서버 distance/LOS/interface 검증은 유지
- cached target을 만들면 target invalidation, stale UI clear, reticle hack path와의 우선순위를 명시
- props class는 `Interact_Implementation()` entrypoint를 보존하고 Diana 전용 진행 상태는 `UDianaNodeInteractionComponent`에 둠
- `ADianaOperatorPawn::RequestNodeInteraction()`은 click-required POI의 component 호출만 담당하고 Roaming/non-combat/Aim/role gate는 `UDianaNodeInteractionComponent::CanOwnerUseNodeInteraction()`에 둠
- `UDianaNodeInteractionComponent::RefreshFocusedNodeLog()`는 focus 후보 갱신 뒤 `MapHackNode`일 때만 자동 요청하고, `LastRequestedMapHackActor`로 focus 유지 중 RPC 반복을 막음
- 임시 node feedback은 billboard/actor component가 아니라 local on-screen log 기준
- 맵 해킹은 `APuzzleInteractable::Interact_Implementation()`에서 component 유무를 분기해, component 있음이면 map hack helper, component 없음이면 기존 `CompletePuzzle()` 경로
- `UMapHackingPuzzleComponent::TotalDepths`는 metadata와 `GenerateSequence()`에서 1~3으로 고정하며, 3보다 늘리려면 `UHackingPuzzleViewModel`과 WBP의 3-slot 표시도 함께 확장
- enemy/map hack `StartHack()`은 새 active session을 연 경우에만 `true`를 반환하고 caller는 상태 재조회 대신 이 결과로 bind 여부를 결정
- `CancelHack()`는 현재 reset trigger를 broadcast하지 않으므로 강제 취소 UI 정리는 별도 HUD 바인딩 해제 또는 cancel trigger 설계 필요

검증 방법:

- Hugh 기준 거리 안 대상은 Hugh 방향과 무관하게 enemy hack 성공
- POI scan은 Hugh 전방 cone 제한 없이 50m registry 후보만 표시
- 거리 밖, LOS 막힘, interface 없음 실패
- forced paired montage 중 scan/hack 실패
- client UI scan result 갱신 확인
- scan 후 reticle을 옮겼을 때 현재 hack이 어느 target을 쓰는지 확인
- MapHackNode는 Diana aim focus만으로 puzzle UI 또는 node 완료가 시작
- ItemBox는 Diana aim focus만으로 열리지 않고 `IA_Interact` 입력 후에만 open
- `RightMouseButton` aim camera zoom in/out 확인
- `Tab` scan 입력 확인
- `LeftMouseButton` 또는 `Gamepad_RightTrigger`로 Diana `IA_Interact` 입력 확인
- `Roaming`이지만 aim이 아니면 node interaction 실패
- `Roaming + Aim`, Hugh 1500cm 이내, 화면 중앙 10도 target이면 `Interactable: <NodeName>` 1회 출력
- `Roaming + Aim`에서 MapHackNode는 조준만으로 실행되고 ItemBox는 `IA_Interact` 입력 후에만 실행
- ItemBox는 Hugh 750cm 이내에서 Diana `IA_Interact`로만 열림
- node complete 후 owning Diana client에 `Done: <NodeName>` 1회 출력
- `Combat`에서는 node interaction 대신 enemy hack puzzle 경로 유지
- enemy hack active 중 Diana 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행
- enemy hack 완료 후 `ApplySuccessfulHack()`와 OverDrive가 1회 적용
- 맵 해킹 active 중 Diana 방향 입력이 서버 `ProcessDirectionInput()`으로만 진행
- Hugh/Diana HUD가 같은 depth, direction, success ring, reset/success trigger 표시
- 맵 해킹 완료 후 `TargetActors`의 문/장치가 양쪽에서 같은 상태로 반응

관련 문서:

- `Docs/Systems/Hacking/map-hacking-integration.md`
- `Docs/Architecture/architecture.md`의 `Role Input and Camera`
- `Docs/Systems/Server/server.md`의 `Diana Replication`

## Diana Mount Animation and Paired Montage

문제:

- Diana avatar는 Hugh 등에 붙어 따라가지만, Diana operator mode와 forced paired animation도 반영해야 함
- hard hit/death/shelter 중 Diana 독립 조작이 montage를 덮으면 연출과 gameplay state가 어긋남

현재 해법:

- `UDianaAnimInstance`가 Hugh movement와 GameState의 Diana pawn mode를 읽어 AnimBP state 변수 계산
- Diana operator pawn 조회는 `APragmataGameState::GetDianaOperatorPawn()`을 우선하고 PIE 초기화 fallback만 world scan을 사용
- toe/hand IK target은 C++에서 socket 위치를 Diana mesh local space로 변환
- hand IK target mode는 layer/follow state로 정하며, 현재 양손은 같은 mode를 사용
- `ABP_Diana` AnimGraph의 FABRIK/Slot node는 Blueprint asset에 유지
- EventGraph runtime IK 계산은 제거하고 native 변수 getter만 AnimGraph에 연결
- Diana hacking pose도 Hugh convention처럼 C++ asset hard reference 없이 native AnimInstance state만 읽음
- enemy hack 성공은 `ADianaOperatorPawn::EnemyHackSuccessSerial` 복제값으로 one-shot 전달
- `UDianaAnimInstance`는 serial 변화 시 `bHackSuccessRequested`를 짧게 열어 `ABP_Diana` success state 진입을 보장
- AnimInstance가 새 Diana pawn을 처음 관측할 때는 현재 success serial을 기준값으로만 저장해 과거 성공 애니메이션 재생을 막음
- `IA_Aim` 중에는 `UDianaAnimInstance::SetRightHandIKEnabled(false)`로 오른손 IK alpha를 0으로 유지
- `bDeathActive` 중에는 `UpdateIKTargets()`가 `ResetIKTargets()` 직후 return해 양손/양발 IK alpha를 모두 0으로 유지
- Diana death direction은 Diana가 직접 계산하지 않고 `UPairedAnimationComponent`가 Hugh death cue의 `Direction`을 그대로 사용
- `DA_PairedAnimationSet`은 `CueType=Death`, `Strength=Death`, `Direction=Front/Right/Back/Left` entry로 Diana death montage를 소유
- `AM_Diana_Damage_0811/0812/0813/0814_Dead_*_Long_*` montage는 `Enable Auto Blend Out=false`로 1회 재생 후 끝 pose를 유지
- paired cue server start time offset은 기존 `Montage_Play` start time으로 적용해 host/client의 death pose 진행도를 보정
- `ABP_Diana` Death state의 `AS_Diana_Damage_0811_Dead_F_Long_F` Sequence Player는 `Loop Animation` off인 front fallback으로 유지
- `ABP_Diana` `Main State Machine`은 `CombatIdle`, `HackingStart`, `HackingLoop`, `HackingEnd`, `HackingSuccess` 상태를 사용
- hair secondary motion은 `ABP_Diana` AnimGraph 끝단의 `RigidBody` node가 `/Game/Character/Diana/PA_Diana_Hair`를 `Override Physics Asset`으로 사용
- `PA_Diana_Hair`는 `PA_Diana` 복제본이며, hair chain body/constraint tuning은 Physics Asset Editor에서 별도 관리
- title showcase는 gameplay `ABP_Diana`에 state를 추가하지 않고 `/Game/Blueprints/Player/ABP_Diana_Title` 전용 AnimBlueprint를 사용
- `ABP_Diana_Title`는 title body loop 위에 facial title loop를 `Layered Blend per Bone`으로 합성하고, branch root는 `Neck_0`, blend depth는 `255`
- `Layered Blend per Bone`의 음수 `BlendDepth`는 `Neck_0` 하위 포함이 아니라 해당 branch blend를 막을 수 있으므로 title facial layer에는 쓰지 않음
- `ABP_Diana_Title`도 최종 blended pose 뒤에 `RigidBody` node를 두고 `/Game/Character/Diana/PA_Diana_Hair`를 override physics asset으로 사용
- `/Game/Map/Title`의 `Title_Diana_Showcase` actor는 `Animation Mode=Use Animation Blueprint`, `Anim Class=ABP_Diana_Title_C`
- hacking animation asset:
  - start: `AS_Diana_Attack_0100_Hacking_Start`
  - loop: `AS_Diana_Attack_0110_Hacking_Loop`
  - end: `AS_Diana_Attack_0150_Hacking_End`
  - success: `AS_Diana_Attack_0120_Hacking_End_Success_VerA`
- `FPairedAnimationEntry`는 cue, Hugh/Diana montage, play rate만 저장
- `UPairedAnimationSet::FindEntry()`는 cue+strength+direction, cue+strength, cue-only 순서로 montage fallback을 고름
- `UPairedAnimationComponent`가 server time 기반 paired montage cue를 복제
- forced cue 중 `APragmataGameState::bDianaActionsLocked`로 scan/hack/combat camera 차단
- `ADianaOperatorPawn::CanUseDianaActionIntent()`가 action lock 중 local scan/hack/reticle/direction intent를 component RPC 전에 차단
- death가 아닌 forced cue action lock은 현재 montage 길이가 아니라 `DefaultActionLockDuration` 고정 timer로 해제

동작 흐름:

```text
Hugh movement + Diana operator mode
-> UDianaAnimInstance state 계산
-> hand target mode 계산
-> ABP_Diana state machine / IK

Diana IA_Aim
-> ADianaOperatorPawn::bIsAiming replication
-> UDianaAnimInstance::bHackAimActive / CombatState
-> UDianaAnimInstance::bRightHandIKEnabled false
-> ABP_Diana HackingStart -> HackingLoop
-> aim release면 HackingEnd -> CombatIdle

Enemy hack success
-> UDianaHackComponent::HandleEnemyHackCompleted()
-> ADianaOperatorPawn::NotifyEnemyHackSucceeded()
-> EnemyHackSuccessSerial replication
-> UDianaAnimInstance::bHackSuccessRequested
-> ABP_Diana HackingSuccess
-> HackingLoop 복귀 후 aim 상태에 따라 loop 유지 또는 end

Server paired cue
-> LastCue replication
-> montage start offset 보정
-> forced cue면 Diana action lock

Title showcase
-> AS_Diana_Title_Loop_VerB body sequence
-> AS_Diana_Face_MiniDemo_md09000_Title_Loop_VerB facial sequence
-> Layered Blend per Bone from Neck_0 with positive depth
-> RigidBody hair physics with PA_Diana_Hair
-> Title_Diana_Showcase SkeletalMeshComponent output

Hugh death
-> AHughCharacter가 damage spec에서 death direction 계산
-> UPairedAnimationComponent::TriggerPairedCue(..., Direction)
-> LastCue replication
-> DA_PairedAnimationSet::FindEntry()가 Hugh cue direction에 맞는 Diana death montage 선택
-> Diana avatar mesh AnimInstance가 direction montage를 1회 재생하고 마지막 pose 유지
-> UDianaAnimInstance::UpdateDeathState()가 bDeathActive를 설정해 IK를 끄고 death root 전환 적용
-> UpdateIKTargets()가 ResetIKTargets() 후 return
-> paired death cue가 누락된 경우에만 ABP_Diana front Death fallback이 socket IK 없이 1회 재생
```

핵심 위치:

- `UDianaAnimInstance`
- `UPairedAnimationComponent`
- `UWaistArmComponent`
- `APragmataGameState`
- `/Game/Blueprints/Player/ABP_Diana`
- `/Game/Blueprints/Player/ABP_Diana_Title`
- `/Game/Map/Title`의 `Title_Diana_Showcase`

수정할 때 주의:

- `GetDianaOperatorPawn()`이 주 runtime 경로
- `TActorIterator` Diana lookup은 GameState pointer가 비어 있는 초기화/PIE fallback
- AnimBP는 native property를 읽고, socket/IK 계산은 C++가 담당
- `_0` suffix Blueprint duplicate variable을 만들지 않음
- AnimGraph skeletal-control node를 C++로 억지로 옮기지 않음
- forced cue와 일반 follow/combat layer 우선순위를 섞지 않음
- hair physics는 `SM_Diana` 기본 PhysicsAsset slot을 바꾸지 말고 `RigidBody` node override로만 연결
- `PA_Diana_Hair` tuning 시 hair root body는 kinematic, 중간 body는 simulated, adjacent hair body collision은 꺼서 흔들림 폭발을 먼저 막음
- title 전용 얼굴/목 연출은 gameplay `ABP_Diana` state machine에 섞지 말고 `ABP_Diana_Title`에서 `Neck_0` branch filter로만 분리
- `Title_Diana_Showcase`를 `AnimationSingleNode`로 되돌리면 facial loop와 hair physics가 동시에 적용되지 않음
- hacking success는 montage 직접 재생이 아니라 state machine one-shot state로 처리
- Diana death direction은 Hugh의 paired cue를 따라야 하며 Diana 쪽 collision/damage 계산을 추가하지 않음
- success signal window를 너무 짧게 줄이면 remote proxy가 transition을 놓칠 수 있음
- success transition priority는 aim-release end transition보다 높게 유지
- `HackingSuccess -> HackingLoop`는 sequence-player automatic rule이며, aim이 풀려 있으면 곧바로 `HackingLoop -> HackingEnd`로 빠짐
- right hand IK는 hacking pose 중 꺼야 하며 AnimGraph는 기존 `RightHandIKAlpha`를 그대로 사용
- death IK 차단은 Diana avatar detach가 아니라 alpha 0 유지로 처리
- death sequence player loop를 다시 켜면 terminal state라도 첫 frame으로 반복되므로 off 유지
- death montage는 `Enable Auto Blend Out=false` 유지
- animation별 hand target이 필요하면 montage name 추론이 아니라 data asset field로 명시
- forced cue lock duration을 고칠 때는 montage length, play rate, start offset을 함께 고려

검증 방법:

- Hugh idle/walk/jog/boost에 Diana mount locomotion 동기화
- jump/hover/death top-level transition 확인
- death clip 길이 이후 Diana pose가 첫 frame으로 loop하지 않는지 확인
- IK alpha가 socket 유무에 맞게 0/1 전환
- `ABP_Diana` EventGraph에 runtime IK 계산 노드 없음
- AnimGraph IK alpha/location pin이 native `UDianaAnimInstance` property getter 사용
- IA_Aim press 때 hacking start 1회 후 loop 유지
- IA_Aim 중 `RightHandIKAlpha == 0`, aim release 후 socket 유효 시 1 복귀
- death 진입 프레임부터 toe/hand/foot IK alpha가 모두 0 유지
- IA_Aim release 때 hacking end 1회 후 idle 복귀
- enemy hack success 때 success animation 1회 재생
- success 중 aim 유지면 loop 복귀, aim release 상태면 end/idle 경로로 빠짐
- Hugh 화면의 Diana avatar도 같은 success serial을 받아 같은 state로 전환
- hard hit/death/shelter cue에서 Diana action lock 확인
- jump/fall/land preview에서 hair가 뒤처졌다가 감쇠하며 복귀하는지 확인
- hair가 떨리면 damping 증가와 adjacent collision off부터 확인하고, 반응이 약하면 `ComponentLinearAccScale`을 조정
- `Title.umap` 재로드 후 `Title_Diana_Showcase`가 `ABP_Diana_Title_C`를 유지하는지 확인
- title preview에서 body loop는 유지되고 `Neck_0` 아래 neck/head/facial motion만 facial sequence를 따르는지 확인
- `UPairedAnimationSet` entry가 montage/play rate만 소유하는지 확인
- non-death forced cue lock 시간이 montage 길이와 어긋나면 `DefaultActionLockDuration` path부터 확인

관련 문서:

- `Docs/Architecture/architecture.md`의 `Diana Animation Blueprint`, `Paired Animation`
- `Docs/Operations/blueprint-tasks.md`의 `Diana Mount Socket / IK`, `Diana`

## Large Asset Git Hygiene

문제:

- Unreal binary asset은 쉽게 Git history를 비대하게 만듦
- Map prop mesh/texture/animation과 일부 character animation은 source 없이도 재수급 가능
- purge 후 다시 stage되면 develop history 정리 효과가 사라짐

현재 해법:

- 대형 재수급 asset은 일반 Git history에서 제외
- source asset은 작업 폴더에 남기고 Git index에서만 제거
- `Docs/Operations/unstaged-assets.md`에 외부 보관 대상과 복원 기준 기록
- `gd-upload/` bundle로 Google Drive 업로드용 구조 생성
- `Content/Map/Props*` 정리는 `origin/minwon` mirror 여부와 purge target list를 같이 확인

동작 흐름:

```text
large asset 발견
-> tracked 여부 확인
-> git index에서 제거
-> .gitignore exact/pattern 반영
-> Docs/Operations/unstaged-assets.md 기록
-> gd-upload bundle 갱신
```

핵심 위치:

- `Docs/Operations/unstaged-assets.md`
- `.gitignore`
- `.agents/skills/unstage/SKILL.md`
- `.agents/skills/gen-gd/SKILL.md`

수정할 때 주의:

- source asset 파일을 디스크에서 삭제하지 않음
- `.uasset`, `.umap`은 텍스트처럼 병합하지 않음
- purge/history 작업은 explicit user request 없이 수행하지 않음
- `Content/Map/Props/` 계열은 minwon/develop 기준과 혼동하지 않게 경로 단위로 확인
- purged `origin/develop` 위로 올릴 때는 merge보다 rebase/force-with-lease 정책을 먼저 확인
- old-history branch를 develop에 다시 merge하지 않음
- broad `git clean -ffdx`로 asset policy 문제를 해결하지 않음

검증 방법:

- `git status --short`에서 의도한 asset만 staged/unstaged인지 확인
- `git check-ignore -v <asset>`로 ignore rule 확인
- `Docs/Operations/unstaged-assets.md` row와 실제 `gd-upload/Content/...` 경로 대조
- 필요 시 `gd-upload/unstaged-assets-content.zip` 재생성
- history purge 후에는 `Docs/Operations/history-rewrite-team-runbook.md` 기준으로 branch ancestry와 purge target object 재유입 여부 확인

관련 문서:

- `Docs/Operations/unstaged-assets.md`
- `Docs/Operations/history-rewrite-team-runbook.md`
- `.agents/directives/12-unstaged-assets.md`

## Build and Engine Baseline

문제:

- engine version, module dependency, generated solution file 기준이 흐려지면 다음 작업자가 다른 환경에서 같은 문제를 다시 밟음
- Runtime module에 Editor-only dependency가 섞이면 package/build boundary가 깨질 수 있음

현재 해법:

- project baseline은 `Pragmata_2P.uproject`의 `EngineAssociation: 5.8` 기준
- Runtime module은 gameplay/runtime dependency만 유지하고 Editor-only 작업은 plugin/Editor path로 분리
- session/lobby runtime은 `OnlineSubsystemNull`, `OnlineSubsystemUtils`, `UMG`, `ModelViewViewModel`, `EngineCameras`, `AIModule` 계열 dependency를 명시
- generated solution/cache 산출물은 repo history에 넣지 않음

동작 흐름:

```text
engine/tooling 변경
-> .uproject EngineAssociation 확인
-> Build.cs runtime dependency 확인
-> Editor-only plugin은 TargetAllowList/Editor path로 제한
-> generated solution/cache 파일 stage 제외
-> README/AGENTS/directive baseline 갱신
```

핵심 위치:

- `Pragmata_2P.uproject`
- `Source/Pragmata_2P/Pragmata_2P.Build.cs`
- `.gitignore`
- `README.md`
- `AGENTS.md`
- `.agents/directives/02-unreal-code.md`
- `.agents/directives/05-architecture-pitfalls.md`

수정할 때 주의:

- `EngineAssociation`이 바뀌면 README와 agent directive의 engine baseline도 같이 갱신
- Runtime module에 `UnrealEd` 같은 Editor-only dependency를 넣지 않음
- `.sln`, `.slnx`, `Binaries/`, `Intermediate/`, `Saved/`는 generated/local state로 취급
- UnreelMCP는 Editor tooling 경로이며 gameplay runtime dependency로 옮기지 않음

검증 방법:

- `Pragmata_2P.uproject`의 engine/plugin 설정 확인
- `Pragmata_2P.Build.cs` dependency가 runtime/editor boundary를 넘지 않는지 확인
- `git status --short`에서 generated solution/cache 파일이 stage되지 않았는지 확인
- build 검증이 필요하면 full Unreal build는 사용자 명시 요청 후 실행

관련 문서:

- `README.md`
- `.agents/directives/02-unreal-code.md`
- `.agents/directives/05-architecture-pitfalls.md`
- `Docs/Operations/unreal-build-warnings.md`

## UnreelMCP and Editor Automation

문제:

- Blueprint/Widget/Map 작업을 C++로 억지 구현하면 runtime code가 asset layout 책임까지 떠안음
- Editor automation tool이 바뀌면 다음 agent가 어떤 경로로 자산을 고쳤는지 잃어버림

현재 해법:

- live Editor 작업은 UnreelMCP 지시문을 먼저 읽고 진행
- C++는 schema, runtime behavior, validation, replication 담당
- layout, named widget, map actor 배치, asset assignment는 Editor/Blueprint 쪽이 소유
- 자동화 결과와 수동 확인 항목은 `Docs/Operations/blueprint-tasks.md`에 남김
- commandlet/script가 nonzero로 끝나도 저장 검증이 성공하면 tool bootstrap 실패와 asset 작업 성공을 분리해 판단

동작 흐름:

```text
asset/UI/map 변경 요청
-> C++ boundary 확인
-> UnreelMCP 또는 manual Editor 경로 선택
-> asset 저장/컴파일
-> Docs/Operations/blueprint-tasks.md 갱신
-> gameplay smoke 또는 Editor 확인
```

핵심 위치:

- `.agents/directives/08-unreelmcp-tools.md`
- `.agents/directives/09-unreelmcp-operation-recipes.md`
- `Docs/Operations/blueprint-tasks.md`
- `Content/UI/Widget/WBP_RoleLobby`, `Content/UI/Widget/WBP_TitleScreen`
- `Content/Map/Title`, `Content/Map/MatchLobby`, `Content/Map/Demo`

수정할 때 주의:

- C++ fallback UI 생성 금지
- `BindWidget` 이름은 WBP와 C++가 동시에 맞아야 함
- command-driven Editor 작업은 저장/컴파일/검증 결과를 남김
- port bind 실패 같은 tool bootstrap 오류를 asset 저장 실패로 단정하지 않음
- Editor가 필요한 일을 runtime hard load나 constructor asset load로 우회하지 않음

검증 방법:

- target asset compile/save 확인
- binary/text inspection으로 target property 값 확인
- `git diff --stat -- <target asset>`로 의도한 asset만 바뀌었는지 확인
- map `WorldSettings.DefaultGameMode` 확인
- named widget bind 실패 log 확인
- PIE에서 Title, MatchLobby, Demo 주요 흐름 확인

관련 문서:

- `Docs/Operations/blueprint-tasks.md`
- `Docs/Architecture/architecture.md`의 `Editor 연결`
- `.agents/directives/05-architecture-pitfalls.md`

## AI Directive and Documentation Gate

문제:

- AI agent가 numbered directive를 자동으로 읽지 않으면 comment, docs, surgical change 규칙을 놓침
- 작업이 끝난 뒤 문서만 한 번 쓰면 iterative debugging 중 얻은 know-how가 사라짐

현재 해법:

- `AGENTS.md`에 prime directives와 loading matrix 유지
- Unreal C++ 변경 전 `02-unreal-code.md` 로드 강제
- human docs 작성 시 `11-human-documentation-style.md` 적용
- non-trivial 문제 해결/로직 개선 시 `Docs/KNOW_HOW.md` recipe 갱신
- 다른 개발자에게 설명할 문서는 먼저 용어를 쉬운 말로 풀고, 그 다음 원인과 runtime 흐름을 단계별로 설명
- callback, lifecycle, replication, timer처럼 헷갈리기 쉬운 개념은 "무엇이 원인인지"와 "무엇이 원인이 아닌지"를 분리해 기록

동작 흐름:

```text
task 수신
-> AGENTS.md prime directives 확인
-> task surface별 numbered directive 로드
-> 구현/검증
-> comment gate
-> docs/KNOW_HOW gate
-> final report 또는 ship
```

핵심 위치:

- `AGENTS.md`
- `.agents/directives/01-behavior.md`
- `.agents/directives/02-unreal-code.md`
- `.agents/directives/03-workflow.md`
- `.agents/directives/11-human-documentation-style.md`
- `Docs/KNOW_HOW.md`

수정할 때 주의:

- `AGENTS.md`에는 짧은 trigger와 matrix만 유지
- 세부 규칙은 numbered directive에 둠
- root `.md` 새 문서는 만들지 않음
- 단순 오타/한 줄 수정은 KNOW_HOW 갱신 불필요 사유만 final에 명시

검증 방법:

- `AGENTS.md`만 읽은 agent가 C++ task에서 `02`를 고르는지 smoke test
- session/Blueprint/asset/doc task별 loading directive가 과하지 않은지 확인
- `rg -n "KNOW_HOW|Documentation Gate"`로 directive hook 확인

관련 문서:

- `AGENTS.md`
- `.agents/directives/03-workflow.md`
- `.agents/directives/11-human-documentation-style.md`

## 몬스터 자세별 Body Sweep

문제:

- `ACharacter` 이동은 루트 Capsule만 sweep하므로 가로로 긴 자세의 머리·몸통·다리가 벽을 통과할 수 있음
- Mesh 자식 collision을 `Block`으로 설정해도 CharacterMovement의 이동 제한 shape로 사용되지 않음
- Physics Asset과 Mesh 자식 collision은 부모 이동 뒤 따라오므로 Hit가 발생해도 루트 Capsule 이동을 자동 rollback하지 않음

현재 해법:

- 모든 `AMonsterBase`가 `UPragmataMonsterMovementComponent` 사용
- sphere는 루트 Capsule 중심 기준 `LocalOffset`과 `Radius`로 설정
- 이동 전 활성 profile sphere를 현재 위치에서 목표 위치까지 sweep
- 가장 이른 body hit까지 루트 Capsule 이동을 제한하고 hit normal을 기존 `SlideAlongSurface` 경로에 전달
- BP profile과 Notify override를 같은 runtime stack 데이터로 처리
- `Movement Component Profile`은 BP의 `Body Sweep Profiles`에서 `ProfileName`으로 조회
- `Notify Override`는 Notify의 `Override Spheres`를 해당 montage 구간에 직접 적용
- `Default Body Sweep Profile Name`은 Notify 없이 지속 적용할 BP profile 지정
- `Drive Follow Sphere From Tracking Bone`은 선택 bone의 실제 pose 이동량으로 sphere를 구동해 수동 시간 보간 제거
- `Attach Follow Sphere To Tracking Bone`은 선택 sphere를 tracking bone 위치에 붙이고 `Tracking Bone Sphere Local Offset`으로 세부 위치만 조정
- bone attach 모드는 Notify 시작 frame에 sphere 기준 위치를 현재 pose의 bone+offset으로 즉시 맞추고 이후 이동만 sweep
- `Animated Profile Offset`은 bone 자동 추적이 맞지 않는 연출만 sphere별 시간 기반 선형 구간으로 수동 추적
- offset 변경도 이전 위치에서 새 위치까지 sweep하며 벽에 막히면 root Capsule 대신 sphere를 충돌 지점에 정지
- profile 활성화 frame에는 root Capsule을 강제로 밀지 않으며, pose 때문에 이미 겹친 sphere는 이후 이동을 영구 차단하지 않음
- `Constrain Mesh Motion To Body Sweep`은 tracking bone과 정지한 sphere의 오차만큼 Mesh를 역보정하고 animation 복귀량만큼 자동 해제
- BP에서 추가한 머리·몸통·다리 collision은 이동 충돌에 유지하되 NavMesh 생성에서는 제외

Editor 설정:

- BP profile은 `Character Movement > Body Sweep Profiles`에서 구성
- 확인할 BP profile의 `Preview in Editor` 활성화
- Blueprint 또는 Level Editor viewport의 `Realtime` 활성화 상태에서 wire sphere 확인
- Montage Notify의 `Preview Monster Class`에 대상 monster BP 지정
- `Movement Component Profile`은 BP profile 이름과 같은 `ProfileName` 지정
- `Notify Override`는 BP profile 없이 `ProfileName`, `Override Spheres`, `Override Preview Color` 지정
- bone 이동을 그대로 충돌 경로로 사용할 때 `Use Animated Profile Offset` 비활성화
- `Constrain Mesh Motion To Body Sweep`과 `Drive Follow Sphere From Tracking Bone` 활성화
- `Mesh Follow Sphere Index`에 bone과 대응하는 sphere index 지정
- `Mesh Tracking Bone Name`에 실제 pose 이동을 측정할 bone/socket 지정
- sphere 시작점을 발·손 bone에 자동으로 붙일 때 `Attach Follow Sphere To Tracking Bone` 활성화
- `Tracking Bone Sphere Local Offset`으로 bone 기준 앞·뒤·위치 보정
- 전후 이동만 추적할 때 `Mesh Compensation Axis Mask = (1, 0, 0)` 사용
- 자동 추적이 맞지 않는 연출에서만 `Use Animated Profile Offset` 활성화
- `Animated Spheres`에서 sphere별 `Radius`, `Start Local Offset`, `Max Local Offset`, `Return Local Offset` 지정
- 시작 위치와 복귀 위치가 달라도 각각 지정한 위치를 유지
- `Notify Override` 활성화 시에도 시작 pose의 기존 겹침으로 root Capsule 위치를 바꾸지 않음
- `Movement Component Profile` source에서는 sphere index 순서로 위치를 적용하고 `Radius`는 BP profile 값 사용
- `Offset Start Delay`, `Move To Offset Duration`, `Hold Offset Duration`, `Return To Base Duration` 설정
- 복귀가 끝난 뒤 각 `Return Local Offset`을 유지해 같은 Notify의 나머지 root motion 구간을 추적
- PIE에서는 `Draw Active Body Sweep in Game`으로 현재 활성 profile 확인

핵심 위치:

- `Source/Pragmata_2P/Enemy/PragmataMonsterMovementComponent.h`
- `Source/Pragmata_2P/Enemy/PragmataMonsterMovementComponent.cpp`
- `Source/Pragmata_2P/Animation/AnimNotifyState_BodySweepProfile.h`
- `Source/Pragmata_2P/Animation/AnimNotifyState_BodySweepProfile.cpp`
- `AMonsterBase::AMonsterBase`

수정 시 주의:

- `Movement Component Profile`은 BP profile과 Notify의 `ProfileName`을 동일하게 유지
- `Notify Override`도 stack 제거 식별자로 사용할 고유 `ProfileName` 지정
- 뒤쪽 위치는 기본 actor forward 기준 각 sphere의 `Max Local Offset.X`를 더 작은 값으로 설정
- 네 시간 설정의 합이 NotifyState 길이를 넘으면 종료 전에 원위치로 복귀하지 못함
- animated offset 충돌은 root Capsule 위치를 바꾸지 않으며 일반 이동 중 profile sweep만 Capsule 이동량을 제한
- bone-driven sphere는 component-space pose delta를 사용하므로 이전 frame의 Mesh 역보정이 다음 목표 위치에 누적되지 않음
- bone attach 모드는 sphere 중심을 profile `LocalOffset`이 아니라 tracking bone pose와 bone local offset으로 매 frame 재계산
- bone attach 시작 위치는 sweep하지 않으므로 시작 pose에서 이미 벽 안에 박힌 배치는 offset·Notify 시작 시간을 조정
- 발 bone은 보행 cycle로 크게 왕복할 수 있으므로 해당 발이 실제 외곽 기준일 때만 사용하고 일반 자세는 `Spine` 또는 `Pelvis` 우선
- mesh constraint의 sphere offset 시간과 tracking bone 이동이 다르면 mesh가 보정 경로를 따라가므로 montage preview에서 두 움직임을 맞춰야 함
- Notify 종료 시 MeshComponent 상대 위치는 시작값으로 복구되므로 `Return Local Offset`을 종료 pose와 맞춰 snap 방지
- sphere 아래쪽이 바닥과 겹친 시작 penetration은 무시하지만 새로 진입하는 충돌은 막으므로 높이와 반지름은 계속 조정
- `ROLE_SimulatedProxy`는 일반 Capsule 이동 body sweep을 적용하지 않고, Capsule을 바꾸지 않는 animation-only sphere sweep만 로컬 시각 보정에 사용
- 항상 긴 체형이면 `Default Body Sweep Profile Name` 지정과 Nav Agent 크기 검토

검증 방법:

- Editor viewport에서 BP profile sphere가 원하는 위치와 크기로 표시되는지 확인
- Montage viewport에서 두 `Profile Source`의 sphere가 Notify 구간에 표시되는지 확인
- montage scrub 시 설정 시간대로 sphere가 이동·유지·복귀하는지 확인
- Notify 구간 시작·종료에 override sphere 충돌이 활성화·해제되는지 확인
- Notify를 0초에 배치하고 정적인 큰 sphere를 바닥과 겹치게 해도 Capsule이 튀거나 `MoveTo`가 영구 실패하지 않는지 확인
- root motion 없는 초반 구간에서 벽 접촉 시 Capsule은 고정되고 Mesh만 반대 방향으로 보정되는지 확인
- mesh constraint 사용 시 기준 sphere가 벽에서 멈춘 뒤 tracking bone과 mesh도 더 진행하지 않는지 확인
- animation이 반대 방향으로 복귀할 때 누적 Mesh 보정이 같은 이동량만큼 자연스럽게 해제되는지 확인
- bone-driven 모드에서 수동 시간 설정 없이 tracking bone과 sphere가 같은 경로로 왕복하는지 확인
- bone attach 모드에서 montage scrub 시 sphere가 tracking bone + local offset 위치에 붙어 보이는지 확인
- Notify 종료 후 MeshComponent가 원래 상대 위치로 복구되고 눈에 띄는 snap이 없는지 확인
- 벽 정면·후진·측면 이동과 제자리 회전에서 머리와 다리가 벽을 통과하지 않는지 확인
- 비스듬한 벽에서 기존 CharacterMovement slide가 유지되는지 확인
- listen server와 client에서 Capsule 위치가 갈라지지 않고 Mesh 벽 제한이 함께 보이는지 확인

## Cleanse 획득 prop

문제:

- `BP_DeadFilament_Box` 상호작용은 컷신 없이 정화 스킬, 튜토리얼, 기계 애니메이션을 한 번에 실행해야 한다.

현재 해법:

- BP는 `ACleanseSkillAcquisitionProp`로 reparent한다.
- acquisition prop은 scan target이 아니므로 scan registry에는 넣지 않는다. Diana camera ray가 prop을 직접 hit했을 때만 click-required interaction 후보에 추가한다.
- `BP_DeadFilament_Box`의 `AnimatedMesh`는 Blueprint 저장값과 무관하게 `BeginPlay()`에서 `QueryOnly`, `Visibility=Block`을 보장한다. 이 collision이 없으면 camera ray가 기계를 hit하지 못해 acquisition 진입 전 조용히 거부된다.
- 서버는 Diana role과 중복 획득을 검증한 뒤 `bAcquired=true`, `UnlockCleanse()`를 즉시 확정한다. `bAcquired` OnRep와 focus prompt 조건은 late join client에서도 완료된 기계의 안내를 숨긴다.
- Diana owning client는 이어서 `ShowTutorialOnce("CleanseAcquisition")`를 호출한다.
- Diana HUD는 `OnCleanseUnlockedChanged`를 받아 `PurificationPanel`을 표시하고 1초 focus pulse를 재생한다.
- HUD가 `NativeConstruct()`될 때 PlayerState 복제가 아직 끝나지 않았으면 `NativeTick()`에서 binding을 다시 시도하고, 연결된 뒤에는 delegate 기반 갱신만 사용한다.
- 마지막으로 `AAnimatedPropInteraction::Interact_Implementation()`을 직접 호출해 box animation serial을 복제한다. 기존 ItemBox reward 경로는 호출하지 않는다.
- scan 대신 local controller가 Hugh 위치에서 2000cm 이내인 prop-owned Screen widget을 보인다.
- `LS_CleanseAcquisition`은 `.uasset`으로만 남기고 획득 runtime에서 로드하거나 재생하지 않는다. Hugh transform과 player camera/input도 변경하지 않는다.

동작 흐름:

```text
Diana Interact
-> server bAcquired + UnlockCleanse
-> Diana tutorial + HUD panel focus
-> DeadFilamentBox interaction animation
```

검증 방법:

- PIE runtime에서 `AnimatedMesh`가 `QueryOnly`, `Visibility=Block`이고 Diana가 `Roaming + Aim`, Hugh 거리 750cm 이내에서 `IA_Interact`를 누르면 `[CleanseAcquisition] Cleanse unlocked without cinematic`이 기록되는지 확인한다.
- 한 번의 상호작용으로 `bCleanseUnlocked=true`, tutorial, HUD panel focus, box interaction animation이 실행되는지 확인한다.
- Hugh 위치, possession, camera, input이 상호작용 전후에 바뀌지 않는지 확인한다.
- `LS_CleanseAcquisition` asset은 남아 있지만 runtime code와 `LunarMap_T` 배치 액터가 참조하지 않는지 확인한다.
- 두 번째 상호작용은 unlock과 animation을 반복하지 않는지 확인한다.
- Hugh 위치가 2000cm를 넘거나 acquired 상태가 되면 object icon이 숨는지 확인한다.

## OverDrive player-owned cinematic

문제:

- OverDrive 연출을 map `ULevelSequence`에 묶으면 map 의존성과 Diana 전용 화면 조건이 섞인다.

현재 해법:

- `UOverDriveComponent` server RPC가 Diana role, full gauge, duplicate cast를 검증하고 action lock을 적용한다.
- Diana owning client만 현재 camera view target을 유지한 채 `AS_Diana_Grapple_0010_HacK_SP` body와 `AS_Diana_Face_Grapple_SPHack` face를 재생한다.
- `DianaSkillBody` slot 재생 중 오른손 IK를 끄고 종료 뒤 복구한다.
- 2초 timer 종료 뒤 server가 Hugh 위치를 snapshot하고 `AMonsterBase::ApplyTimedHackState(20.f)` 및 replicated mesh ring을 실행한다.
- ring은 `M_OverDriveRing`의 orange emissive material로 `RingHeightOffset=75cm`에서 cube mesh segment를 0~5000cm까지 확장한다.

검증 방법:

- Diana에서 keyboard `X`, 양 thumbstick을 0.25초 이내 눌러 full gauge cast가 한 번만 승인되는지 확인한다.
- OverDrive 시작과 종료 전후 Diana의 view target이 변하지 않고, Diana look/aim/hack/scan/cleanse/interact만 막히며 Hugh 조작은 유지되는지 확인한다.
- cinematic 종료 전 enemy hack/ring이 없고, 종료 직후 Hugh 중심 50m 안 enemy만 20초 hack되는지 확인한다.
- packaged build 전 `AS_Diana_Grapple_0010_HacK_SP`와 `AS_Diana_Face_Grapple_SPHack`이 `PAL_CoreRuntime`에 포함되는지 확인한다.

## BGM Region Actor

문제:

- 지역별 배경음을 맵에 배치한 범위 기준으로 재생해야 한다.
- 현재 플레이 구조는 두 플레이어가 항상 같은 장소에 있으므로 복잡한 전역 우선순위 매니저가 당장은 필요하지 않다.
- listen server와 client에서 같은 BGM이 중복 트리거되지 않도록 각 로컬 플레이어 기준으로만 재생해야 한다.
- 몬스터도 Pawn일 수 있으므로 BGM 시작 조건은 Hugh/Diana 플레이어 Pawn으로만 제한해야 한다.

현재 해법:

- `ABGMRegionActor`를 맵에 배치하고 `RegionTrigger` 크기를 지역에 맞춘다.
- `BackgroundMusic`에 WAV, SoundWave, SoundCue를 지정한다.
- Hugh 또는 Diana 로컬 컨트롤 Pawn이 BeginOverlap되면 `PlaybackDelay`만큼 기다린 뒤, 아직 Region 안에 있을 때만 `MusicAudioComponent`가 `FadeInDuration`, `TargetVolume`으로 재생된다. 기본값 `0`은 즉시 재생이다.
- delay 중 Region을 나가면 예약을 취소한다. 따라서 `bStopOnExit`가 꺼져 있어도 아직 시작하지 않은 지역 BGM이 범위 밖에서 뒤늦게 재생되지는 않는다.
- EndOverlap에서 `bStopOnExit`가 켜져 있으면 `FadeOutDuration`으로 페이드 아웃된다.
- EndOverlap 직후에는 `ExitRecheckDelay`만큼 기다렸다가 실제로 Hugh/Diana 로컬 Pawn이 Trigger 밖인지 다시 확인한다. 캐릭터 collision 흔들림으로 BGM이 끊기고 처음부터 반복 재생되는 것을 막기 위해서다.
- 기존 BP 인스턴스에 이전 기본값이 남아도 `BeginPlay`에서 collision, `Auto Activate`, AudioComponent 기본 볼륨을 보정한다.
- `RegionTrigger`는 Pawn 채널만 Overlap하지만, BGM 시작 조건은 `AHughCharacter` 또는 `ADianaOperatorPawn`으로 한 번 더 거른다. AI 몬스터 Pawn은 통과하지 않는다.
- `BeginPlay`에서 AudioComponent가 이미 재생 중이면 먼저 Stop하고, 현재 로컬 Pawn이 Trigger 안에 있을 때만 다시 재생한다.
- 2P client에서 Pawn possess가 늦어 `BeginPlay`의 기존 overlap 검사를 놓칠 수 있으므로, `InitialOverlapRetryInterval` 간격으로 `InitialOverlapRetryCount`만큼 로컬 Pawn overlap을 재검사한다.

동작 흐름:

```text
Player enters RegionTrigger
-> IsLocallyControlled Pawn check
-> PlaybackDelay timer
-> PlayRegionMusic
-> AudioComponent FadeIn

Player exits RegionTrigger
-> IsLocallyControlled Pawn check
-> StopRegionMusic
-> AudioComponent FadeOut

Player already inside RegionTrigger during client possession
-> BeginPlay overlap check may fail
-> InitialOverlapRetry timer
-> IsLocallyControlled Pawn check
-> PlayRegionMusic
```

접속 위치:

- `Source/Pragmata_2P/Props/BGMRegionActor.h`
- `Source/Pragmata_2P/Props/BGMRegionActor.cpp`
- `Docs/Intent/Props/BGMRegionActor.md`

설정 및 주의:

- BGM을 2D처럼 들리게 하려면 SoundWave 또는 SoundCue 쪽에서 attenuation/spatial 설정을 비공간 재생에 맞춘다.
- `PlaybackDelay`는 overlap 기반 시작에만 적용한다. Blueprint/Sequencer가 `PlayRegionMusic()`을 직접 호출하면 즉시 재생된다.
- 범위 밖에서도 시작부터 들리면 `MusicAudioComponent`의 `Auto Activate`가 BP에 남아 있는지, 또는 SoundWave/SoundCue가 다른 곳에서 직접 재생되는지 확인한다.
- 여러 BGM 영역이 겹치는 경우 현재 클래스는 우선순위나 다른 BGM 정지를 처리하지 않는다.
- 두 플레이어가 같은 지역에 있다는 전제를 벗어나면 PlayerController 또는 GameInstanceSubsystem 기반 BGM 매니저가 필요해질 수 있다.
- `BackgroundMusic`을 넣었는데도 안 들리면 AudioComponent `VolumeMultiplier`가 0인지, `RegionTrigger`가 플레이어와 실제로 겹치는지 먼저 확인한다.
- 2P에서만 늦게 들리거나 퍼즐 상호작용 후 들리면 `InitialOverlapRetryCount`를 일시적으로 늘려 possess/overlap 타이밍 문제인지 확인한다.

검증 방법:

- BP_BGMRegion 같은 Blueprint를 만들고 `BackgroundMusic`을 지정한다.
- `RegionTrigger`를 지역 크기에 맞게 조정한다.
- PIE listen server와 client에서 각각 지역 진입/이탈 시 페이드 인/아웃되는지 확인한다.

## Tutorial Popup and World Trigger

문제:

- 맵별 Level Blueprint에 overlap과 popup 호출을 복사하면 역할 판정, 영구 최초 1회, timer 종료가 서로 달라진다.
- 이전 timed trigger가 더 최근 tutorial을 닫거나, soft UI asset이 packaged build에서 빠질 수 있다.

현재 해법:

- 공통 로직은 `ATutorialTriggerActor`가 server authority에서 처리한다.
- 목적별 `BP_TutorialTrigger_<Name>` child는 Event Graph 없이 `TutorialStep`, stable `TutorialId`, Box, `ActivationRole`, 종료 정책만 기본값으로 가진다.
- title, preview image, description, key binding, `TargetRole`은 child에 복사하지 않고 `DA_Tutorial_<Name>`에서 편집한다.
- `ActivationRole`은 trigger를 밟을 역할, `TargetRole`은 popup을 볼 역할이다. 둘을 독립적으로 설정할 수 있다.
- `GameState`가 표시마다 non-zero `PresentationSerial`을 발급한다. 직접 client 표시도 subsystem의 높은 local serial을 발급한다. 각 성공 표시는 DataAsset `DisplayDurationSeconds`(기본 5초) timer가 같은 serial만 닫아 최신 popup을 보존한다.
- `ShowTutorialOnce(WorldContextObject, TutorialId, StepData)`는 어디서든 호출하는 단일 Blueprint API다. server 호출은 양쪽 peer로 multicast되고, 각 peer는 성공 표시한 ID만 `PragmataTutorialProgress` SaveGame에 기록한다.
- packaged/standalone은 `PragmataTutorialProgress` SaveGame으로 1회 표시를 유지한다. PIE는 disk slot을 읽거나 쓰지 않는 transient 이력을 사용하므로 PIE를 다시 시작하면 같은 ID를 다시 검증할 수 있다.
- `TutorialId`는 저장 키다. 출시 후 rename하거나 다른 tutorial에 재사용하지 않는다. 역할 불일치와 UI 초기화 실패는 저장 이력을 소비하지 않는다.
- `UntilReplaced`는 trigger actor의 server hide 정책이다. popup은 항상 DataAsset timer로 닫힌다. 현재 `LunarMap_T`의 여섯 공간 trigger는 별도 server hide도 요청하도록 모두 `Timed 5초`를 사용한다.
- `DA_Tutorial_WeaponsUse=Hugh`, `DA_Tutorial_Overdrive=Diana`, `DA_CleanseAcquisitionTutorial=None`이다. Cleanse acquisition은 server `ShowTutorialOnce()`로 multicast해 양쪽 peer에 표시한다.
- `bShowObjectiveAlertOnExit`은 tutorial popup 종료 정책이 아니다. WeaponUse의 첫 유효 EndOverlap에서만 server가 `Multicast_ShowObjectiveAlert()`을 호출해 양쪽 `WBP_ObjectiveAlertWidget`에 `발전소 전력을 복구한다`를 10초 표시한다.

제작 순서:

1. `DA_Tutorial_<Name>`을 만들고 title, image, description, key binding, `TargetRole`을 입력한다.
2. `BP_TutorialTriggerBase`의 child `BP_TutorialTrigger_<Name>`을 만든다.
3. child Class Defaults에 DataAsset, 고유 `TutorialId`, trigger 기본값을 지정한다.
4. 맵에 child를 배치하고 위치, Box transform, 필요한 인스턴스 override만 조정한다.
5. Config soft `WBP_TutorialPopup`과 각 soft preview texture를 `PAL_CoreRuntime` explicit asset에 넣는다. 동적 spawn/soft reference child와 DataAsset도 PAL 소유로 둔다.

동작 흐름:

```text
Player Pawn overlaps TriggerBox
-> server validates controller and ActivationRole
-> ShowTutorialOnce requests GameState PresentationSerial and multicast show
-> each local TutorialManager checks saved TutorialId and TargetRole, then initializes MVVM popup
-> successful local popup writes TutorialId to dedicated SaveGame slot
-> local DataAsset timer hides the same serial only
```

접속 위치:

- `Source/Pragmata_2P/Props/TutorialTriggerActor.h`
- `Source/Pragmata_2P/Props/TutorialTriggerActor.cpp`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`
- `Source/Pragmata_2P/Game/TutorialManagerSubsystem.cpp`
- `Source/Pragmata_2P/Game/TutorialBlueprintLibrary.cpp`
- `Source/Pragmata_2P/Game/TutorialProgressSaveGame.cpp`
- `Docs/Intent/Tutorial/TutorialTriggerActor.md`

검증 방법:

- child 두 개에 서로 다른 DataAsset을 지정하고 순서대로 진입해 popup 내용 교체를 확인한다.
- Hugh/Diana 전용 역할 matrix와 역할 불일치 시 영구 표시 이력 미소비를 2-player PIE에서 확인한다.
- 같은 `TutorialId` 재호출과 게임 재실행 뒤 재호출이 차단되는지 확인한다. 역할 불일치 peer의 ID는 저장되지 않아야 한다.
- PIE를 종료·다시 시작해 같은 `TutorialId`가 다시 표시되는지, standalone 또는 packaged 실행에서는 기존 1회 표시가 유지되는지 확인한다.
- WeaponUse를 나갈 때 양쪽 HUD에 objective alert가 한 번만 표시되고 10초 뒤 숨는지 확인한다.
- WeaponUse=Hugh, Overdrive=Diana, Cleanse=None role matrix에서 대상 peer만 또는 양쪽 peer가 5초 표시되는지 확인한다.
- timed A 뒤 B, 같은 DataAsset을 쓰는 C, cleanse legacy 표시 순서에서 A timer가 최신 popup을 닫지 않는지 확인한다.
- Blueprint compile/save/reopen 뒤 Class Defaults가 유지되는지 확인한다.
- PAL readback과 packaged smoke test에서 popup, preview image, RichText key icon 로드를 확인한다.
