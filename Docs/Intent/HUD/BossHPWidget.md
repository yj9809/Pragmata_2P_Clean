# UBossHPWidget 의도 카드

## 대상
`UBossHPWidget`

## 종류

HUD-anchored boss HP widget

## 정의 위치

- `Source/Pragmata_2P/UI/BossHPWidget.h`
- `Source/Pragmata_2P/UI/BossHPWidget.cpp`

## 코드가 존재하는 이유

보스 HP를 월드 위젯이 아니라 Hugh/Diana 역할별 root HUD 안의 고정 위치에 표시하기 위해 존재한다.

## 사용해야 하는 상황

- `WBP_HughHUD`와 `WBP_DianaHUD` 안에 boss HP child widget을 넣는 경우
- `UBossHPViewModel`이 전달한 boss HP/groggy 상태를 HUD bar에 표시해야 하는 경우
- death 또는 target 해제 시 HUD 표시를 접어야 하는 경우

## 사용하면 안 되는 상황

- 일반 enemy 머리 위 HP 표시
- `UWidgetComponent` 기반 world/screen space HP 표시
- gameplay damage, phase, aggro 판정

## 기본 흐름

`SetBossTarget(BossActor)` -> `BossHPViewModel` 조회 -> `UBossHPViewModel::SetBossTarget` 호출 -> ViewModel UI event 수신 -> child `UEnemyHPBarWidget`과 선택 텍스트 갱신

groggy는 아직 자동 source를 정하지 않았다. `UBossHPViewModel::SetBossGroggyValue()` 또는 `SetBossGroggyPercent()`가 호출되면 ViewModel event를 통해 `BossGroggyBar` child의 `UEnemyGroggyGaugeWidget` material percent를 갱신한다.

## 호출하는 쪽

- `UPragmataHUDWidget::HandleActiveBossTargetChanged`
- 필요 시 WBP event graph

## 호출되는 쪽

- `UEnemyHPBarWidget::SetHealthValue`
- `UEnemyGroggyGaugeWidget::SetGroggyPercent`
- `UBossHPViewModel::OnBossHealthChangedForUI`
- `UBossHPViewModel::OnBossGroggyChangedForUI`
- `OnBossTargetChanged`
- `OnBossHealthChanged`
- `OnBossGroggyChanged`
- `OnBossHPVisibilityChanged`

## 서버/클라이언트 조건

local HUD widget이다. authority 판단을 하지 않고, replicated actor/component 상태는 ViewModel을 거쳐 표시만 한다.

## Blueprint 연결 여부

WBP parent class로 사용한다. View Models panel에 `BossHPViewModel`을 이름 그대로 추가하고 Creation Type은 `Create Instance`로 둔다. 내부 HP child 이름은 `HPBar` 또는 `BossHPBar`, groggy child 이름은 `BossGroggyBar`, 선택 텍스트는 `BossNameText`, `BossHPText`.

## 헷갈리기 쉬운 코드

- `UEnemyHPWidget`: enemy actor 머리 위 HP/groggy widget
- `UEnemyHPWidgetComponent`: enemy actor에 붙는 world/screen space 표시 component
- `UEnemyHPBarWidget`: HP material percent만 갱신하는 재사용 가능 하위 bar
- `UEnemyGroggyGaugeWidget`: groggy material percent만 갱신하는 재사용 가능 하위 gauge

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/BossHPViewModel.md`
- `Docs/Intent/HUD/PragmataGameState-ActiveBossTarget.md`
- `Docs/Intent/HUD/PragmataHUDWidget-HandleActiveBossTargetChanged.md`

## 확실한 근거

- `UPragmataHUDWidget`이 `WBP_BossHPWidget` child를 찾아 `SetBossTarget`을 호출
- `UBossHPWidget`은 gameplay component를 직접 구독하지 않고 `UBossHPViewModel` event를 구독
- `UBossHPViewModel`이 `UPragmataHealthComponent`의 `CurrentHealth`, `MaxHealth`, death 상태를 ViewModel 값으로 변환
- `UBossHPWidget`이 ViewModel event를 받아 `BossGroggyBar` child에 `SetGroggyPercent`를 전달

## 추정된 의도

enemy HP UI의 material bar 패턴은 재사용하되, gameplay-state binding은 ViewModel이 갖고 위치/생명주기 소유권은 root HUD가 갖게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 것

최종 boss name 표기 방식. 현재 C++ fallback은 actor `GetName()`을 쓰고, WBP event에서 별도 표시명으로 덮을 수 있다.

## 검증 방법

```powershell
rg -n "UBossHPWidget|UBossHPViewModel|SetBossTarget|BossHPBar|BossGroggyBar|BossHPText" Source Docs
```

## 다음 수정 때 주의점

보스별 이름, phase, shield bar를 추가할 때 gameplay actor를 WBP가 직접 polling하지 않게 한다. 필요한 state는 GameState나 `UBossHPViewModel` 경로로 넘긴다.
