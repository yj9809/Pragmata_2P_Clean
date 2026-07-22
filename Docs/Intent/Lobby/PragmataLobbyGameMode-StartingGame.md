# PragmataLobbyGameMode StartingGame 의도 카드

## 대상

`APragmataLobbyGameMode::RequestStartGame`

`APragmataLobbyGameMode::RequestSkinSelectionStep`

## 종류

양쪽 client에 시작 spinner를 복제한 뒤 gameplay map으로 넘어가는 lobby start phase

## 정의 위치

- `Source/Pragmata_2P/Game/PragmataLobbyGameMode.h`
- `Source/Pragmata_2P/Game/PragmataLobbyGameMode.cpp`
- `Source/Pragmata_2P/Data/PragmataTypes.h`
- `Source/Pragmata_2P/UI/PragmataRoleLobbyWidget.cpp`
- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.h`
- `Source/Pragmata_2P/Data/LobbyCharacterSkinSet.cpp`

## 이 코드가 존재하는 이유

host가 start를 눌렀을 때 host는 invite/copy 화면에 남고 joiner는 ready 화면에 남는 문제를 없애기 위해 존재한다. 시작 결정은 서버가 phase로 복제하고, UI는 그 phase만 보고 기존 lobby UI를 모두 숨긴다.

## 사용해야 하는 상황

- 두 player가 서로 다른 role을 선택하고 모두 ready인 상태에서 host가 start를 누를 때
- player가 현재 role의 skin을 상하 입력으로 바꿀 때
- `ServerTravel` 직전 양쪽 화면에 동일한 loading spinner가 필요할 때

## 사용하면 안 되는 상황

- session 생성/search/join 중 overlay를 대체할 때
- gameplay map load 완료 이후 HUD loading 상태를 표현할 때

## 기본 흐름

skin input -> 서버 role별 skin 유효성 검증 -> `LobbySelectedSkinId` 복제 -> both ready -> lobby phase `ReadyToStart` -> host button label `게임 시작` -> host start click -> server revalidation -> assigned role/skin 확정 -> `StartingGame` phase 복제 -> UI full-screen dimmer/spinner -> 짧은 delay 후 `ServerTravel`

## 이 코드를 호출하는 쪽

- `APragmataLobbyPlayerController::RequestLobbyConfirm`
- `UPragmataRoleLobbyWidget` ready/start button handler

## 이 코드가 호출하는 쪽

- `APragmataLobbyGameState::SetLobbyPhase`
- `APragmataPlayerState::SetLobbySelectedSkinId`
- `PragmataSessionUtils::BuildGameTravelURL`
- `UWorld::ServerTravel`

## 서버/클라이언트 조건

`RequestStartGame`은 server authority에서만 travel을 결정한다. `StartingGame` phase는 replicated lobby state로 client UI에 전달된다.

## Blueprint 연결 여부

`WBP_RoleLobby`에는 `SessionMatchDimmer`, `SessionLoadingSpinner`, `ReadyButtonLabel` bind가 있으면 C++가 바로 사용한다. 나머지 role status/description text는 optional bind로 점진 연결한다.

## 헷갈리기 쉬운 코드

- `EPragmataSessionPhase::Creating/Search/Joining`: network action overlay
- `EPragmataLobbyPhase::ReadyToStart`: host가 start를 누를 수 있는 lobby state
- `EPragmataLobbyPhase::StartingGame`: travel 직전 full-screen spinner state

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Architecture/architecture.md`
- `Docs/Intent/Lobby/PragmataRoleLobbyWidget-RefreshLobbyView.md`

## 확실한 근거

- `RequestStartGame`은 `StartingGame`을 set한 뒤 timer로 travel을 지연한다.
- `RequestSkinSelectionStep`은 role별 skin 목록 안에서만 `LobbySelectedSkinId`를 변경한다.
- `RefreshLobbyPhaseAndWarning`은 `StartingGame`이면 phase를 character select로 되돌리지 않는다.
- `UPragmataRoleLobbyWidget`은 `StartingGame`에서 기존 known widgets를 숨기고 overlay만 표시한다.

## 추정한 의도

loading spinner가 실제 map travel 전 복제될 시간을 확보해 listen server host와 join client가 같은 시작 피드백을 받게 하려는 의도.

## 확신도

강함

## 아직 모르는 점

0.35초 delay가 최종 packaged LAN 환경에서 충분한지 실제 2-player smoke로 확인해야 한다.

## 검증 방법

```powershell
rg -n "ReadyToStart|StartingGame|StartGameTravelDelay|RequestSkinSelectionStep|LobbySelectedSkinId|게임 시작" Source Docs
```

## 다음 수정 시 주의점

`ServerTravel`을 `StartingGame` phase set 직후 즉시 호출하면 client가 spinner phase를 보기 전에 map travel이 시작될 수 있다.
