# GripGun 발사 권한 경계 보강

생성 시각: 2026-07-03 19:07:59

대상 커밋: `b3b430995de012ad17161890bf3b356b6c617206`

범위 제외:

- `Config/DefaultEditor.ini`: 기존 작업트리 변경
- `Content/Enemy/Executor/Animations/General/AM_Executor_BodyOpen.uasset`: 기존 작업트리 삭제

## 핵심 변경

- `AGripGun::PullTrigger()`를 서버 전용 발사 확정 진입점으로 정리
- ammo 소모, reload 전환, trace, damage, impact VFX multicast를 `CommitServerShot()` 주변으로 분리
- reload 완료를 `OnCompleteReload()` 단일 public API로 축소
- `ReplenishAmmo()`와 timer helper를 private 서버 helper로 내려 외부 조합 호출 차단
- `Docs/KNOW_HOW.md`, `Docs/Architecture/architecture.md`에 fire/reload authority 계약 기록

## 변경 파일 목록

| 파일 | 변경 요약 |
| --- | --- |
| `Source/Pragmata_2P/Weapon/GripGun.h` | public API 축소, 서버 commit/reload/VFX helper 선언 |
| `Source/Pragmata_2P/Weapon/GripGun.cpp` | 서버 authority gate, shot commit, reload 완료, multicast VFX 분리 |
| `Source/Pragmata_2P/Weapon/WeaponBase.h` | `PullTrigger()` 반환값을 `bool`로 변경 |
| `Source/Pragmata_2P/Weapon/WeaponBase.cpp` | 공통 ammo/cooldown gate 반환, `RefillAmmo()` authority guard 추가 |
| `Source/Pragmata_2P/Characters/HughCharacter.cpp` | reload montage 완료 후 GripGun 단일 완료 API 호출 |
| `Docs/KNOW_HOW.md` | damage flow recipe에 GripGun server fire/reload 계약 추가 |
| `Docs/Architecture/architecture.md` | Damage Flow 항목에 GripGun authority 경계 추가 |

## `Source/Pragmata_2P/Weapon/GripGun.h`

### 무엇이 바뀜

- `PullTrigger()`가 성공 여부를 반환하도록 override 변경
- `ReplenishAmmo()`, `StartAutoReplenish()`, `StopAutoReplenish()`, `SetIsReloading()`을 public API에서 제거
- 서버 shot commit, reload 시작, VFX multicast helper 추가

### 이전

```cpp
virtual void PullTrigger() override;
public:
	void ReplenishAmmo();
	void OnCompleteReload();
	void StartAutoReplenish();
	void StopAutoReplenish();
	FORCEINLINE	void SetIsReloading(bool flag) { IsReloading = flag; };
```

### 이후

```cpp
virtual bool PullTrigger() override;
public:
	void OnCompleteReload();

private:
	void CommitServerShot(UWorld* World, AController* OwnerController);
	void BeginServerReload();

	void ReplenishAmmo();
	void StartAutoReplenish();
	void StopAutoReplenish();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactEffect(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal);
```

### 비고

외부 caller가 reload flag와 ammo 보충을 따로 조합할 수 없게 public 표면 축소.

## `Source/Pragmata_2P/Weapon/GripGun.cpp`

### 무엇이 바뀜

- non-authority 호출은 즉시 실패
- owner/controller/world 검증 후에만 ammo 소모 진행
- hit VFX는 server trace 결과를 multicast로 표시
- dedicated server는 VFX spawn 생략

### 이전

```cpp
void AGripGun::PullTrigger()
{
	Super::PullTrigger();

	if (CurAmmunition <= 0 || IsReloading) return;
	
	--CurAmmunition;
	BroadcastAmmoChanged();
```

```cpp
if (bSuccess)
{
	DealDamageToHitActor(Hit, OwnerController);

	if (ImpactEffect)
	{
		FRotator ImpactRotation = Hit.ImpactNormal.Rotation();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		   GetWorld(),
		   ImpactEffect,
		   Hit.Location,
		   ImpactRotation
		);
	}
}
```

### 이후

```cpp
bool AGripGun::PullTrigger()
{
	if (!HasAuthority() || IsReloading)
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr)
	{
		return false;
	}

	AController* OwnerController = Cast<AController>(OwnerPawn->GetController());
	if (OwnerController == nullptr)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || !Super::PullTrigger())
	{
		return false;
	}

	CommitServerShot(World, OwnerController);
	return true;
}
```

```cpp
void AGripGun::CommitServerShot(UWorld* World, AController* OwnerController)
{
	--CurAmmunition;
	BroadcastAmmoChanged();
	
	if (CurAmmunition <= 0)
	{
		BeginServerReload();
	}
	else
	{
		StartAutoReplenish();
	}
```

```cpp
void AGripGun::SpawnImpactEffectAtLocation(const FVector& ImpactLocation, const FVector& ImpactNormal) const
{
	if (GetNetMode() == NM_DedicatedServer || !ImpactEffect || !GetWorld())
	{
		return;
	}

	const FRotator ImpactRotation = ImpactNormal.Rotation();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		ImpactEffect,
		ImpactLocation,
		ImpactRotation);
}
```

### 비고

`CommitServerShot()`는 이제 validation 이후의 서버 gameplay commit만 담당.

## `Source/Pragmata_2P/Weapon/WeaponBase.h`

### 무엇이 바뀜

- 공통 발사 함수가 gate 성공 여부를 반환

### 이전

```cpp
// 발사 입력 처리
virtual void PullTrigger();
```

### 이후

```cpp
// 서버 발사 확정에 필요한 공통 ammo/cooldown gate 통과 시 true 반환
virtual bool PullTrigger();
```

### 비고

파생 무기가 ammo/cooldown gate 실패 후 shot commit을 계속하지 않게 됨.

## `Source/Pragmata_2P/Weapon/WeaponBase.cpp`

### 무엇이 바뀜

- `PullTrigger()`가 ammo/cooldown gate 실패 시 `false` 반환
- `RefillAmmo()`는 서버에서만 ammo snapshot 변경
- 미사용 `Characters/HughController.h` include 제거

### 이전

```cpp
void AWeaponBase::PullTrigger()
{
	if (CurAmmunition <= 0) return;
	
	TargetRecoilPitch = TargetRecoilPitchAmount;
	
	StartFireCooldown();
}
```

### 이후

```cpp
bool AWeaponBase::PullTrigger()
{
	if (CurAmmunition <= 0 || IsCoolingDown)
	{
		return false;
	}
	
	TargetRecoilPitch = TargetRecoilPitchAmount;
	
	StartFireCooldown();
	return true;
}
```

```cpp
void AWeaponBase::RefillAmmo()
{
	if (!HasAuthority())
	{
		return;
	}

	CurAmmunition = MaxAmmunition;
	BroadcastAmmoChanged();
}
```

### 비고

client-side ammo refill 경로 차단.

## `Source/Pragmata_2P/Characters/HughCharacter.cpp`

### 무엇이 바뀜

- reload montage 완료 후 `SetIsReloading(false)`와 `ReplenishAmmo()` 조합 호출 제거
- `OnCompleteReload()` 단일 호출로 GripGun 서버 reload 완료 경계 사용

### 이전

```cpp
if (GripGun)
{
	//재장전 변수값 원상복구
	GripGun->SetIsReloading(false);
	//한발 장전
	GripGun->ReplenishAmmo();
}
```

### 이후

```cpp
if (GripGun)
{
	// 서버 montage 완료 시점이 GripGun gameplay reload 완료 시점
	GripGun->OnCompleteReload();
}
```

### 비고

현재 계약은 서버 montage blending-out callback이 gameplay reload 완료 clock.

## `Docs/KNOW_HOW.md`

### 무엇이 바뀜

- Damage Flow recipe에 GripGun server fire/reload 규칙 추가
- 향후 수정 시 client-side ammo/reload/damage 판정을 다시 열지 않도록 주의점 추가

### 이전

```markdown
- gun damage는 server fire trace 결과를 `FPragmataDamageSpec`으로 전달
- client gun 입력은 damage 없이 local cooldown만 시작해 중복 RPC와 recoil을 막음
```

### 이후

```markdown
- gun damage는 server fire trace 결과를 `FPragmataDamageSpec`으로 전달
- client gun 입력은 damage 없이 local cooldown만 시작해 중복 RPC와 recoil을 막음
- `AGripGun::PullTrigger()`는 non-authority 호출을 즉시 거부하고, 서버만 `CommitServerShot()`에서 ammo 소모·reload 전환·trace·damage를 확정
- `AWeaponBase::PullTrigger()`는 서버 발사 확정에 필요한 ammo/cooldown gate를 통과한 경우에만 `true` 반환
- impact VFX는 서버 trace 결과를 `MulticastSpawnImpactEffect()`로 전달해 gameplay damage와 시각 효과 경계를 분리
- `BeginServerReload()`는 서버 reload state를 열고, Hugh server montage 완료 callback이 `OnCompleteReload()`로 gameplay reload를 닫음
- `ReplenishAmmo()`와 auto replenish helper는 private 서버 helper로 유지해 외부 호출부가 ammo/reload 상태를 직접 조합하지 않게 함
```

### 비고

기존 Damage Flow recipe에 현행 규칙만 추가.

## `Docs/Architecture/architecture.md`

### 무엇이 바뀜

- Damage Flow 항목에 GripGun fire/reload/VFX authority 경계 추가

### 이전

```markdown
- weapon damage: `AWeaponBase::DealDamageToHitActor` -> `UPragmataDamageLibrary::DealDamageToActor` -> `IPragmataDamageable::ReceiveDamage`
- melee damage: `UAnimNotifyState_AttackHitWindow`가 server socket sweep 후 `DealDamageToActor` 호출
```

### 이후

```markdown
- weapon damage: `AWeaponBase::DealDamageToHitActor` -> `UPragmataDamageLibrary::DealDamageToActor` -> `IPragmataDamageable::ReceiveDamage`
- GripGun fire는 `AHughCharacter::Server_RequestFire`가 서버로 넘긴 뒤 `AGripGun::PullTrigger` authority gate와 `CommitServerShot`에서 ammo/trace/damage를 확정
- GripGun reload gameplay clock은 서버 montage 완료 callback이 `OnCompleteReload`를 호출하는 시점이며, client presentation은 ammo/reload state를 직접 바꾸지 않음
- GripGun impact VFX는 서버 trace 결과를 multicast로 표시하며 damage 판정과 분리
- melee damage: `UAnimNotifyState_AttackHitWindow`가 server socket sweep 후 `DealDamageToActor` 호출
```

### 비고

상위 architecture 문서에는 계약 요약만 기록하고 세부 recipe는 `Docs/KNOW_HOW.md`에 유지.

## 검증 결과

### staged whitespace 검증

```text
git diff --cached --check
```

결과:

```text
통과
```

### code review lane

```text
subagent code-review 재검토
```

결과:

```text
RECOMMENDATION: APPROVE
HIGH: 0
MEDIUM: 0
LOW: 0
```

### architecture lane

```text
subagent architecture 재검토
```

결과:

```text
Architectural Status: CLEAR
```

### 남은 수동 검증

```text
Unreal build/UHT/PIE 미실행
```

결과:

```text
프로젝트 지시상 명시 요청 없이 full Unreal build 실행하지 않음
listen-server PIE에서 발사, reload montage 완료 후 ammo 복구, impact VFX multicast 표시 확인 필요
```
