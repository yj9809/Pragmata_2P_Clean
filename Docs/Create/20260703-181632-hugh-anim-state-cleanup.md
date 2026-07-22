# Hugh animation state 중복 캐시 제거

생성 시각: 2026-07-03 18:16:32

## 핵심 변경

- `AHughCharacter` raw `IsAiming/IsRunning/IsJumpInput/IsHovering` 캐시 제거
- `FHughReplicatedAnimState::bIs*`를 Hugh animation/display 상태 원본으로 정리
- `HughAnimState` 복제를 `COND_SkipOwner`로 바꿔 owner local prediction 보호
- `WantsTo*`는 movement prediction intent 전용으로 유지

관련 없는 dirty 파일:

- `Config/DefaultEditor.ini` 제외

## 변경 파일 목록

| 파일 | 변경 요약 |
| --- | --- |
| `Source/Pragmata_2P/Characters/HughCharacter.h` | raw `Is*` 멤버 제거, `OnRep` old snapshot signature 반영 |
| `Source/Pragmata_2P/Characters/HughCharacter.cpp` | `HughAnimState.bIs*` 직접 사용, `COND_SkipOwner` 적용 |
| `Docs/KNOW_HOW.md` | Hugh 상태 소유권과 raw cache 재도입 금지 기록 |
| `Docs/Architecture/replication-role-animation-fix.md` | prediction intent와 display snapshot 계약 보강 |

## `Source/Pragmata_2P/Characters/HughCharacter.h`

### 무엇이 바뀜

- `AHughCharacter`가 별도 raw bool cache를 갖지 않도록 선언 제거
- remote proxy용 `ApplyReplicatedAnimState`와 `OnRep_HughAnimState`가 이전 snapshot을 받도록 변경

### 이전

```cpp
void ApplyReplicatedAnimState();

UFUNCTION()
void OnRep_HughAnimState();

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Combat")
bool IsAiming = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
bool IsRunning = false;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movement")
bool IsJumpInput = false;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Movement")
bool IsHovering = false;
```

### 이후

```cpp
void ApplyReplicatedAnimState(const FHughReplicatedAnimState& OldHughAnimState);

UFUNCTION()
void OnRep_HughAnimState(FHughReplicatedAnimState OldHughAnimState);
```

### 비고

- `DefaultFOV`, `AimFOV`, `RotateSpeed`, `bIsTryingToMove` 등 기존 설정값은 유지

## `Source/Pragmata_2P/Characters/HughCharacter.cpp`

### 무엇이 바뀜

- owner/server가 `HughAnimState.bIs*`를 직접 갱신
- owner에게 `HughAnimState` 서버 snapshot을 보내지 않도록 replication condition 변경
- remote proxy는 `OnRep_HughAnimState`에서 표시 부작용만 재구성

### 이전

```cpp
DOREPLIFETIME(AHughCharacter, HughAnimState);

if (IsAiming == bNewAiming)
{
	return;
}

IsAiming = bNewAiming;
ActionState = IsAiming ? EActionState::Combat : EActionState::Locomotion;
HughMovement->SetWantsToAim(IsAiming);
```

```cpp
HughAnimState.bIsAiming = IsAiming;
HughAnimState.bIsRunning = IsRunning;
HughAnimState.bIsJumpInput = IsJumpInput;
HughAnimState.bIsHovering = IsHovering;
```

### 이후

```cpp
DOREPLIFETIME_CONDITION(AHughCharacter, HughAnimState, COND_SkipOwner);

if (HughAnimState.bIsAiming == bNewAiming)
{
	return;
}

HughAnimState.bIsAiming = bNewAiming;
ActionState = HughAnimState.bIsAiming ? EActionState::Combat : EActionState::Locomotion;
HughMovement->SetWantsToAim(HughAnimState.bIsAiming);
```

```cpp
HughAnimState.ActionState = ActionState;
HughAnimState.MovementState = MovementState;
HughAnimState.DisplayMovementSpeed = ResolveDisplayMovementSpeed();
HughAnimState.bIsTryingToMove = bIsTryingToMove;
```

### 비고

- `UPragmataHughMovementComponent::WantsTo*`와 `FHughSavedMove` 경로는 그대로 유지
- `ABP_HughCharacter`의 `IsAiming/IsHovering/IsJumpInput` 문자열은 `UHughAnimInstance` 변수이며 삭제 대상 아님

## `Docs/KNOW_HOW.md`

### 무엇이 바뀜

- Hugh 표시 상태 원본과 owner prediction 보호 규칙 추가
- raw `Is*` cache 재도입 금지 기준 추가

### 이전

```text
- gameplay HUD와 aim 중 표시되는 shared crosshair는 pawn/BP controller가 아니라 owning local `APragmataPlayerController` 책임
```

### 이후

```text
- gameplay HUD와 aim 중 표시되는 shared crosshair는 pawn/BP controller가 아니라 owning local `APragmataPlayerController` 책임
- Hugh aim/run/jump hold/hover 표시 상태는 `HughAnimState.bIs*`가 원본
- Hugh owner는 local prediction으로 `HughAnimState`를 직접 갱신하고, 서버 복제는 `COND_SkipOwner`로 owner를 건너뜀
- `UPragmataHughMovementComponent::WantsTo*`는 movement prediction intent 전용이며, 표시 상태 저장소로 쓰지 않음
```

## `Docs/Architecture/replication-role-animation-fix.md`

### 무엇이 바뀜

- `WantsTo*`와 `HughAnimState`의 역할 분리 명시
- `COND_SkipOwner` replication 정책 기록

### 이전

```text
- `AHughCharacter::HughAnimState`
- `ReplicatedUsing=OnRep_HughAnimState`
```

### 이후

```text
- `AHughCharacter::HughAnimState`
- `ReplicatedUsing=OnRep_HughAnimState`
- `DOREPLIFETIME_CONDITION(..., COND_SkipOwner)`로 owner local prediction 보호
```

## 검증 결과

### raw `Is*` 제거 검색

```text
rg -n "\b(IsAiming|IsRunning|IsJumpInput|IsHovering)\b" Source/Pragmata_2P/Characters/HughCharacter.h Source/Pragmata_2P/Characters/HughCharacter.cpp
```

결과:

```text
출력 없음
```

### `AHughCharacter` Blueprint 직접 참조 검색

```text
rg -a -n "HughCharacter:IsAiming|HughCharacter:IsRunning|HughCharacter:IsJumpInput|HughCharacter:IsHovering|AHughCharacter:IsAiming|AHughCharacter:IsRunning|AHughCharacter:IsJumpInput|AHughCharacter:IsHovering" Content/Blueprints Content/UI
```

결과:

```text
출력 없음
```

### replication 정책 확인

```text
rg -n "DOREPLIFETIME\(AHughCharacter, HughAnimState\)|DOREPLIFETIME_CONDITION\(AHughCharacter, HughAnimState, COND_SkipOwner\)|bool IsAiming|bool IsRunning|bool IsJumpInput|bool IsHovering|void AHughCharacter::OnRep_HughAnimState\(" Source/Pragmata_2P/Characters/HughCharacter.h Source/Pragmata_2P/Characters/HughCharacter.cpp
```

결과:

```text
Source/Pragmata_2P/Characters/HughCharacter.cpp:269:	DOREPLIFETIME_CONDITION(AHughCharacter, HughAnimState, COND_SkipOwner);
Source/Pragmata_2P/Characters/HughCharacter.cpp:1417:void AHughCharacter::OnRep_HughAnimState(FHughReplicatedAnimState OldHughAnimState)
```

### whitespace 검사

```text
git diff --check -- Source/Pragmata_2P/Characters/HughCharacter.h Source/Pragmata_2P/Characters/HughCharacter.cpp Docs/KNOW_HOW.md Docs/Architecture/replication-role-animation-fix.md
```

결과:

```text
통과
```

### 남은 검증

```text
Unreal Editor target build
BP_HughCharacter, ABP_HughCharacter, ABP_Diana compile
2-player PIE smoke
```

결과:

```text
full Unreal build와 Editor/Blueprint 검증은 사용자 명시 요청 전까지 미실행
```
