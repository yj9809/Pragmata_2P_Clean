# PragmataRoleLobbyWidget RefreshLobbyView 의도 카드

## 대상

`UPragmataRoleLobbyWidget::RefreshLobbyView`

`UPragmataRoleLobbyWidget::TriggerTitleView`

`UPragmataRoleLobbyWidget::TriggerSessionSetupView`

`UPragmataRoleLobbyWidget::TriggerWaitingForPlayerView`

`UPragmataRoleLobbyWidget::TriggerPlayerJoiningView`

`UPragmataRoleLobbyWidget::TriggerCharacterSelectView`

`UPragmataRoleLobbyWidget::TriggerStartingGameView`

`UPragmataRoleLobbyWidget::TriggerSessionFoundJoiningView`

`UPragmataRoleLobbyWidget::TriggerCharacterRevealView`

## 종류

Role lobby screen state, role status, skin label, description, loading overlay 표시

## 정의 위치

- `Source/Pragmata_2P/UI/PragmataRoleLobbyWidget.h`
- `Source/Pragmata_2P/UI/PragmataRoleLobbyWidget.cpp`

## 이 코드가 존재하는 이유

로비 UI가 host/join/session action, character select, ready/start, starting travel 상태를 한 화면에서 일관되게 숨기고 보여 주기 위해 존재한다.

## 사용해야 하는 상황

- session status가 바뀌었을 때
- replicated lobby state가 바뀌었을 때
- local skin prediction 또는 replicated skin id가 바뀌었을 때
- both ready 이후 host button label과 starting spinner를 갱신해야 할 때
- WBP 디자이너가 특정 lobby view 상태를 미리 보거나 Blueprint animation hook에서 명시적으로 전환해야 할 때

## 사용하면 안 되는 상황

- widget hierarchy를 C++에서 새로 생성할 때
- actor/world selection label을 갱신할 때

## 기본 흐름

session/lobby/player state 읽기 -> phase별 `Trigger...View` 호출 -> known widget visibility 갱신 -> role status text 갱신 -> skin label 갱신 -> description text 갱신 -> ready/start label 갱신 -> session/loading overlay 갱신

## 이 코드를 호출하는 쪽

- widget initialization
- controller/session/lobby state delegate callback
- public `Trigger...View` Blueprint 호출 경로

## 이 코드가 호출하는 쪽

- `Trigger...View`
- `BuildRoleStatusText`
- `UpdateRoleDescription`
- `UpdateSkinDescription`
- `UpdateReadyButtonLabel`
- `UpdateSessionMatchOverlay`

## 서버/클라이언트 조건

UI는 replicated `LobbyPhase`, player ready/selection state, local selection prediction을 읽는다. 서버 판정은 하지 않는다.

## Blueprint 연결 여부

`WBP_RoleLobby`는 `MatchmakingBackground`, `LobbyMatchmakingWidget`, `LobbyCharacterSelectWidget`을 optional bind로 받고, `ResolveSplitWidgetBindings`가 child 내부의 session/character control을 기존 state 로직에 연결한다. `ReadyButtonLabel`, `SessionOverlayStatusText`, `HughRoleStatusText`, `DianaRoleStatusText`, `RoleDescriptionTitleText`, `RoleDescriptionBodyText`, `SkinLabelPanel`, `SkinNameText`, `SkinCodeLabelText`는 optional bind라 WBP 갱신 중에도 C++가 동작한다. `RoleDescriptionTitleText`는 중첩 WBP 재편집 뒤에도 현재 widget tree의 같은 이름 TextBlock을 우선 찾아 선택 role에 맞는 이름을 표시한다.

`HughRoleStatusText`와 `DianaRoleStatusText`의 색상은 `BuildRoleStatusText`가 text와 함께 정한다. 내 선택은 파란색, 상대 선택은 빨간색, 선택 충돌은 노란색, 빈 역할은 흰색으로 표시한다.

`Trigger...View` 함수들은 WBP가 직접 호출해도 이전 view 조각이 남지 않도록 먼저 known lobby mode widgets를 숨긴 뒤 필요한 root만 켠다. layout, 색상, animation, z-order는 WBP가 소유한다.

`WBP_RoleLobby` layout은 `WBP_HughHUD`처럼 root `CanvasPanel`에 직접 child widget을 배치하고 Canvas slot anchor와 engine DPI scaling을 사용한다. 전역 design-size `ScaleBox` wrapper는 재도입하지 않는다.

`RoomCodeText`, `StatusText`, `CopyButton`, `DestroyButton`은 `WBP_LobbyMatchmaking` 내부 bind widget이다. SessionSetup과 WaitingForPlayer에서는 parent `LobbyMatchmakingWidget`과 `MatchmakingBackground`를 함께 표시한다. CharacterSelect와 ReadyToStart에서는 캐릭터 선택 UI만 남기기 위해 두 matchmaking widget을 함께 숨긴다.

## 헷갈리기 쉬운 코드

- `ShouldShowSessionMatchOverlay`: session action과 `StartingGame` 모두 overlay를 켜는 gate
- `Trigger...View`: 디자이너가 호출할 수 있는 view 전환 entry point이며 session state를 판정하지 않는다
- `ReadyButton`: ready와 start command를 같은 button이 phase에 따라 수행
- `StatusText`: `StartingGame`에서는 숨겨 spinner 뒤에 다른 UI가 보이지 않게 한다
- `SkinNameText`: replicated/predicted `LobbySelectedSkinId`의 UI label만 표시하고 서버 판정은 하지 않는다

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Architecture/architecture.md`
- `Docs/Intent/Lobby/PragmataLobbyGameMode-StartingGame.md`

## 확실한 근거

- `RefreshLobbyView`는 `StartingGame`에서 known UI widgets를 숨긴다.
- host + `ReadyToStart` phase에서 `ReadyButtonLabel`을 `게임 시작`으로 바꾼다.
- role status helper는 다른 player 선택/ready 여부를 role별 text와 색상으로 분리한다.
- skin helper는 `ULobbyCharacterSkinSet` DataAsset 또는 fallback 정의에서 label을 찾는다.
- `WBP_RoleLobby`, `WBP_LobbyMatchmaking`, `WBP_LobbyCharacterSelect`는 2026-07-09 Editor 재시작 후 compile/save 검증을 통과했다.

## 추정한 의도

WBP가 최종 layout과 style을 소유하고 C++는 state-to-widget binding만 담당하게 하려는 의도.

## 확신도

강함

## 아직 모르는 점

WBP 디자이너가 `Trigger...View`에서 추가 animation event를 붙일지, `OnLobbyPhaseChanged`만 사용할지는 아직 정하지 않았다.

## 검증 방법

```powershell
rg -n "RefreshLobbyView|Trigger.*View|BuildRoleStatusText|UpdateSkinDescription|UpdateReadyButtonLabel|SessionLoadingSpinner" Source Docs
```

## 다음 수정 시 주의점

`StartingGame`에서 parent panel을 통째로 숨기면 spinner까지 같이 사라질 수 있으므로 known child widgets를 개별로 숨기는 현재 방식을 유지한다.
