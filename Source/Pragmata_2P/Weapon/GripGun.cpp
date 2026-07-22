// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/GripGun.h"

#include "Characters/HughCharacter.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AGripGun::AGripGun()
{
	CurAmmunition = MaxAmmunition = 10;
	BaseDamage = 50;
	CoolDown = 0.4f;
	MaxRange = 3000.f;
	
	//recoil
	TargetRecoilPitchAmount = 2.f;
	TargetRecoilPitch = 0.f;
	CurrentRecoilPitch = 0.f;
	LastRecoilPitch = 0.f;
	CameraUpSpeed = 30.f;
	CameraDownSpeed = 10.f;
}

void AGripGun::BeginPlay()
{
	Super::BeginPlay();
	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AGripGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGripGun::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	GetWorldTimerManager().ClearTimer(AmmoReplenishTimerHandle);
	Super::EndPlay(EndPlayReason);
}

bool AGripGun::PullTrigger()
{
	if (!HasAuthority() || bIsReloading)
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

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	OwnerController->GetPlayerViewPoint(Location, Rotation);

	//카메라의 위치에서 카메라가 바라보는 방향벡터에 총의 사거리만큼 곱해서 새로운 벡터 계산
	FVector End = Location + Rotation.Vector() * MaxRange;

	FHitResult Hit;
	//linetracing
	bool bSuccess = World->LineTraceSingleByChannel(
		Hit,
		Location,
		End,
		ECollisionChannel::ECC_GameTraceChannel1);

	if (bSuccess)
	{
		DealDamageToHitActor(Hit, OwnerController);
		MulticastSpawnImpactEffect(Hit.Location, Hit.ImpactNormal);

		AActor* HitActor = Hit.GetActor();
		if (HitActor)
		{
			// Physics Asset 약점 설정은 이 로그의 Component와 BoneName을 그대로 기준으로 사용
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Hit Actor Name: %s, Component: %s, BoneName: %s"),
				*HitActor->GetName(),
				*GetNameSafe(Hit.GetComponent()),
				*Hit.BoneName.ToString());
		}
	}

	// if (CameraShakeClass)
	// {
	// 	// OwnerController를 통해 플레이어 컨트롤러를 가져옵니다.
	// 	APlayerController* PlayerController = Cast<APlayerController>(OwnerController);
	// 	if (PlayerController && PlayerController->PlayerCameraManager)
	// 	{
	// 		// 세 번째 인자는 흔들림의 세기 배율(Scale)입니다. 1.0f이 기본값입니다.
	// 		PlayerController->PlayerCameraManager->StartCameraShake(CameraShakeClass, 0.4f);
	// 	}
	// }
}

void AGripGun::BeginServerReload()
{
	if (!HasAuthority() || bIsReloading || !GetWorld())
	{
		return;
	}

	bIsReloading = true;
	StopAutoReplenish();
	ForceNetUpdate();
	OnRep_IsReloading();

	AHughCharacter* OwnerHugh = Cast<AHughCharacter>(GetOwner());
	const float ReloadDuration = OwnerHugh
		? OwnerHugh->GetGripGunReloadDuration()
		: 1.f;
	GetWorldTimerManager().SetTimer(
		ReloadTimerHandle,
		this,
		&AGripGun::FinishServerReload,
		FMath::Max(ReloadDuration, 0.1f),
		false);
}

// auto replenish는 한 번에 1발만 보충하고 다음 tick을 다시 예약
void AGripGun::ReplenishAmmo()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CurAmmunition >= MaxAmmunition)
	{
		StopAutoReplenish();
		return;
	}
	++CurAmmunition;
	BroadcastAmmoChanged();
	
	if (CurAmmunition >= MaxAmmunition)
	{
		StopAutoReplenish();
	}
	else
	{
		StartAutoReplenish();
	}
}

// reload 완료 경로를 하나로 모아 ammo와 reload lock을 함께 갱신
void AGripGun::FinishServerReload()
{
	if (!HasAuthority() || !bIsReloading)
	{
		return;
	}

	// montage callback과 무관하게 server timer가 ammo와 reload lock을 함께 확정
	bIsReloading = false;
	CurAmmunition = 1;
	BroadcastAmmoChanged();
	StartAutoReplenish();
	ForceNetUpdate();
	OnRep_IsReloading();
}

void AGripGun::CancelServerReload()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	bIsReloading = false;
	ForceNetUpdate();
	OnRep_IsReloading();
}

// loop timer 대신 one-shot 예약만 유지해 reload state와 timer가 엇갈리지 않게 함
void AGripGun::StartAutoReplenish()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	if (bIsReloading || CurAmmunition >= MaxAmmunition)
	{
		StopAutoReplenish();
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AmmoReplenishTimerHandle,
		this,
		&AGripGun::ReplenishAmmo,
		ReplenishGap,
		false);
}

void AGripGun::OnRep_IsReloading()
{
	if (AHughCharacter* OwnerHugh = Cast<AHughCharacter>(GetOwner()))
	{
		OwnerHugh->ApplyGripGunReloadPresentation(bIsReloading);
	}
}

void AGripGun::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGripGun, bIsReloading);
}

void AGripGun::StopAutoReplenish()
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AmmoReplenishTimerHandle);
}

void AGripGun::MulticastSpawnImpactEffect_Implementation(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal)
{
	SpawnImpactEffectAtLocation(ImpactLocation, ImpactNormal);
}

void AGripGun::SpawnImpactEffectAtLocation(const FVector& ImpactLocation, const FVector& ImpactNormal) const
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
