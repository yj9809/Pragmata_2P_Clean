# UWaistArmComponent::SetArmState 의도 카드

## 대상

`UWaistArmComponent::SetArmState`

## 종류

WaistArm animation state bridge

## 정의 위치

- `Source/Pragmata_2P/Characters/WaistArmComponent.h`
- `Source/Pragmata_2P/Characters/WaistArmComponent.cpp`

## 이 코드가 존재하는 이유

Hugh 본체 상태 변화가 WaistArm skeletal mesh AnimInstance에 같은 state로 전달되게 하기 위해 존재한다.

## 사용해야 하는 상황

- Hugh aim, reload, weapon attach 상태가 WaistArm animation에 반영되어야 하는 경우

## 사용하면 안 되는 상황

- Hugh body animation state를 직접 바꾸는 경우
- WaistArm mesh hierarchy를 재구성하려는 경우

## 기본 흐름

state 저장 -> `WaistArmMesh`의 `UWaistArmAnimInstance` cast -> anim instance state 갱신

## 이 코드를 호출하는 쪽

- `AHughCharacter` aim/reload/weapon 상태 경로

## 이 코드가 호출하는 쪽

- `UWaistArmAnimInstance` state setter

## 서버/클라이언트 조건

animation presentation 경로. 호출 위치에 따라 local/replicated animation state와 함께 확인 필요.

## Blueprint 연결 여부

`BlueprintCallable`. `BP_HughCharacter > WaistArmComponent > WaistArmMesh` hierarchy와 함께 확인 필요.

## 헷갈리기 쉬운 코드

- `AHughCharacter::AttachWaistArm`: component attachment
- `EnsureWaistArmMeshAttachment`: Blueprint template mismatch 보정

## 관련 문서

- `Docs/Architecture/architecture.md`
- `Docs/Architecture/replication-role-animation-fix.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `5312f35f build: PAL cook 검증 지시 보강` 이전 history 추가 확인 필요

## 확실한 근거

- `Docs/KNOW_HOW.md`에 `SetArmState` link 실패와 header inline 제거 기록
- `Docs/Architecture/architecture.md`에 WaistArm component가 mesh와 animation state 전달 담당이라고 기록

## 추정한 의도

Hugh body와 WaistArm mesh를 분리하면서 animation state 전달 경계를 component 하나로 유지하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

모든 replicated aim state가 이 함수로 들어오는지 여부.

## 검증 방법

```powershell
rg -n "SetArmState|WaistArmComponent|WaistArmAnimInstance" Source Docs
```

## 다음 수정 시 주의점

header inline 변경이나 signature 변경은 UHT/link와 Blueprint callable 경계를 함께 확인.
