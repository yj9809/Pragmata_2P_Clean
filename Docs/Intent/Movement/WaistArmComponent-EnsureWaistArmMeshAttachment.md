# UWaistArmComponent::EnsureWaistArmMeshAttachment 의도 카드

## 대상

`UWaistArmComponent::EnsureWaistArmMeshAttachment`

## 종류

WaistArm mesh hierarchy repair

## 정의 위치

- `Source/Pragmata_2P/Characters/WaistArmComponent.h`
- `Source/Pragmata_2P/Characters/WaistArmComponent.cpp`

## 이 코드가 존재하는 이유

Blueprint override를 보존하면서 `WaistArmMesh`가 `WaistArmComponent`의 child로 유지되게 보정하기 위해 존재한다.

## 사용해야 하는 상황

- component registration 전후에 `WaistArmMesh` attach parent가 어긋날 수 있는 경우
- Hugh Blueprint hierarchy를 깨지 않고 보정해야 하는 경우

## 사용하면 안 되는 상황

- `WaistArmMesh`를 Hugh actor-owned component로 완전 분리하려는 refactor
- socket 위치를 임시로 바꾸기 위한 호출

## 기본 흐름

template 여부 확인 -> `WaistArmMesh` 존재 확인 -> attach parent가 component가 아니면 `SetupAttachment(this)`

## 이 코드를 호출하는 쪽

- `AHughCharacter::PreRegisterAllComponents` 또는 관련 registration 보정 경로

## 이 코드가 호출하는 쪽

- `USkeletalMeshComponent::SetupAttachment`

## 서버/클라이언트 조건

component hierarchy 보정. network gameplay state와 직접 관련 없음.

## Blueprint 연결 여부

Blueprint hierarchy 보존 목적. `BP_HughCharacter > WaistArmComponent > WaistArmMesh`와 직접 관련.

## 헷갈리기 쉬운 코드

- `AHughCharacter::AttachWaistArm`: WaistArmComponent를 Hugh mesh socket에 붙임
- `GetWaistArmMesh`: weapon/Diana IK socket lookup용 mesh accessor

## 관련 문서

- `Docs/Architecture/architecture.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

미확인. `git log -SEnsureWaistArmMeshAttachment -- Source/Pragmata_2P Docs`로 추적.

## 확실한 근거

- `Docs/Architecture/architecture.md`에 `Template Mismatch`와 invisible 위험, component child 유지 원칙 기록
- `Docs/KNOW_HOW.md`에 WaistArm hierarchy current solution 기록

## 추정한 의도

기존 Blueprint asset override를 잃지 않으면서 C++ component ownership을 안정화하려는 의도.

## 확신도

강한 추정

## 아직 모르는 점

어떤 migration 시점에 attach parent가 실제로 틀어졌는지의 최초 원인.

## 검증 방법

```powershell
rg -n "EnsureWaistArmMeshAttachment|Template Mismatch|WaistArmMesh" Source Docs
```

## 다음 수정 시 주의점

Blueprint component path를 바꾸는 refactor는 asset override 손실 위험을 별도 검증해야 한다.
