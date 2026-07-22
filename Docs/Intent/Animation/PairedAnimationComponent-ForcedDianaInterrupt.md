# UPairedAnimationComponent::LockDianaActionsForCue 의도 카드

## 대상

`UPairedAnimationComponent::TriggerPairedCue`, `PlayCue`, `IsForcedDianaInterruptCue`, `LockDianaActionsForCue`, `ReleaseDianaActionLock`

## 종류

Diana forced paired animation/action lock boundary

## 정의 위치

- `Source/Pragmata_2P/Components/PairedAnimationComponent.h`
- `Source/Pragmata_2P/Components/PairedAnimationComponent.cpp`
- `Source/Pragmata_2P/Game/PragmataGameState.h`
- `Source/Pragmata_2P/Game/PragmataGameState.cpp`
- `Source/Pragmata_2P/Animation/DianaAnimInstance.h`
- `Source/Pragmata_2P/Animation/DianaAnimInstance.cpp`

## 이 코드가 존재하는 이유

Hugh의 큰 피격, 사망, 강제 paired 연출 중 Diana가 scan/hack/camera combat action으로 montage와 IK 상태를 덮어쓰지 못하게 하기 위해 존재한다. Diana는 별도 damage/collision 판정이 없으므로 사망 방향은 Hugh가 확정한 paired cue direction을 그대로 따른다.

## 사용해야 하는 상황

- Hugh가 `Large`, `Death`, `ForcedPair` 피격을 받아 Diana avatar도 강제 연출에 들어가야 할 때
- shelter, death, hard hit처럼 Diana 독립 조작보다 paired montage가 우선인 상황
- Diana AnimBP가 일반 follow/combat layer 대신 forced pair layer를 선택해야 할 때
- Diana death animation을 Hugh death direction과 같은 방향으로 1회 재생해야 할 때

## 사용하면 안 되는 상황

- `Small`, `Medium` 피격처럼 Hugh 혼자 반응해도 되는 damage
- Diana에게 별도 collision/damage direction 계산을 추가하려는 경우
- 일반 enemy hack, map hack, scan 실패를 막기 위한 임시 disable flag 대용
- montage 길이와 무관한 gameplay stun 상태를 표현하려는 경우

## 기본 흐름

`AHughCharacter::ReceiveDamage` -> `ShouldSendPairedHitReactCue` -> `UPairedAnimationComponent::TriggerPairedCue` -> replicated `LastCue` -> `PlayCue` -> `DA_PairedAnimationSet::FindEntry`가 cue/strength/direction으로 montage 선택 -> forced cue이면 Diana montage 재생과 `APragmataGameState::SetDianaActionsLocked(true)` -> `UDianaAnimInstance`가 forced pair/death state 반영

## 이 코드를 호출하는 쪽

- `AHughCharacter::ReceiveDamage`
- `UPairedAnimationComponent::OnRep_LastCue`

## 이 코드가 호출하는 쪽

- `UPairedAnimationSet::FindEntry`
- `USkeletalMeshComponent::GetAnimInstance`
- `UAnimInstance::Montage_Play`
- `APragmataGameState::SetDianaActionsLocked`

## 서버/클라이언트 조건

cue 확정은 server에서 시작한다. `LastCue` 복제로 각 peer가 같은 paired montage와 Diana directional death montage를 재생하고, action lock 상태는 `APragmataGameState::bDianaActionsLocked` 복제로 Diana scan/hack/node interaction gate에 반영된다.

## Blueprint 연결 여부

paired montage asset과 cue entry는 `UPairedAnimationSet` DataAsset이 소유한다. Diana directional death도 `DA_PairedAnimationSet`의 direction별 `DianaMontage` entry가 소유한다. Death cue 수신 시 Hugh montage는 cue direction과 같은 `Front/Right/Back/Left` section에서 시작하고, 같은 direction entry의 Diana montage와 IK/root death 상태를 즉시 적용한다.

## 헷갈리기 쉬운 코드

- `EPragmataHitReactStrength::ForcedPair`: damage strength이면서 Diana action lock을 요구하는 강제 연출 의도
- `EPairedAnimationCueType::Death`: 사망 cue type
- `FPairedAnimationEntry::Direction`: Diana가 자체 방향 계산을 하지 않고 Hugh cue direction을 montage 선택에만 사용
- `UDianaAnimInstance::UpdateIKTargets`: forced pair 중에도 IK target 계산 자체는 남지만 layer 선택과 alpha가 AnimBP에서 달라질 수 있음
- `ADianaOperatorPawn::CanUseDianaActionIntent`: action lock 중 local scan/hack/reticle intent를 RPC 전에 차단

## 관련 문서

- `Docs/KNOW_HOW.md`
- `Docs/Intent/Animation/DianaAnimInstance-UpdateIKTargets.md`
- `Docs/Intent/Damage/HughCharacter-ReceiveDamage.md`

## 관련 커밋

- `c4dfe237 refactor: Hugh animation state 원본 정리`
- session 기록 `codex-260622-diana-abp-implementation`
- session 기록 `codex-260630-forcedpair-comments-and-asset`

## 확실한 근거

- `AHughCharacter::ShouldSendPairedHitReactCue`가 `Large`, `Death`, `ForcedPair`만 Diana cue 대상으로 둠
- `UPairedAnimationComponent::IsForcedDianaInterruptCue`가 death, mounted watch, death strength, forced pair strength를 action lock 대상으로 판정
- `UPairedAnimationSet::FindEntry`가 cue+strength+direction 우선순위로 Diana front/right/back/left death montage를 선택
- `UPairedAnimationComponent::LockDianaActionsForCue`가 `APragmataGameState::SetDianaActionsLocked(true)`를 호출
- `UDianaAnimInstance`가 `AreDianaActionsLocked()`와 Hugh death state로 forced pair/death animation state를 계산
- `Docs/KNOW_HOW.md`가 hard hit/death/shelter 중 Diana 독립 조작이 montage를 덮지 않아야 한다고 기록

## 추정한 의도

Diana avatar는 Hugh 등에 붙어 있지만 플레이어가 별도 pawn을 조작하므로, 강제 연출 순간만 공유 lock을 걸어 co-op 조작과 paired montage의 우선순위를 명확히 하려는 의도. death는 Diana가 맞는 것이 아니라 Hugh가 맞아 확정된 결과이므로 Diana animation도 Hugh cue를 추종해야 한다.

## 확신도

확정

## 아직 모르는 점

death가 아닌 forced cue의 `DefaultActionLockDuration`이 실제 montage 길이, play rate, start offset과 항상 일치하는지는 Blueprint/DataAsset 검증이 필요하다.

## 검증 방법

```powershell
rg -n "PlayCue|ForcedPair|IsForcedDianaInterruptCue|LockDianaActionsForCue|AreDianaActionsLocked" Source Docs
```

## 다음 수정 시 주의점

forced pair lock을 임시 UI disable flag처럼 확장하지 않는다. 연출 lock duration을 바꿀 때는 paired montage entry의 play rate와 cue start offset을 함께 확인한다. Diana death 방향이 필요해도 Diana collision/damage 계산을 추가하지 말고 Hugh의 replicated paired cue를 사용한다.
