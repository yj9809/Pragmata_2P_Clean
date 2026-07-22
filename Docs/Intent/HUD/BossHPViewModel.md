# UBossHPViewModel 의도 카드

## 대상
`UBossHPViewModel`

## 종류

Boss HUD ViewModel

## 정의 위치

- `Source/Pragmata_2P/UI/BossHPViewModel.h`
- `Source/Pragmata_2P/UI/BossHPViewModel.cpp`

## 코드가 존재하는 이유

보스 HP는 world widget이 아니라 Hugh/Diana root HUD 안에 표시하지만, HUD/WBP가 gameplay component를 직접 구독하면 MVVM 경계가 깨진다. 이 ViewModel은 replicated boss actor의 health component를 local UI 값으로 변환하는 중간층이다.

## 사용해야 하는 상황

- `APragmataGameState::ActiveBossTarget`이 바뀐 뒤 boss HP 표시 source를 갱신하는 경우
- boss actor의 `UPragmataHealthComponent`를 HUD 표시값으로 바꾸는 경우
- WBP가 `BossHPBar`, `BossGroggyBar`를 FieldNotify 또는 UI event 기반으로 갱신해야 하는 경우

## 사용하면 안 되는 상황

- 일반 enemy 머리 위 HP 표시
- boss damage, phase, aggro 판정
- viewport widget 생성이나 HUD layout 배치

## 기본 흐름

`UBossHPWidget::SetBossTarget` -> `UBossHPViewModel::SetBossTarget` -> boss `UPragmataHealthComponent` 조회 -> `OnHealthChanged`/`OnDeath` delegate 구독 -> FieldNotify 값 및 UI event 갱신

groggy source는 아직 확정하지 않았다. 현재는 `SetBossGroggyValue()` 또는 `SetBossGroggyPercent()`로 외부에서 값을 넣을 수 있게만 열어 둔다.

## 호출하는 쪽

- `UBossHPWidget::SetBossTarget`
- 필요 시 WBP event graph 또는 이후 boss groggy source

## 호출되는 쪽

- `UPragmataHealthComponent::OnHealthChanged`
- `UPragmataHealthComponent::OnDeath`
- `OnBossHealthChangedForUI`
- `OnBossGroggyChangedForUI`
- `OnBossHPVisibilityChangedForUI`
- `OnBossNameChangedForUI`

## 서버/클라이언트 조건

local HUD ViewModel이다. authority 판단은 하지 않고, server replicated GameState/HealthComponent 상태를 표시값으로 변환한다.

## Blueprint 연결 여부

`WBP_BossHPWidget` View Models panel에 이름을 정확히 `BossHPViewModel`로 추가하고 Creation Type을 `Create Instance`로 둔다.

## 헷갈리기 쉬운 코드

- `UPragmataHUDViewModel`: 플레이어 공용 HUD ViewModel
- `UBossHPWidget`: boss bar composite 표시 widget
- `UEnemyHPBarWidget`: material percent만 갱신하는 하위 bar

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/BossHPWidget.md`
- `Docs/Intent/HUD/PragmataGameState-ActiveBossTarget.md`
- `Docs/Intent/HUD/PragmataHUDWidget-HandleActiveBossTargetChanged.md`

## 확실한 근거

- 프로젝트 HUD 규칙은 gameplay state -> ViewModel -> WBP 표시 흐름을 사용
- 기존 `UPragmataHUDViewModel`이 health component delegate를 ViewModel에서 구독
- `UBossHPWidget`은 child material bar 표시만 담당하도록 유지

## 추정된 의도

보스전 HUD가 Hugh/Diana 양쪽에서 같은 replicated boss state를 보되, WBP layout과 gameplay state binding을 분리하려는 의도.

## 확신도

강한 추정

## 아직 모르는 것

보스 groggy의 최종 replicated source. 현재는 ViewModel setter만 준비했고 자동 구독 source는 정하지 않았다.

## 검증 방법

```powershell
rg -n "UBossHPViewModel|BossHPViewModel|OnBossHealthChangedForUI|SetBossGroggyPercent" Source Docs
```

## 다음 수정 때 주의점

boss phase, shield, groggy component를 붙일 때 WBP나 widget이 gameplay actor를 직접 polling하지 않게 한다. source가 생기면 `UBossHPViewModel`에 bind 함수를 추가한다.
