# UObjectiveAlertWidget 의도 카드

## 대상
`UObjectiveAlertWidget`, `UPragmataHUDWidget::ShowObjectiveAlert`, `APragmataPlayerController::Client_ShowObjectiveAlert`, `APragmataGameState::Multicast_ShowObjectiveAlert`

## 종류

Root HUD objective alert bridge

## 정의 위치

- `Source/Pragmata_2P/UI/ObjectiveAlertWidget.h`
- `Source/Pragmata_2P/UI/ObjectiveAlertWidget.cpp`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`
- `Source/Pragmata_2P/Game/PragmataPlayerController.h`
- `Source/Pragmata_2P/Game/PragmataPlayerController.cpp`
- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`

## 왜 코드가 존재하는가

새 목표 생성 판단과 HUD 표시를 분리하기 위해 존재한다. 게임 로직은 목표가 생긴 시점에 텍스트와 표시 시간을 넘기고, root HUD는 `WBP_ObjectiveAlertWidget` child에 전달해 화면 맨 위 알림만 표시한다.

## 사용해야 하는 상황

- 새 objective가 생성되어 Hugh/Diana HUD 위쪽에 일시 알림이 필요할 때
- 한 플레이어에게만 보일 목표 문구를 owning controller로 전달할 때
- 양쪽 플레이어에게 같은 목표 문구를 multicast로 보여줄 때
- `BP_TutorialTrigger_WeaponUse` 종료처럼 server가 확정한 map-local objective를 양쪽에 한 번 알릴 때

## 사용하면 안 되는 상황

- 퀘스트 진행 상태 저장
- objective 완료 조건 판정
- scan billboard나 POI marker 표시

## 기본 흐름

gameplay objective event -> `Client_ShowObjectiveAlert` 또는 `Multicast_ShowObjectiveAlert` -> local `APragmataPlayerController::ShowObjectiveAlert` -> root HUD `ShowObjectiveAlert` -> `UObjectiveAlertWidget::ShowObjective`

## 서버/클라이언트 조건

표시는 local HUD에서만 수행한다. `GameState` multicast는 저장 상태를 복제하지 않고, 각 클라이언트의 local controller에 표시 요청만 전달한다.

WeaponUse trigger는 server EndOverlap 검증 뒤 `Multicast_ShowObjectiveAlert(발전소 전력을 복구한다, 10.0f)`만 호출한다. 표시 횟수 gate는 trigger가 소유하며, 이 widget과 `GameState`는 objective 상태를 저장하지 않는다.

## Blueprint 연결 여부

`WBP_HughHUD`와 `WBP_DianaHUD` 안에 `UObjectiveAlertWidget` 기반 child를 `WBP_ObjectiveAlertWidget` 이름으로 배치해야 한다. child 내부 텍스트는 `ObjectiveText` 이름의 `TextBlock`으로 두면 C++가 직접 값을 채운다.

## 헷갈리기 쉬운 코드

- `UTutorialManagerSubsystem`: 튜토리얼 팝업용이며 새 objective 알림 저장/표시 경로가 아니다.
- `UScanIconWidget`: scan 결과를 대상 actor 위 in-world billboard로 표시하며 objective top alert와 분리한다.

## 검증 방법

```powershell
rg -n "ShowObjectiveAlert|ObjectiveAlertWidget|Multicast_ShowObjectiveAlert" Source Docs
```

## 다음 수정 시 주의점

objective 상태 자체를 이 위젯에 저장하지 않는다. 표시 시간이 끝나면 숨김 처리만 하며, 재표시는 게임 로직이 다시 호출해야 한다.
