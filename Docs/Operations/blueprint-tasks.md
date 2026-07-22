# Blueprint TODO

사용자가 Unreal Editor 또는 Blueprint에서 직접 처리할 작업만 기록한다.

## GameFlow

- [ ] `BP_GameFlow_Default` recompile/resave 후 class defaults에 `LobbyMap`, `LobbyGameMap`, `GameplayGameMode`만 남는지 확인
- [ ] Project Settings > Pragmata Game Flow > `GameFlowClass`가 `BP_GameFlow_Default`인지 확인
- [ ] Project Settings > Maps & Modes > Game Instance Class가 project custom GameInstance를 가리키지 않는지 확인
- [ ] `Title.umap` 시작 후 start 입력 시 `LobbyMap` 진입 확인
- [ ] `Demo`, `PlayerTestLevel` 등 임의 map 직접 PIE 시작 시 title GameFlow 자동 travel 없음 확인

## MatchLobby Presentation

- [ ] `SessionMatchDimmer`, `SessionLoadingSpinner`가 full-screen 최상위 overlay에 있고 다른 lobby panel의 child가 아닌지 확인
- [ ] host가 두 번째 player를 받는 동안 code/copy/close session UI 없이 `플레이어가 참가 중입니다...` + spinner만 보이는지 확인
- [ ] joiner가 session을 찾은 뒤 기존 UI 없이 `세션을 찾았습니다. 참가 중입니다...` + spinner만 보이는지 확인
- [ ] `PlayerJoining`에서 character select로 넘어갈 때 텍스트/스피너 없는 black empty screen 후 fade-in 되는지 확인
- [ ] `StartingGame` phase에서 dimmer/spinner 외 UI가 보이지 않는지 확인

## HUD / 해킹 UI

- [ ] `/Game/UI/Widget/WBP_HughHUD`에 `/Game/UI/Widget/MapHacking/WBP_MapHackingWidget` 배치
- [ ] `/Game/UI/Widget/WBP_DianaHUD`에 `/Game/UI/Widget/MapHacking/WBP_MapHackingWidget` 배치
- [ ] Diana HUD는 map hacking puzzle 전체 조작 UI로 layout 조정
- [ ] Hugh HUD는 map hacking puzzle 관전/요약 표시 기준으로 layout 조정
- [ ] `WBP_HughHUD`의 `OnActiveMapPuzzleActorChanged(PuzzleActor)` event에서 `PuzzleActor`의 `UMapHackingPuzzleComponent` 찾기
- [ ] `WBP_DianaHUD`의 `OnActiveMapPuzzleActorChanged(PuzzleActor)` event에서 `PuzzleActor`의 `UMapHackingPuzzleComponent` 찾기
- [ ] 찾은 `UMapHackingPuzzleComponent`를 `WBP_MapHackingWidget`의 `UHackingPuzzleViewModel::BindToComponent(Component)`에 연결
- [ ] `PuzzleActor == None` 또는 component 없음이면 `UHackingPuzzleViewModel::UnbindFromComponent()` 호출
- [ ] 맵 퍼즐 종료, 취소, 거리 이탈, actor null 전환 시 `UnbindFromComponent()` 호출
- [ ] `BP_HughCharacter` class defaults의 `GameplayHUDClass`가 `WBP_HughHUD`인지 확인
- [ ] `BP_DianaOperatorPawn` class defaults의 `GameplayHUDClass`가 `WBP_DianaHUD`인지 확인
- [ ] `WBP_HughHUD`/`WBP_DianaHUD` EventGraph에 별도 root HUD `Create Widget` 경로가 남아 있으면 제거

## Map Hack Node

- [ ] map hack node Blueprint가 `APuzzleInteractable` 기반인지 확인
- [ ] map hack node Blueprint에 `UMapHackingPuzzleComponent` 추가
- [ ] 이미 배치된 map hack node actor에도 `UMapHackingPuzzleComponent`가 실제로 있는지 확인
- [ ] component 없는 `APuzzleInteractable`을 map hack node로 쓰지 않도록 정리

## Combat HUD / Idle

- [ ] `WBP_HughHUD`의 `OnEnemyCombatActiveChanged(bool)`에서 combat용 HUD 요소 표시/숨김 연결
- [ ] `ABP_HughCharacter` idle 상태에서 `bEnemyCombatActive == true`이면 `Idle_Tension` 또는 `tense_loop` 계열 animation 사용
- [ ] `ABP_Diana` combat idle 상태에서 `bEnemyCombatActive == true`이면 `Idle_Tension` 또는 `tense_loop` 계열 animation 사용
- [ ] `bEnemyCombatActive == false` 복귀 시 기존 idle/locomotion animation으로 돌아오는 transition 확인

## Enemy / Boss

- [ ] 기존 `Content/Blueprints/Enemy/Sentinel/BP_Sentinel` parent가 `ASentinelMonster`인지 확인
- [ ] `Content/AI/BT_Sentinel` 생성, `BB_Monster` 사용 확인
- [ ] `Content/Blueprints/Enemy/Sentinel/ABP_Sentinel` 생성
- [ ] `BP_Sentinel`에 Sentinel mesh, attack pattern, death animation 설정
- [ ] `BP_Sentinel` scan text와 hackable 설정 확인

## Manual Verification

- [ ] 2-player listen server에서 host/client 각각 root HUD가 1개만 보이는지 확인
- [ ] Hugh 화면은 `WBP_HughHUD`, Diana 화면은 `WBP_DianaHUD` 기준으로 표시되는지 확인
- [ ] Diana가 map hack node를 aim하면 즉시 완료되지 않고 map hacking UI가 표시되는지 확인
- [ ] map hacking UI에서 `W/A/S/D` 입력으로 puzzle depth가 진행되는지 확인
- [ ] 성공/실패/취소 후 Hugh/Diana 양쪽 map hacking UI가 같이 정리되는지 확인
- [ ] `UMapHackingPuzzleComponent`가 있는 map hack node를 aim했을 때 즉시 `CompletePuzzle()`되지 않고 map hacking UI가 표시되는지 확인
- [ ] enemy가 Hugh/Diana를 target으로 잡으면 Hugh combat HUD 요소가 표시되는지 확인
- [ ] enemy target 상실 후 약 1초 뒤 Hugh combat HUD 요소가 숨겨지는지 확인
- [ ] Hugh/Diana idle이 combat 중 `tense_loop`, non-combat 중 기본 idle로 전환되는지 확인
- [ ] Diana IA_Aim press 때 hacking start 1회 후 loop 유지 확인
- [ ] Diana IA_Aim release 때 hacking end 1회 후 idle 복귀 확인
- [ ] enemy hack success 때 Diana success animation 1회 재생 확인
- [ ] success 중 IA_Aim 유지 시 Diana가 hacking loop로 복귀하는지 확인
- [ ] success 직후 IA_Aim 해제 상태면 Diana가 hacking end/idle 경로로 빠지는지 확인
