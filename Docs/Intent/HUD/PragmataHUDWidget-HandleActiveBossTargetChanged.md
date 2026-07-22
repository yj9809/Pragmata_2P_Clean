# UPragmataHUDWidget::HandleActiveBossTargetChanged 의도 카드

## 대상
`UPragmataHUDWidget::HandleActiveBossTargetChanged`

## 종류

Root HUD boss HP bridge

## 정의 위치

- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`

## 코드가 존재하는 이유

GameState의 replicated boss target 변경을 역할별 root HUD 안의 boss HP child widget으로 전달하기 위해 존재한다.

## 사용해야 하는 상황

- PlayerController가 `ActiveBossTarget` 변경 delegate를 받은 경우
- `WBP_HughHUD`와 `WBP_DianaHUD`가 같은 boss actor의 HP를 표시해야 하는 경우

## 사용하면 안 되는 상황

- boss HP 값을 직접 계산하거나 gameplay state를 변경하는 경우
- 보스 actor가 직접 viewport widget을 생성하는 경우
- enemy 머리 위 `WidgetComponent` HP bar 처리

## 기본 흐름

`APragmataPlayerController::HandleActiveBossTargetChanged` -> `UPragmataHUDWidget::HandleActiveBossTargetChanged` -> `ResolveBossHPWidget()` -> `UBossHPWidget::SetBossTarget` -> `UBossHPViewModel::SetBossTarget`

## 호출하는 쪽

- `APragmataPlayerController::HandleActiveBossTargetChanged`

## 호출되는 쪽

- `UBossHPWidget::SetBossTarget`
- `UBossHPViewModel::SetBossTarget`
- `OnActiveBossTargetChanged` Blueprint event

## 서버/클라이언트 조건

client UI 처리. source state는 server replicated GameState.

## Blueprint 연결 여부

`BlueprintCallable`과 `BlueprintImplementableEvent` 경계가 있다. WBP에 `WBP_BossHPWidget` child가 없으면 Blueprint event에서 별도 처리할 수 있다.

`WBP_BossHPWidget` 안에는 View Models panel의 `BossHPViewModel` entry가 필요하다.

## 헷갈리기 쉬운 코드

- `HandleActiveEnemyHackTargetChanged`: enemy hack HUD bridge
- `HandleActiveMapPuzzleActorChanged`: map hack HUD bridge
- `ResolveWeaponWheelRoot`: HUD child 이름 fallback

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/BossHPViewModel.md`
- `Docs/Intent/HUD/PragmataGameState-ActiveBossTarget.md`
- `Docs/Intent/HUD/BossHPWidget.md`

## 확실한 근거

- `Docs/KNOW_HOW.md`에 boss HP가 root HUD 안의 고정 child widget으로 표시된다고 기록

## 추정된 의도

GameState 변경을 HUD Blueprint 내부 구조와 분리하는 C++ bridge로 두려는 의도.

## 확신도

강한 추정

## 아직 모르는 것

최종 `WBP_HughHUD`/`WBP_DianaHUD` 안에서 boss HP child를 어떤 anchor와 animation으로 배치할지.

## 검증 방법

```powershell
rg -n "HandleActiveBossTargetChanged|OnActiveBossTargetChanged|WBP_BossHPWidget" Source Docs
```

## 다음 수정 때 주의점

root HUD를 중복 생성하지 않는다. boss HP는 `AddToViewport` 대상이 아니라 root HUD 내부 child로 유지한다.
