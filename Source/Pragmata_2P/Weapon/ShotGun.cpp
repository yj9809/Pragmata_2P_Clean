// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/ShotGun.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "NiagaraFunctionLibrary.h"
#include "Combat/PragmataDamageLibrary.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

AShotGun::AShotGun()
{
	BaseDamage = 100.f;
	CoolDown = 1.f;
	//무기 획득 시 세팅 필요
	CurAmmunition = 4;
	MaxAmmunition = 4;
	MaxRange = 1000.f;
	WeaponType = EWeaponType::Attack;
	
	//recoil
	TargetRecoilPitchAmount = 5.f;
	TargetRecoilPitch = 0.f;
	CurrentRecoilPitch = 0.f;
	LastRecoilPitch = 0.f;
	CameraUpSpeed = 30.f;
	CameraDownSpeed = 20.f;
}

// BP default가 이전 Primary 값이어도 Hugh inventory 등록 전에 Attack 타입을 보장
void AShotGun::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	WeaponType = EWeaponType::Attack;
}

void AShotGun::BeginPlay()
{
	Super::BeginPlay();
	WeaponType = EWeaponType::Attack;
}

void AShotGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AShotGun::PullTrigger()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)return false;
	
	if ( !Super::PullTrigger()) return false;
	
	if (!HasAuthority()) return true;

	//---SERVER---
	AController* OwnerController = Cast<AController>(OwnerPawn->GetController());
	if (!OwnerController) return false;

	UWorld* World = GetWorld();
	if (!World)	return false;

	CommitServerShot(World, OwnerController);
	return true;
}

void AShotGun::CommitServerShot(UWorld* World, AController* Controller)
{
	if (CurAmmunition <= 0) return;
	
	--CurAmmunition;
	BroadcastAmmoChanged();//ui전달용
	
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	Controller->GetPlayerViewPoint(Location, Rotation);
	FVector ForwardVector = Rotation.Vector();
	Location += ForwardVector * 150.f;
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(static_cast<const AActor*>(Controller->GetPawn()));
	
	// //서버렉 방지 핵심: 10발의 팰릿 중 어떤 액터가 몇발을 맞았는지 한 프레임동안 값을 누적시키기 위함
	// TMap<AActor*, int32> HitActorMap;
	// //액터별로 가장 먼저 맞은 첫번째 탄환의 충돌정보를 대표값으로 설정
	// TMap<AActor*, FHitResult> RepresentativeHitMap;
	
	//샷건 한 발당 10개의 팰릿(구체)이 들어있음
	for (int32 i = 0; i < TotalPellets; ++i)
	{
		float ConeHalfAngleRad = FMath::DegreesToRadians(ShotAngle);
		//정면 벡터 (forwardvector)를 기준, 라디안각도 크기의 원뿔 공간에서 무작위로 벡터(shotdir)를 생성.
		FVector ShotDir = FMath::VRandCone(ForwardVector, ConeHalfAngleRad);
		FVector End = Location + ShotDir * MaxRange;
		
		FHitResult HitResult;
		bool bIsHit = World->LineTraceSingleByChannel(
			HitResult,
			Location,
			End,
			ECC_GameTraceChannel1,
			QueryParams);
		
		DrawDebugLine(World, Location, bIsHit ? HitResult.ImpactPoint : End, FColor::Red, 
			false, 1.f, 0, 0.1f);
		
		if (bIsHit)
		{
			DealShotgunDamageToActor(HitResult, Controller);
			MulticastSpawnImpactEffect(HitResult.Location, HitResult.ImpactNormal);
		}
	}
	
	if (CurAmmunition <= 0 && OnAmmoDepletion.IsBound())
	{
		//무기 바닥에 던지는 애니메이션 재생
		OnAmmoDepletion.Broadcast();
	}
}

bool AShotGun::DealShotgunDamageToActor(const FHitResult& HitResult, AController* InstigatorController)
{
	if (!InstigatorController) return false;
	
	const UPragmataPlayerStatsComponent* PlayerStatsComponent =
		GetOwner() ? GetOwner()->FindComponentByClass<UPragmataPlayerStatsComponent>() : nullptr;
	
	const float DamageMultiplier = PlayerStatsComponent ? PlayerStatsComponent->GetPlayerDamageMultiplier() : 1.f;
	
	FPragmataDamageSpec DamageSpec;
	DamageSpec.DamageAmount = BaseDamage * FMath::Max(0.f, DamageMultiplier);;
	DamageSpec.SourceActor = GetOwner();
	DamageSpec.DamageCauser = this;
	DamageSpec.InstigatorController = InstigatorController;
	DamageSpec.DamageKind = EPragmataDamageKind::Weapon;
	DamageSpec.HitReactStrength = EPragmataHitReactStrength::Medium;
	DamageSpec.bHasImpact = true;
	DamageSpec.ImpactHit = HitResult;
	DamageSpec.ImpactDirection = (HitResult.ImpactPoint - HitResult.TraceStart).GetSafeNormal();
	
	return UPragmataDamageLibrary::DealDamageToActor(HitResult.GetActor(), DamageSpec);
}

void AShotGun::MulticastSpawnImpactEffect_Implementation(FVector_NetQuantize ImpactLocation,
	FVector_NetQuantizeNormal ImpactNormal)
{
	if (GetNetMode() == NM_DedicatedServer || !ImpactEffect || !GetWorld())
	{
		return;
	}

	// 맞은 벽면의 방향을 바라보도록 impact normal을 회전값으로 변환
	const FRotator ImpactRotation = ImpactNormal.Rotation();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		ImpactEffect,
		ImpactLocation,
		ImpactRotation);
}
