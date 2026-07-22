# UPragmataHUDWidget::ResolveWeaponWheelRoot 의도 카드

## 대상

`UPragmataHUDWidget::ResolveWeaponWheelRoot`

## 종류

HUD child widget name compatibility fallback

## 정의 위치

- `Source/Pragmata_2P/UI/PragmataHUDWidget.h`
- `Source/Pragmata_2P/UI/PragmataHUDWidget.cpp`

## 이 코드가 존재하는 이유

`WBP_HughHUD`의 기존 child 이름이 `WBP_WeaponWheel`인 상태에서도 C++가 weapon wheel을 찾고 ammo binding을 복구하게 하기 위해 존재한다.

## 사용해야 하는 상황

- `BindWidget` 이름 `WeaponWheelRoot`가 asset에 없지만 기존 `WBP_WeaponWheel` child가 있을 때
- HUD가 Hugh weapon source에 bind되거나 weapon wheel visibility를 바꿀 때
- asset rename 없이 C++에서 backward-compatible 연결이 필요할 때

## 사용하면 안 되는 상황

- 모든 Widget Blueprint child 이름을 C++ 계약에 맞춰 이미 정리한 경우
- weapon wheel이 아닌 임의 child widget을 찾아야 하는 경우
- missing child를 조용히 무시해도 되는 임시 테스트 코드

## 기본 흐름

`WeaponWheelRoot` cached pointer 확인 -> 기존 child 이름 `WBP_WeaponWheel` 조회 -> cast 성공 시 cache -> 실패 시 warning 1회 출력

## 이 코드를 호출하는 쪽

- `NativeConstruct`
- `NativeDestruct`
- `SetWeaponWheelVisible`
- `BindSharedHUDStats`

## 이 코드가 호출하는 쪽

- `GetWidgetFromName`
- `UWeaponWheelWidget::BindWeaponSource`
- `UWeaponWheelWidget::SetWeaponWheelVisible`

## 서버/클라이언트 조건

local HUD widget에서만 의미가 있다. gameplay state를 결정하지 않고, replicated/shared source를 UI에 연결한다.

## Blueprint 연결 여부

`WBP_HughHUD` 내부 child 이름과 직접 연결된다. Blueprint asset scan 또는 Widget Designer 확인으로 child 이름을 검증해야 한다.

## 헷갈리기 쉬운 코드

- `UWeaponWheelWidget::BindWeaponSource`: Hugh weapon/ammo delegate에 실제 bind
- `UPragmataHUDWidget::BindSharedHUDStats`: Hugh/Diana shared stats source를 HUD ViewModel에 연결
- `APragmataPlayerController::EnsureGameplayHUD`: 역할별 root HUD 생성

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/HUD/PragmataPlayerController-EnsureGameplayHUD.md`

## 관련 커밋

- `030bd673 fix: 구조 정리 회귀 복구`
- `d0889728 fix: WeaponWheel ammo 바인딩 복구`

## 확실한 근거

- `PragmataHUDWidget.cpp`가 `GetWidgetFromName(TEXT("WBP_WeaponWheel"))` fallback을 사용
- `.agents/work` 기록에 asset rename 시도 후 asset 변경을 되돌리고 C++ fallback을 유지한 사실 존재
- `Docs/KNOW_HOW.md`가 child 이름 `WBP_WeaponWheel` 유지와 C++ fallback을 기록

## 추정한 의도

Widget asset 이름 변경과 C++ build timing이 서로 어긋나도 팀원이 HUD를 바로 테스트할 수 있게 하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

향후 Widget Blueprint를 정리한 뒤 fallback을 제거해도 되는 시점.

## 검증 방법

```powershell
rg -n "ResolveWeaponWheelRoot|WBP_WeaponWheel|WeaponWheelRoot|BindWeaponSource" Source Docs
```

## 다음 수정 시 주의점

fallback 제거 전에는 `WBP_HughHUD` child 이름 변경과 packaged HUD 검증을 먼저 끝내야 한다.
