# UPragmataHughMovementComponent saved move 의도 카드

## 대상

`UPragmataHughMovementComponent`, `FHughSavedMove`, `SetWantsToAim`, `SetWantsToRun`, `SetWantsToHover`, `WantsToAim`, `WantsToRun`, `WantsToHover`

## 종류

Hugh movement prediction intent carrier

## 정의 위치

- `Source/Pragmata_2P/Characters/PragmataHughMovementComponent.h`
- `Source/Pragmata_2P/Characters/PragmataHughMovementComponent.cpp`
- `Source/Pragmata_2P/Characters/HughCharacter.cpp`

## 이 코드가 존재하는 이유

Hugh의 aim/run/hover/jump hold 입력 의도를 Unreal CharacterMovement saved move에 실어 server movement update에서 같은 입력 sample을 재생하기 위해 존재한다.

## 사용해야 하는 상황

- client 입력 timing이 movement prediction과 함께 서버에 전달되어야 할 때
- aim/run/hover 전환이 movement sample 경계에서 보존되어야 할 때
- local prediction과 server state 확정이 모두 필요한 Hugh movement state를 추가할 때

## 사용하면 안 되는 상황

- 최종 gameplay state 저장소로 쓰는 경우
- remote proxy animation display state로 직접 읽는 경우
- 서버가 결정해야 하는 stamina/condition check를 client intent bit로 대체하는 경우

## 기본 흐름

Hugh input setter -> `SetWantsToAim`, `SetWantsToRun`, `SetWantsToHover` -> `FHughSavedMove::SetInitialPosition` snapshot -> compressed flag encode -> server `UpdateFromCompressedFlags` decode -> `AHughCharacter::SyncMovementIntentFromMovementComponent`가 gameplay state 확정

## 이 코드를 호출하는 쪽

- `AHughCharacter` input/state setter
- Unreal CharacterMovement prediction system

## 이 코드가 호출하는 쪽

- `FSavedMove_Character`
- `FNetworkPredictionData_Client_Character`
- `AHughCharacter::SyncMovementIntentFromMovementComponent`

## 서버/클라이언트 조건

client는 intent bit를 saved move에 싣고, server는 compressed flag에서 intent를 복원한다. 최종 state 확정과 replication은 `AHughCharacter`가 담당한다.

## Blueprint 연결 여부

C++ movement component 경계다. Blueprint 직접 호출 경로는 없다.

## 헷갈리기 쉬운 코드

- `FHughReplicatedAnimState`: remote proxy와 AnimBP 표시용 replicated state
- `AHughCharacter::SetAimingState`, `SetRunningState`, `SetHoveringState`: gameplay state 확정
- `UHughAnimInstance`: 표시 state를 읽는 animation layer

## 관련 문서

- `Docs/Systems/Movement/hugh-movement-component-guide.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `9a4fed8e fix: listen server role replication 보강`
- `a4583618 docs: server replication 구조 문서 추가`
- `27e1f5b2 docs: Hugh movement intent 가이드 추가`
- `c4dfe237 refactor: Hugh animation state 원본 정리`

## 확실한 근거

- `PragmataHughMovementComponent.cpp`가 `FLAG_Custom_0..3`로 Hugh intent를 encode/decode
- `FHughSavedMove::CanCombineWith`가 intent가 다른 move를 합치지 않음
- `Docs/Systems/Movement/hugh-movement-component-guide.md`가 intent와 gameplay state를 분리해 설명

## 추정한 의도

client가 버튼을 눌렀다는 사실과 server가 실제 state를 허용하는 결정을 분리해, network prediction과 gameplay authority를 동시에 유지하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

새 movement intent가 추가될 때 `FLAG_Custom` 여유 bit가 충분한지 여부.

## 검증 방법

```powershell
rg -n "FHughSavedMove|WantsToAim|UpdateFromCompressedFlags|SyncMovementIntentFromMovementComponent" Source Docs
```

## 다음 수정 시 주의점

`WantsToAim`, `WantsToRun`, `WantsToHover` 값을 remote animation source처럼 읽지 말고, state 확정은 `AHughCharacter`에 남긴다.
