# UEnemyHackingWidget Countdown 의도 카드

## 대상
`UEnemyHackingWidget`, `UEnemyHackingPuzzleViewModel::SetHackTimeRemaining`, `UEnemyHackingPuzzleViewModel::GetDoorGaugeRatio`, `UEnemyHackingPuzzleViewModel::GetHackCompletedHighlight`

## 종류

Enemy hack HUD countdown and completion highlight bridge

## 정의 위치

- `Source/Pragmata_2P/UI/EnemyHackingWidget.h`
- `Source/Pragmata_2P/UI/EnemyHackingWidget.cpp`
- `Source/Pragmata_2P/UI/EnemyHackingPuzzleViewModel.h`
- `Source/Pragmata_2P/UI/EnemyHackingPuzzleViewModel.cpp`

## 왜 코드가 존재하는가

enemy hack UI의 왼쪽 게이지와 남은 시간 숫자를 WBP/MVVM에서 읽을 수 있게 하기 위해 존재한다. 완료 후 노란색 처리도 실제 enemy gameplay state가 아니라 완료 delegate를 UI 표시 상태로 바꾸는 bridge로 둔다.

## 사용해야 하는 상황

- enemy hack UI가 열린 동안 기존 왼쪽 게이지를 진행률/남은 시간 겸용으로 표시해야 할 때
- 완료 후 grid/material을 노란색 하이라이트로 전환해야 할 때
- WBP에서 `DoorGaugeRatio`, `HackTimeRemainingText`, `HackCompletedHighlight` 값을 material parameter에 연결할 때

## 사용하면 안 되는 상황

- 서버 authoritative timeout 판정
- enemy damage 적용
- active hack target clear timing 변경

## 기본 흐름

GameState active enemy hack target -> root HUD -> `UEnemyHackingWidget::SetHackTarget` -> ViewModel `NormalProgressRatio` 갱신 -> `DoorGaugeRatio`가 파란 노드 진행률로 표시

완료 시에는 `UEnemyHackingPuzzleViewModel::HandleHackCompleted` -> `HackTimeRemainingRatio=NormalProgressRatio` -> `HackCompletedHighlight=true` -> `OnHackCompletedWithDamage` -> `UEnemyHackingWidget::HandleHackCompleted` -> local countdown tick -> `DoorGaugeRatio`가 완료 직전 채워진 게이지 값에서 0까지 감소

## 서버/클라이언트 조건

countdown은 local HUD presentation이다. 실제 timeout cancel이 필요하면 게임 로직 담당자가 서버 puzzle/component 쪽에 판정을 추가하고, 같은 ViewModel setter로 HUD 값을 동기화해야 한다.

## Blueprint 연결 여부

`WBP_EnemyHackingWidget`의 기존 왼쪽 `DoorGauge` Percent는 `EnemyHackingPuzzleViewModel.DoorGaugeRatio`에 바인딩한다. 같은 게이지를 쓰면 완료 전에는 `NormalProgressRatio`, 완료 후에는 완료 직전 `NormalProgressRatio`를 시작값으로 삼은 `HackTimeRemainingRatio`가 자동 선택된다.

남은 시간 숫자는 `HackTimeRemainingText`에 바인딩한다. C++ `BindWidgetOptional` 방식의 `HackCountdownGauge`, `HackCountdownText`, `HackCountdownFill` 직접 연결은 사용하지 않는다.

## 헷갈리기 쉬운 코드

- `UEnemyHackingPuzzleComponent`: puzzle 성공/실패 판정과 replicated grid state source다. countdown 표시 시간을 소유하지 않는다.
- `UDianaHackComponent::HandleEnemyHackCompleted`: damage/OverDrive/target clear를 담당한다. 노란 UI 색상 전환 자체를 소유하지 않는다.

## 검증 방법

```powershell
rg -n "DoorGaugeRatio|HackTimeRemaining|HackCompletedHighlight|HackCountdown" Source Docs
```

## 다음 수정 시 주의점

UI-only countdown이 0이 되어도 puzzle은 자동 취소되지 않는다. gameplay timeout을 추가할 때는 서버 RPC/authority 경계를 먼저 설계해야 한다.
