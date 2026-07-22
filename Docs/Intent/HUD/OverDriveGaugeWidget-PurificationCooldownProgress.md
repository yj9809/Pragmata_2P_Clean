# UOverDriveGaugeWidget 정화 UI 상태 표시 의도 카드

## 대상

`UOverDriveGaugeWidget::SetPurificationPanelVisible`, `UOverDriveGaugeWidget::SetPurificationSkillAvailable`, `UOverDriveGaugeWidget::SetPurificationCooldownPercent`

## 종류

HUD child visibility and material parameter binding

## 정의 위치

- `Source/Pragmata_2P/UI/OverDriveGaugeWidget.h`
- `Source/Pragmata_2P/UI/OverDriveGaugeWidget.cpp`

## 이 코드가 존재하는 이유

정화 스킬 UI를 `WBP_OverDriveGauge` 안에 두되, 스킬 획득/사용 가능/cooldown 진행 판정과 UMG 렌더링을 분리하기 위해 존재한다.

## 사용해야 하는 상황

- 정화 스킬 획득 여부에 따라 `PurificationPanel` 전체를 켜거나 끌 때
- 정화 스킬 사용 가능 상태에 따라 `PurificationDot`과 `CoolDownPanel`을 교체할 때
- 정화 스킬 cooldown 중 0~1 진행률을 HUD material에 전달할 때

## 사용하면 안 되는 상황

- cooldown 완료 여부를 UI에서 판정하려는 경우
- `ProgressBar` 위젯을 사용해 사각형 fill을 만들려는 경우
- OverDrive gauge 본체의 `FillAmount`를 정화 cooldown 값으로 재사용하려는 경우

## 기본 흐름

PlayerState `OnCleanseUnlockedChanged` -> `SetPurificationPanelVisible(bool)` -> 최초 unlock이면 `PlayCleanseUnlockedFocus` -> 사용 가능 상태 -> `SetPurificationSkillAvailable(bool)` -> cooldown 계산 -> `SetPurificationCooldownPercent(0~1)` -> `CoolDownImage` material의 `Percent` parameter 갱신

## Blueprint 연결 여부

`WBP_OverDriveGauge`에 선택적 child가 필요하다.

- `PurificationPanel`: 정화 UI 전체 root
- `PurificationDot`: 사용 가능 상태 표시
- `CoolDownPanel`: 사용 불가/cooldown 상태 root. 내부에 `RELOAD` TextBlock 배치
- `CoolDownImage`: cooldown fill `Image`, Brush는 UI material 또는 material instance

`PurificationPanel`은 스킬 미획득 상태에서 공간을 차지하지 않도록 `Collapsed`를 사용한다. `PurificationDot`과 `CoolDownPanel` 상호 전환은 `Hidden`으로 처리해 `RELOAD` TextBlock과 cooldown image layout 위치를 유지한다.

`UOverDriveGaugeWidget`는 PlayerState delegate를 구독한다. NativeTick polling은 unlock 판정에 사용하지 않는다. `PlayCleanseUnlockedFocus`의 native 기본 구현은 `PurificationPanel`을 1초 동안 축소 복귀시키며, Widget Blueprint는 필요할 때 별도 focus animation으로 override한다.

## Material 기준

`Percent=0`은 비어 있음, `Percent=1`은 가득 참이다. 아래에서 위로 차오르는 원형 fill은 UI material에서 원형 mask와 `UV.y >= 1 - Percent` 조건을 곱해 만든다.

## 헷갈리기 쉬운 코드

- `SetOverDrivePercent`: OverDrive gauge 본체 진행률
- `ApplyFill`: OverDrive material의 `FillAmount` 갱신
- `CoolDownPercentParameterName`: 정화 cooldown material parameter 이름

## 관련 문서

- `Docs/KNOW_HOW.md`

## 검증 방법

```powershell
rg -n "PurificationPanel|PurificationSkillAvailable|CoolDownImage|SetPurificationCooldown" Source Docs
```

Editor에서는 `WBP_OverDriveGauge`에서 `PurificationPanel`, `PurificationDot`, `CoolDownPanel`, `CoolDownImage` 이름을 맞추고, `CoolDownImage` material의 `Percent`를 `0`, `0.5`, `1`로 바꿔 아래에서 위로 차오르는지 확인한다. PIE 흐름 확인은 gameplay 쪽에서 `SetPurificationPanelVisible`, `SetPurificationSkillAvailable`, `SetPurificationCooldownPercent`를 호출하는 경로가 연결된 뒤 수행한다.
