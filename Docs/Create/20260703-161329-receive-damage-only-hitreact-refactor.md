# ReceiveDamage 단일화와 Hugh HitReact 리팩터링

생성 시각: 2026-07-03 16:13:29

## 핵심 변경

- gameplay damage 경로를 `ReceiveDamage` 하나로 단일화
- `TakeDamage` bridge 제거
- `FPragmataDamageSpec` 생성 책임을 피해 source 쪽으로 고정
- Hugh HitReact를 `ReceiveDamage` 성공 이후 실제 HP 감소량 기준으로 처리
- `Behit*Montage` 이름을 `HitReact*Montage`로 정리
- 기존 Blueprint montage 할당 보존을 위해 `CoreRedirect` 추가

최종 damage 경로:

```text
FPragmataDamageSpec 생성
-> UPragmataDamageLibrary::DealDamageToActor
-> IPragmataDamageable::ReceiveDamage
-> UPragmataHealthComponent::ReceiveDamage
```

참고:

- `Config/DefaultEditor.ini`는 작업 시작 전부터 dirty 상태였고 이 changelist 대상이 아님
- `ApplyDamage` 검색 결과에 남는 `PlayerHPBarWidget::ApplyDamageGhostLayout`은 UI helper 이름이며 Unreal damage API 호출이 아님

## 변경 파일 목록

| 파일 | 변경 요약 |
| --- | --- |
| `Source/Pragmata_2P/Characters/PlayableCharacter.h` | `TakeDamage` 선언과 `m_DamageSpec` 제거 |
| `Source/Pragmata_2P/Characters/PlayableCharacter.cpp` | Unreal damage event bridge 구현 제거 |
| `Source/Pragmata_2P/Characters/HughCharacter.h` | Hugh HitReact helper 추가, `Behit*` 이름 변경 |
| `Source/Pragmata_2P/Characters/HughCharacter.cpp` | HitReact 실행 위치를 `ReceiveDamage` 이후로 이동 |
| `Source/Pragmata_2P/Enemy/MonsterBase.h` | `TakeDamage` 선언 제거 |
| `Source/Pragmata_2P/Enemy/MonsterBase.cpp` | `TakeDamage` 구현 제거 |
| `Source/Pragmata_2P/Interfaces/PragmataDamageable.h` | 공통 HitReact 계산 API 제거 |
| `Config/DefaultEngine.ini` | Hugh montage property rename redirect 추가 |
| `Docs/Systems/Damage/damage-flow.md` | ReceiveDamage-only 흐름과 Mermaid diagram으로 갱신 |
| `Docs/KNOW_HOW.md` | damage recipe를 현재 구조로 갱신 |
| `Docs/Architecture/architecture.md` | `TakeDamage` bridge 유지 설명 제거 |
| `Docs/Systems/Hacking/enemy-hacking-integration.md` | stale `ApplyDamage` 표현 수정 |

## `Source/Pragmata_2P/Characters/PlayableCharacter.h`

### 무엇이 바뀜

- Unreal `TakeDamage` override 선언 제거
- `FDamageEvent` forward declaration 제거
- 임시 damage spec 저장용 `m_DamageSpec` 제거

### 이전

```cpp
struct FDamageEvent;

// 외부 Unreal damage event를 공통 damage spec으로 변환
virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

UPROPERTY()
FPragmataDamageSpec m_DamageSpec;
```

### 이후

```cpp
// 공통 피해 수신 가능 여부 확인
virtual bool CanReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) const override;

// 공통 피해 수신 처리
virtual bool ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec) override;
```

### 비고

`APlayableCharacter`는 이제 receiver 역할만 담당한다. `FPragmataDamageSpec`은 공격자, 무기, notify, hack 같은 source 쪽에서 만든다.

## `Source/Pragmata_2P/Characters/PlayableCharacter.cpp`

### 무엇이 바뀜

- `Engine/DamageEvents.h` include 제거
- `GameFramework/Controller.h` include 제거
- `APlayableCharacter::TakeDamage` 구현 제거
- health 적용은 `ReceiveDamage_Implementation`만 유지

### 이전

```cpp
float APlayableCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	m_DamageSpec.DamageAmount = DamageAmount;
	m_DamageSpec.SourceActor = EventInstigator ? EventInstigator->GetPawn() : DamageCauser;
	m_DamageSpec.DamageCauser = DamageCauser;
	m_DamageSpec.InstigatorController = EventInstigator;
	m_DamageSpec.DamageKind = EPragmataDamageKind::Generic;

	const bool bApplied = IPragmataDamageable::Execute_ReceiveDamage(this, m_DamageSpec);
	...
}
```

### 이후

```cpp
bool APlayableCharacter::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	if (!IPragmataDamageable::Execute_CanReceiveDamage(this, DamageSpec))
	{
		return false;
	}

	return HealthComponent && HealthComponent->ReceiveDamage(DamageSpec);
}
```

### 비고

기본 playable class는 health 처리만 담당한다. HitReact나 특수 연출은 하위 character가 `ReceiveDamage` 성공 이후에 붙인다.

## `Source/Pragmata_2P/Characters/HughCharacter.h`

### 무엇이 바뀜

- `TakeDamage` override 제거
- `CalculateHitDirection`, `CalculateHitReact` override 제거
- Hugh 전용 HitReact helper 추가
- `Behit*Montage`를 `HitReact*Montage`로 rename

### 이전

```cpp
virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

virtual EPragmataHitDirection CalculateHitDirection_Implementation(const FPragmataDamageSpec& DamageSpec) override;

virtual EPragmataHitReactStrength CalculateHitReact_Implementation(const FPragmataDamageSpec& DamageSpec) override;

TObjectPtr<UAnimMontage> BehitSmallMontage;
TObjectPtr<UAnimMontage> BehitMediumMontage;
TObjectPtr<UAnimMontage> BehitLargeMontage;
```

### 이후

```cpp
// 실제 감소량을 MaxHealth 비율로 바꿔 Hugh 피격 강도를 결정
EPragmataHitReactStrength ResolveHitReactStrength(float ActualDamage) const;

// damage source 위치를 기준으로 Hugh 피격 방향을 결정
EPragmataHitDirection ResolveHitReactDirection(const FPragmataDamageSpec& DamageSpec) const;

// Hugh mesh에서 hit react montage와 방향 section을 직접 재생
bool PlayHitReactMontage(EPragmataHitReactStrength Strength, EPragmataHitDirection Direction);

// Diana를 같이 잠글 정도의 큰 피격 cue인지 확인
bool ShouldSendPairedHitReactCue(EPragmataHitReactStrength Strength) const;

TObjectPtr<UAnimMontage> HitReactSmallMontage;
TObjectPtr<UAnimMontage> HitReactMediumMontage;
TObjectPtr<UAnimMontage> HitReactLargeMontage;
```

### 비고

HitReact 계산은 모든 damageable actor의 공통 계약이 아니다. 현재는 Hugh 전용 구현으로 두는 것이 읽기 쉽고 책임도 명확하다.

## `Source/Pragmata_2P/Characters/HughCharacter.cpp`

### 무엇이 바뀜

- Hugh `TakeDamage` 구현 제거
- HitReact 실행 위치를 `ReceiveDamage_Implementation`으로 이동
- `Super::ReceiveDamage_Implementation` 전후 health를 비교해 실제 감소량 계산
- `DamageRatio = ActualDamage / MaxHealth` 기준으로 HitReact 강도 결정
- `Small <= 0.04f`, `Medium <= 0.10f`, 나머지 `Large`
- Hugh montage는 Hugh가 직접 재생
- Diana paired cue는 `Large`, `Death`, `ForcedPair`에서만 전송

### 이전

```cpp
float AHughCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	...
	switch (CalculateHitReact_Implementation(m_DamageSpec))
	{
	case EPragmataHitReactStrength::Small:
		Montage = BehitSmallMontage;
		break;
	...
	}
}
```

문제점:

- 현재 프로젝트의 gun, melee, hack damage는 대부분 `TakeDamage`를 거치지 않음
- `m_DamageSpec`라는 임시 상태에 의존
- HitReact 강도, 방향, montage 재생이 한 함수에 섞임
- `DamageInPercentage <= 4.f`처럼 ratio와 percentage 기준이 섞여 거의 항상 Small이 됨

### 이후

```cpp
bool AHughCharacter::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	const float PreviousHealth = HealthComponent ? HealthComponent->GetCurrentHealth() : 0.f;
	const bool bApplied = Super::ReceiveDamage_Implementation(DamageSpec);
	if (!bApplied)
	{
		return bApplied;
	}

	const bool bIsDead = HealthComponent && HealthComponent->IsDead();
	const float CurrentHealth = HealthComponent ? HealthComponent->GetCurrentHealth() : PreviousHealth;
	const float ActualDamage = FMath::Max(PreviousHealth - CurrentHealth, 0.f);

	const EPragmataHitReactStrength Strength = bIsDead
		? EPragmataHitReactStrength::Death
		: DamageSpec.HitReactStrength == EPragmataHitReactStrength::ForcedPair
			? EPragmataHitReactStrength::ForcedPair
			: ResolveHitReactStrength(ActualDamage);
	const EPragmataHitDirection Direction = ResolveHitReactDirection(DamageSpec);

	PlayHitReactMontage(Strength, Direction);

	if (PairedAnimationComponent && ShouldSendPairedHitReactCue(Strength))
	{
		const EPairedAnimationCueType CueType = bIsDead
			? EPairedAnimationCueType::Death
			: EPairedAnimationCueType::HitReact;
		PairedAnimationComponent->TriggerPairedCue(CueType, Strength, Direction);
	}

	return true;
}
```

HitReact 강도 계산:

```cpp
const float DamageRatio = ActualDamage / HealthComponent->GetMaxHealth();
if (DamageRatio <= SmallHitReactDamageRatio)
{
	return EPragmataHitReactStrength::Small;
}
if (DamageRatio <= MediumHitReactDamageRatio)
{
	return EPragmataHitReactStrength::Medium;
}

return EPragmataHitReactStrength::Large;
```

### 비고

`0.04`는 4%다. 이전 코드는 `0.04 <= 4.f`처럼 비교해서 의도보다 넓은 범위가 Small로 분류됐다. 새 코드는 ratio 상수로 직접 비교한다.

## `Source/Pragmata_2P/Enemy/MonsterBase.h`

### 무엇이 바뀜

- `AMonsterBase::TakeDamage` 선언 제거

### 이전

```cpp
// Unreal damage event를 공통 damage spec으로 변환
virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
```

### 이후

```cpp
// scan UI에 보여줄 monster 정보 생성
virtual FPragmataScanResult GetPragmataScanResult_Implementation(AActor* Scanner) const override;
```

### 비고

Monster도 `ReceiveDamage`만 damage receiver entry point로 사용한다.

## `Source/Pragmata_2P/Enemy/MonsterBase.cpp`

### 무엇이 바뀜

- `Engine/DamageEvents.h` include 제거
- `AMonsterBase::TakeDamage` 구현 제거

### 이전

```cpp
float AMonsterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = DamageAmount;
	DamageSpec.SourceActor = EventInstigator ? EventInstigator->GetPawn() : DamageCauser;
	DamageSpec.DamageCauser = DamageCauser;
	DamageSpec.InstigatorController = EventInstigator;
	DamageSpec.DamageKind = EPragmataDamageKind::Generic;
	...
}
```

### 이후

```cpp
bool AMonsterBase::ReceiveDamage_Implementation(const FPragmataDamageSpec& DamageSpec)
{
	...
	const bool bApplied = HealthComponent && HealthComponent->ReceiveDamage(FinalDamageSpec);
	...
}
```

### 비고

Monster death animation 선택 흐름은 유지된다. 죽기 전 `DamageSpec`을 보고 `CurrentDeathAnimation`을 확정한 뒤 health를 차감한다.

## `Source/Pragmata_2P/Interfaces/PragmataDamageable.h`

### 무엇이 바뀜

- `CalculateHitDirection` 제거
- `CalculateHitReact` 제거
- interface를 receiver 계약으로 축소

### 이전

```cpp
UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
EPragmataHitDirection CalculateHitDirection(const FPragmataDamageSpec& DamageSpec);

UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
EPragmataHitReactStrength CalculateHitReact(const FPragmataDamageSpec& DamageSpec);
```

### 이후

```cpp
UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
bool CanReceiveDamage(const FPragmataDamageSpec& DamageSpec) const;

UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
bool ReceiveDamage(const FPragmataDamageSpec& DamageSpec);
```

### 비고

HitReact는 receiver마다 다르게 처리될 가능성이 높다. 공통 interface에 두면 오히려 의미가 흐려진다.

## `Config/DefaultEngine.ini`

### 무엇이 바뀜

- Hugh montage property rename용 `PropertyRedirects` 추가

### 이전

```ini
[CoreRedirects]
+EnumRedirects=(OldName="/Script/Pragmata_2P.EEnemyHackType",NewName="/Script/Pragmata_2P.EEnemyHackType",ValueChanges=(("Worker","Walker")))
```

### 이후

```ini
[CoreRedirects]
+EnumRedirects=(OldName="/Script/Pragmata_2P.EEnemyHackType",NewName="/Script/Pragmata_2P.EEnemyHackType",ValueChanges=(("Worker","Walker")))
+PropertyRedirects=(OldName="/Script/Pragmata_2P.HughCharacter.BehitSmallMontage",NewName="/Script/Pragmata_2P.HughCharacter.HitReactSmallMontage")
+PropertyRedirects=(OldName="/Script/Pragmata_2P.HughCharacter.BehitMediumMontage",NewName="/Script/Pragmata_2P.HughCharacter.HitReactMediumMontage")
+PropertyRedirects=(OldName="/Script/Pragmata_2P.HughCharacter.BehitLargeMontage",NewName="/Script/Pragmata_2P.HughCharacter.HitReactLargeMontage")
```

### 비고

기존 `BP_HughCharacter`에 할당된 `Behit*` montage 값이 rename 후에도 로드되도록 보존한다.

## `Docs/Systems/Damage/damage-flow.md`

### 무엇이 바뀜

- `TakeDamage Bridge` 섹션 제거
- ReceiveDamage-only 구조로 문서 재작성
- source/receiver 책임 분리 설명 추가
- `FPragmataDamageSpec` 생성 위치 명시
- Gun, melee, hack, Hugh HitReact, Monster receiver 흐름을 Mermaid diagram으로 정리

### 이전

```markdown
## Unreal TakeDamage Bridge

`APlayableCharacter::TakeDamage`와 `AMonsterBase::TakeDamage`는 외부 Unreal damage event 호환용 bridge다.
```

### 이후

```markdown
프로젝트 damage는 하나의 주 경로만 사용한다.

FPragmataDamageSpec
-> UPragmataDamageLibrary::DealDamageToActor
-> IPragmataDamageable::ReceiveDamage
-> UPragmataHealthComponent::ReceiveDamage
```

### 비고

문서의 기준 질문은 두 가지다.

- `FPragmataDamageSpec`은 어디서 생성되는가
- Hugh HitReact는 언제 실행되는가

둘 다 현재 코드 기준으로 바로 찾을 수 있게 정리했다.

## `Docs/KNOW_HOW.md`

### 무엇이 바뀜

- 기존 `Damage Flow and Melee Hit Notify` recipe를 ReceiveDamage-only 기준으로 갱신
- Hugh HitReact threshold와 Diana paired cue 조건 추가
- Unreal damage API를 gameplay path로 쓰지 않는 규칙 추가

### 이전

```markdown
- 외부 Unreal damage event는 `TakeDamage` bridge가 실제 적용 피해량만 `Super::TakeDamage`로 알림
```

### 이후

```markdown
- Unreal `ApplyDamage`, `ApplyPointDamage`, `ApplyRadialDamage`는 gameplay damage 경로로 사용하지 않음
- `TakeDamage` bridge는 제거했고, source가 `FPragmataDamageSpec`을 직접 만든 뒤 `ReceiveDamage` 경로로 전달
- Hugh HitReact 기준은 `Small <= 0.04`, `Medium <= 0.10`, `Large > 0.10`
```

### 비고

새 recipe를 만들지 않고 기존 damage recipe를 현재 사실로 갱신했다.

## `Docs/Architecture/architecture.md`

### 무엇이 바뀜

- `TakeDamage` bridge 유지 설명 제거
- Hugh montage 직접 재생과 Diana paired cue 조건을 현재 구조로 수정

### 이전

```markdown
- `TakeDamage`는 외부 Unreal damage event 호환 bridge로만 유지하고 실제 적용 피해량만 engine damage event로 알림
- Hugh damage 성공 시 `UPairedAnimationComponent`가 pair cue 생성
- cue replicated 후 Hugh montage는 cue entry 기준 재생
```

### 이후

```markdown
- Unreal damage API는 gameplay damage 경로로 쓰지 않고 source가 `FPragmataDamageSpec`을 직접 생성
- Hugh damage 성공 시 Hugh가 HitReact montage를 직접 재생
- `Large`, `Death`, `ForcedPair` 피격만 `UPairedAnimationComponent`가 pair cue 생성
```

### 비고

architecture 문서도 코드와 같은 책임 분리 기준으로 맞췄다.

## `Docs/Systems/Hacking/enemy-hacking-integration.md`

### 무엇이 바뀜

- 오래된 `HealthComponent->ApplyDamage` 표현을 `ReceiveDamage` 경로로 수정

### 이전

```markdown
- `ApplySuccessfulHack`은 적 내부에서 `FPragmataDamageSpec`(Kind=Hack) → `HealthComponent->ApplyDamage`까지 처리. **float 하나만 넘기면 끝.**
```

### 이후

```markdown
- `ApplySuccessfulHack`은 적 내부에서 `FPragmataDamageSpec`(Kind=Hack) 생성 후 `ReceiveDamage` 경로로 처리. **float 하나만 넘기면 끝.**
```

### 비고

Hack damage는 `AMonsterBase::ApplySuccessfulHack_Implementation`에서 `bHacked=true` 설정 후 `ReceiveDamage`로 들어간다.

## 검증 결과

### 제거 대상 검색

```text
rg -n "TakeDamage|Super::TakeDamage|FDamageEvent|FPointDamageEvent|Engine/DamageEvents|m_DamageSpec|CalculateHitDirection|CalculateHitReact|Behit" Source/Pragmata_2P
```

결과:

```text
0건
```

### Unreal damage API 검색

```text
rg -n "ApplyDamage|ApplyPointDamage|ApplyRadialDamage" Source/Pragmata_2P
```

결과:

```text
Source/Pragmata_2P/UI/PlayerHPBarWidget.cpp
Source/Pragmata_2P/UI/PlayerHPBarWidget.h
```

판단:

- `PlayerHPBarWidget::ApplyDamageGhostLayout`만 검색됨
- UI ghost layout helper 이름이며 gameplay damage 호출이 아님

### Whitespace 검사

```text
git diff --check
```

결과:

```text
통과
```

비고:

- 작업 전부터 dirty였던 `Config/DefaultEditor.ini` line-ending warning만 표시

### Build

```text
.agents\skills\build-fix\scripts\run-editor-build.ps1
```

결과:

```text
Pragmata_2PEditor Win64 Development build succeeded
```

기존 경고:

```text
WeaponWheelSlotWidget.cpp
UImage::SetBrushSize deprecated
```

이번 변경과 직접 관련 없는 기존 API 경고다.
