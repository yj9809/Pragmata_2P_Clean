// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/StasisNet.h"

#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Weapon/StasisNetField.h"

AStasisNet::AStasisNet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CurAmmunition = MaxAmmunition = 3;
	BaseDamage = 20.f;
	HackedDamage = 30.f;
	CoolDown = 1.f;
	MaxRange = 5000.f;
	WeaponType = EWeaponType::Tactical;
	StasisFieldClass = AStasisNetField::StaticClass();
}

// BP default나 merge된 asset 설정이 tick을 꺼도 조준 preview는 계속 갱신
void AStasisNet::BeginPlay()
{
	Super::BeginPlay();

	SetActorTickEnabled(true);
}

void AStasisNet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	DrawAimPreview();
}

// 서버 발사 확정 지점에서 ammo를 소모하고 preview trace와 같은 위치에 field를 생성
bool AStasisNet::PullTrigger()
{
	if (!HasAuthority())
	{
		return false;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}

	AController* OwnerController = OwnerPawn->GetController();
	if (!OwnerController)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || !Super::PullTrigger())
	{
		return false;
	}

	--CurAmmunition;
	BroadcastAmmoChanged();

	FHitResult Hit;
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	const bool bBlockingHit = ResolveAimTrace(World, OwnerController, Hit, TraceStart, TraceEnd);
	SpawnStasisField(World, OwnerController, bBlockingHit ? Hit.ImpactPoint : TraceEnd);
	
	if (CurAmmunition <= 0 && OnAmmoDepletion.IsBound())
	{
		//무기 바닥에 던지는 애니메이션 재생
		OnAmmoDepletion.Broadcast();
	}
	return true;
}

// owner 복제나 attach 갱신 타이밍이 달라도 preview 기준 Hugh를 찾음
const AHughCharacter* AStasisNet::ResolvePreviewHughOwner() const
{
	if (const AHughCharacter* OwnerHugh = Cast<AHughCharacter>(GetOwner()))
	{
		return OwnerHugh;
	}

	if (const AHughCharacter* InstigatorHugh = Cast<AHughCharacter>(GetInstigator()))
	{
		return InstigatorHugh;
	}

	return Cast<AHughCharacter>(GetAttachParentActor());
}

// 조준 preview는 StasisNet을 장착하고 조준 중인 local Hugh 화면에만 표시
bool AStasisNet::IsLocalPreviewOwner() const
{
	const AHughCharacter* HughOwner = ResolvePreviewHughOwner();
	if (!HughOwner || !HughOwner->IsLocallyControlled() || !HughOwner->GetIsAiming())
	{
		return false;
	}

	const AWeaponBase* EquippedWeapon = HughOwner->GetCurrentWeapon();
	return EquippedWeapon == this || (EquippedWeapon && EquippedWeapon->GetWeaponType() == EWeaponType::Tactical);
}

// socket이 없어도 skeletal mesh bone 이름을 총구 기준점으로 사용
FVector AStasisNet::ResolveMuzzleTraceStart() const
{
	const bool bHasMuzzleAnchor = Mesh
		&& !MuzzleSocketName.IsNone()
		&& (Mesh->DoesSocketExist(MuzzleSocketName) || Mesh->GetBoneIndex(MuzzleSocketName) != INDEX_NONE);
	if (bHasMuzzleAnchor)
	{
		return Mesh->GetSocketTransform(MuzzleSocketName, RTS_World).TransformPosition(MuzzleLocalOffset);
	}

	return GetActorTransform().TransformPosition(MuzzleLocalOffset);
}

// HUD 중앙 reticle 위치를 먼저 찾고, 실제 판정선은 총구에서 그 지점을 향해 발사
bool AStasisNet::ResolveAimTrace(
	UWorld* World,
	AController* OwnerController,
	FHitResult& OutHit,
	FVector& OutTraceStart,
	FVector& OutTraceEnd) const
{
	if (!World || !OwnerController)
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	FVector CameraTraceStart = FVector::ZeroVector;
	OwnerController->GetPlayerViewPoint(CameraTraceStart, ViewRotation);
	const FVector CameraTraceEnd = CameraTraceStart + ViewRotation.Vector() * MaxRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StasisNetAimTrace), true);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult CameraHit;
	const bool bCameraBlockingHit = World->LineTraceSingleByChannel(
		CameraHit,
		CameraTraceStart,
		CameraTraceEnd,
		ECollisionChannel::ECC_GameTraceChannel1,
		QueryParams);
	const FVector ReticleWorldPoint = bCameraBlockingHit ? CameraHit.ImpactPoint : CameraTraceEnd;

	OutTraceStart = ResolveMuzzleTraceStart();
	FVector FireDirection = ReticleWorldPoint - OutTraceStart;
	if (!FireDirection.Normalize())
	{
		FireDirection = ViewRotation.Vector();
	}
	OutTraceEnd = OutTraceStart + FireDirection * MaxRange;

	return World->LineTraceSingleByChannel(
		OutHit,
		OutTraceStart,
		OutTraceEnd,
		ECollisionChannel::ECC_GameTraceChannel1,
		QueryParams);
}

// 임시 HUD/VFX 대체용 월드 debug 조준 표시
void AStasisNet::DrawAimPreview() const
{
	if (!IsLocalPreviewOwner())
	{
		return;
	}

	UWorld* World = GetWorld();
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
	if (!World || !OwnerController)
	{
		return;
	}

	FHitResult Hit;
	FVector TraceStart = FVector::ZeroVector;
	FVector TraceEnd = FVector::ZeroVector;
	const bool bBlockingHit = ResolveAimTrace(World, OwnerController, Hit, TraceStart, TraceEnd);
	const FVector AimPoint = bBlockingHit ? Hit.ImpactPoint : TraceEnd;

	DrawDebugLine(
		World,
		TraceStart,
		AimPoint,
		FColor::Blue,
		false,
		AimDebugLifetime,
		0,
		AimDebugLineThickness);

	DrawDebugSphere(
		World,
		AimPoint,
		AimDebugSphereRadius,
		16,
		bBlockingHit ? FColor::Red : FColor::Blue,
		false,
		AimDebugLifetime);
}

// projectile 없이 명중 위치에 지속 field actor만 생성
void AStasisNet::SpawnStasisField(UWorld* World, AController* OwnerController, const FVector& FieldLocation)
{
	if (!World || !StasisFieldClass)
	{
		return;
	}

	FTransform SpawnTransform(FRotator::ZeroRotator, FieldLocation);
	AStasisNetField* Field = World->SpawnActorDeferred<AStasisNetField>(
		StasisFieldClass,
		SpawnTransform,
		this,
		Cast<APawn>(GetOwner()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Field)
	{
		return;
	}

	// field actor가 BeginPlay 전에 weapon damage 값을 받아 dot damage 기준을 고정
	Field->InitializeField(
		GetOwner(),
		OwnerController,
		BaseDamage,
		HackedDamage,
		FieldRadius,
		FieldDuration,
		StunDuration,
		DamageInterval);
	Field->FinishSpawning(SpawnTransform);
}
