# UDianaAnimInstance::UpdateIKTargets 의도 카드

## 대상

`UDianaAnimInstance::UpdateIKTargets`, `UpdateHandIKTargetMode`, `SetRightHandIKEnabled`

## 종류

Diana animation IK target 계산 경계

## 정의 위치

- `Source/Pragmata_2P/Animation/DianaAnimInstance.h`
- `Source/Pragmata_2P/Animation/DianaAnimInstance.cpp`

## 이 코드가 존재하는 이유

`ABP_Diana` EventGraph에 흩어진 socket world 위치 계산을 C++로 옮겨, AnimBP는 pose graph와 skeletal-control node에 집중하게 하기 위해 존재한다.

## 사용해야 하는 상황

- Diana mounted pose의 toe/hand IK target을 Hugh와 WaistArm socket 기준으로 계산할 때
- follow layer와 Hugh 이동 상태에 따라 `Handle`과 `SideHandle` socket을 바꿔야 할 때
- hack aim pose 또는 `DianaSkillBody` slot 재생 중 오른손 animation을 IK가 덮지 않게 막아야 할 때
- death pose에서 Hugh socket IK가 Diana 사망 animation을 덮지 않게 모든 IK alpha를 0으로 유지할 때
- Diana skill의 body와 face sequence를 기존 mount AnimBP 위에 합성할 때

## 사용하면 안 되는 상황

- AnimGraph FABRIK/TwoBoneIK node 자체를 C++로 대체하려는 경우
- socket이 없는 asset 문제를 코드 offset으로 숨기려는 경우
- Diana operator pawn의 gameplay mode를 animation layer 외부에서 우회 판정하려는 경우
- 사망 pose 문제를 Hugh/Diana attach 해제로 우회하려는 경우

## 기본 흐름

`NativeUpdateAnimation` -> Hugh/Diana pawn cache -> layer/follow/combat/death state 갱신 -> hand IK mode 결정 -> `UpdateIKTargets`가 alpha reset -> death면 모든 IK alpha 0 유지 후 반환, death가 아니면 socket target을 Diana mesh local 위치로 변환 -> AnimGraph가 alpha/location 변수 사용

## 이 코드를 호출하는 쪽

- `UDianaAnimInstance::NativeUpdateAnimation`
- `UDianaAnimInstance::UpdateCombatState`

## 이 코드가 호출하는 쪽

- `AHughCharacter::GetMesh`
- `UWaistArmComponent::GetWaistArmMesh`
- `USceneComponent::DoesSocketExist`
- `USceneComponent::GetSocketLocation`
- `FTransform::InverseTransformPosition`

## 서버/클라이언트 조건

animation 계산은 각 peer의 AnimInstance에서 실행된다. gameplay authority를 결정하지 않고, replicated Hugh/Diana 상태와 local socket 상태를 표시용으로 읽는다.

`EDianaOperatorMode::Combat`과 enemy combat flag는 Diana의 입력·스킬 허용 및 HUD 표시 경계다. 이 값들이 Diana의 locomotion layer를 `CombatIndependent` 또는 tense idle로 바꾸면 안 된다. aim/hack과 forced pair만 각각의 명시적 AnimBP 상태·montage로 표시한다.

## Blueprint 연결 여부

AnimBP가 `VisibleAnywhere, BlueprintReadOnly` IK 변수와 `EDianaHandIKTargetMode`를 읽는다. `ABP_Diana`는 state machine 뒤에 `DianaSkillBody` slot과 `DianaSkillFace` slot을 둔다. face slot은 `ABP_Diana_Title`와 같이 `Layered Blend per Bone`의 `Neck` branch로 합성한다. Death cue 수신 즉시 Diana death montage, IK alpha 0, death bind root 전환을 함께 시작한다. Directional montage는 저장된 Hugh death cue direction으로 `DA_PairedAnimationSet`의 0811-0814 entry를 선택한다.

## 헷갈리기 쉬운 코드

- `UDianaAnimInstance::UpdateDeathState`: Hugh death 상태를 읽어 `bDeathActive`를 켜는 경계
- `UWaistArmComponent::EnsureWaistArmMeshAttachment`: WaistArm mesh를 Hugh에 붙이는 component 경계
- `UPairedAnimationComponent`: forced pair cue와 Diana lock을 담당하는 gameplay/animation cue 경계
- `ADianaOperatorPawn`: camera/input/hack mode를 소유하는 pawn
- `PlayAnimation()`/single-node mode: `DianaAvatarMesh`의 AnimBP, IK, face layer를 끊으므로 skill presentation에 사용 금지

## 관련 문서

- `Docs/Intent/Movement/WaistArmComponent-EnsureWaistArmMeshAttachment.md`
- `Docs/Operations/unreelmcp-anomalies.md`
- `Docs/Architecture/architecture.md`
- `Docs/KNOW_HOW.md`

## 관련 커밋

- `f55cb8c8 feat: Diana 손 IK target mode 추가`
- `812cd365 feat: Diana IK target 계산 C++ 이관`

## 확실한 근거

- `.omx` context에 `ABP_Diana` EventGraph socket 계산을 `UDianaAnimInstance`로 옮기는 목표 기록
- `DianaAnimInstance.cpp`에서 `UpdateSocketIKTarget`이 socket 검증 실패 시 해당 limb alpha만 0으로 처리
- `UpdateLayerMode`가 `Combat` operator mode에서도 `FollowHugh`를 선택해 normal idle을 유지
- `UpdateCombatState`가 aim 중이거나 `DianaSkillBody` slot이 active인 동안 `SetRightHandIKEnabled(false)`를 호출
- `NativeUpdateAnimation`이 `UpdateDeathState()`를 `UpdateIKTargets()`보다 먼저 호출
- `ResetIKTargets()`가 모든 IK alpha를 0으로 초기화

## 추정한 의도

Blueprint graph의 시각적 pose node는 유지하되, 반복 계산과 mode 판정은 C++에서 관리해 ABP가 커질수록 의도를 잃지 않게 하려는 의도.

## 확신도

확정

## 아직 모르는 점

현재 `ABP_Diana` asset이 모든 exposed 변수를 실제 AnimGraph에서 쓰는지 여부는 최신 Blueprint scan으로 재검증 필요.

## 검증 방법

```powershell
rg -n "UpdateIKTargets|EDianaHandIKTargetMode|SetRightHandIKEnabled|bDeathActive" Source/Pragmata_2P/Animation Docs
```

## 다음 수정 시 주의점

socket 누락을 C++ offset 보정으로 숨기지 말고 asset socket 문제로 드러내야 한다. death 중에는 `bDeathActive` 기반 alpha 0과 mesh detach를 함께 사용해 IK와 Hugh attachment가 원본 root 이동을 덮지 않게 한다.
