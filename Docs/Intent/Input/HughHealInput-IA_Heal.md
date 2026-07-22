# Hugh IA_Heal Hold 의도 카드

## 대상

`/Game/Blueprints/Player/Input/Hugh/IA_Heal`, `IMC_Hugh`, `AHughCharacter::OnStartHeal`, `AHughCharacter::Server_RequestHeal`, `UPragmataHUDViewModel` recovery fields

## 종류

Enhanced Input hold-based Hugh heal flow

## 정의 위치

- `Content/Blueprints/Player/Input/Hugh/IA_Heal.uasset`
- `Content/Blueprints/Player/Input/Hugh/IMC_Hugh.uasset`
- `Content/CookLabels/PAL_CoreRuntime.uasset`
- `Source/Pragmata_2P/Characters/HughCharacter.h`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`
- `Source/Pragmata_2P/Components/PragmataPlayerStatsComponent.h`
- `Source/Pragmata_2P/UI/PragmataHUDViewModel.h`
- `Source/Pragmata_2P/UI/PragmataHUDViewModel.cpp`

## 이 코드가 존재하는 이유

Hugh의 repair cartridge 회복 기능을 실제 player input으로 연결하고, Hugh/Diana 양쪽 HUD가 남은 cartridge 수량과 사용 가능 상태를 같은 source에서 보게 하기 위해 존재한다. 버튼 hold progress는 local UI feedback이고, HP 회복과 cartridge 소모는 server authority에서만 확정한다.

## 사용해야 하는 상황

- Hugh player가 heal 버튼을 1초 이상 hold했을 때
- UI가 heal circular hold progress를 표시해야 할 때
- UI가 heal 금지 상태를 표시해야 할 때
- 양쪽 플레이어 HUD가 repair cartridge count를 공유해야 할 때

## 사용하면 안 되는 상황

- UI 위젯이 직접 heal RPC나 `ConsumeRepairCartridge()`를 호출하는 경우
- Diana cleanse, interact, upgrade hold처럼 별도 action lifecycle이 있는 입력
- client 임시 값으로 cartridge 수량이나 HP를 확정하는 경우

## 기본 흐름

`IA_Heal Started` -> local hold timer 시작 -> `HealHoldThresholdSeconds` 도달 -> `Server_RequestHeal()` -> server `CanUseHeal` 조건 재검증 -> `UPragmataPlayerStatsComponent::ConsumeRepairCartridge()` -> 실제 HP 회복 성공 시 cartridge 1개 차감 -> replicated stat/availability -> `UPragmataHUDViewModel` recovery field 갱신

hold가 계속 유지되면 `HealRepeatIntervalSeconds`마다 같은 server 요청을 반복한다. cartridge가 0이 되거나 blocker가 생기면 local hold 상태와 progress를 0으로 정리한다.

## 이 코드를 호출하는 쪽

- `AHughCharacter::SetupPlayerInputComponent`
- `IMC_Hugh`의 `Q`, `Gamepad_FaceButton_Right` mapping
- `UPragmataHUDWidget::BindSharedHUDStats`
- `UPragmataHUDViewModel::BindSharedHUDStats`

## 이 코드가 호출하는 쪽

- `UPragmataPlayerStatsComponent::ConsumeRepairCartridge`
- `UPragmataHealthComponent::RecoverHP`
- `UPragmataHUDViewModel::SetCanUseHeal`
- `UPragmataHUDViewModel::SetRecoveryHoldProgress`
- `UPragmataHUDViewModel::SetRecoveryItemCount`

## 서버/클라이언트 조건

입력 hold와 progress 계산은 local owner 전용이다. 실제 회복 사용은 `Server_RequestHeal()`에서만 확정하고, server는 remaining cartridge, full HP 여부, death, reload, dash, run, falling, hovering 상태를 다시 검사한다. `bCanUseHeal`은 replicated snapshot이고, hold progress는 복제하지 않는다.

## Blueprint 연결 여부

`IA_Heal`은 Boolean Input Action이다. `IMC_Hugh`에서 keyboard `Q`, gamepad `Gamepad_FaceButton_Right`에 매핑한다. `PAL_CoreRuntime`에는 `IMC_Hugh`와 `IA_Heal`을 모두 explicit asset으로 둔다. UI designer는 ViewModel의 `CanUseHeal()`, `GetRecoveryHoldProgress()`, `GetRecoveryItemCount()`, `IsRecoveryHolding()`, `IsRecoveryProhibited()`를 읽는다.

## 헷갈리기 쉬운 코드

- `UPragmataPlayerStatsComponent::GetRepairCartridge()`와 `GetRemainingHealCartridges()`는 같은 수량을 가리킨다. 후자는 UI/기획 용어 alias다.
- `bRecoveryProhibited`는 표시 alias이며 `!bCanUseHeal`로 동기화된다.
- full HP는 heal 사용 불가다.
- boost/dodge blocker는 현재 `IsDashing`과 `HughAnimState.bIsRunning`을 모두 차단한다.
- airborne blocker는 `CharacterMovement->IsFalling()`과 `HughAnimState.bIsHovering`을 모두 차단한다.

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Stats/PragmataPlayerStatsComponent.md`
- `Docs/Intent/Input/HughLookInput-IA_Look.md`

## 관련 커밋

미확인. asset 변경 이력은 binary라 `git log -- Content/Blueprints/Player/Input/Hugh/IA_Heal.uasset Content/Blueprints/Player/Input/Hugh/IMC_Hugh.uasset` 확인 필요.

## 확실한 근거

- `AHughCharacter::ResolveInputActions()`가 `IA_Heal`을 soft-load fallback으로 등록한다.
- `AHughCharacter::SetupPlayerInputComponent()`가 `IA_Heal`의 `Started`, `Completed`, `Canceled`를 bind한다.
- `Server_RequestHeal()`은 server에서 `CalculateHealAvailability()`를 다시 확인한 뒤 `ConsumeRepairCartridge()`를 호출한다.
- `ConsumeRepairCartridge()`는 `RecoverHP()` 성공 시에만 cartridge를 차감한다.
- `UPragmataHUDViewModel`은 Hugh heal delegates를 받아 recovery count, hold progress, holding, prohibited, can-use 값을 갱신한다.

## 추정한 의도

heal UI는 양쪽 플레이어가 같은 상태를 보되, 버튼 hold ring은 실제 입력을 누르는 local Hugh player의 feedback으로 제한하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

heal 사용 성공/실패의 audio, animation, camera feedback은 아직 정해지지 않았다. `bCanUseHeal`은 현재 gameplay blocker boolean만 제공하고 blocker reason enum은 없다.

## 검증 방법

```powershell
rg -n "IA_Heal|Server_RequestHeal|CanUseHeal|GetHealHoldProgress|OnHealAvailabilityChanged|RecoveryHoldProgress" Source Docs
```

PIE에서 Hugh `Q`와 `Gamepad_FaceButton_Right`를 각각 1초 hold해 1회 회복되는지 확인한다. 계속 hold하면 1초마다 반복 회복되고, cartridge 0/full HP/dash/run/falling/hovering 중에는 progress가 reset되고 `CanUseHeal=false`가 HUD에 표시되는지 확인한다.

## 다음 수정 시 주의점

UI에서 heal을 직접 실행하지 않는다. 새 blocker를 추가하면 `CalculateHealAvailability()`와 HUD 표시 요구를 함께 갱신한다. input asset을 이동하거나 새로 만들면 `PAL_CoreRuntime` explicit asset을 다시 확인한다.
