// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/DecoyGun.h"

#include "DecoyProjectile.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ADecoyGun::ADecoyGun()
{
	BaseDamage = 1.f;
	CoolDown = 1.f;
	//무기 획득 시 세팅 필요
	CurAmmunition = 2;
	MaxAmmunition = 2;
	MaxRange = 2000.f;
	
	//recoil
	TargetRecoilPitchAmount = 3.f;
	TargetRecoilPitch = 0.f;
	CurrentRecoilPitch = 0.f;
	LastRecoilPitch = 0.f;
	CameraUpSpeed = 30.f;
	CameraDownSpeed = 20.f;
}

void ADecoyGun::BeginPlay()
{
	Super::BeginPlay();
	WeaponType = EWeaponType::Defense;
	CurAmmunition = MaxAmmunition = 2;
}

void ADecoyGun::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool ADecoyGun::PullTrigger()
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

void ADecoyGun::CommitServerShot(UWorld* World, AController* Controller)
{
	if (CurAmmunition <= 0 || !DecoyProjectileClass) return;
	
	--CurAmmunition;
	BroadcastAmmoChanged();
	
	FVector MuzzleLocation = Mesh->GetSocketLocation(TEXT("MuzzleSocket"));
	FVector ForwardVector = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	FVector LaunchDirection = ForwardVector + FVector(0.f, 0.f, -0.1f);
	LaunchDirection.Normalize();
	
	float LaunchSpeed = 1500.f;
	FVector FinalLaunchVelocity = LaunchDirection * LaunchSpeed;

	FTransform SpawnTransform(FRotator::ZeroRotator, MuzzleLocation);
	
	ADecoyProjectile* SpawnedDecoyProjectile = GetWorld()->SpawnActorDeferred<ADecoyProjectile>(
		DecoyProjectileClass,
		SpawnTransform,
		this,
		GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	
	if (SpawnedDecoyProjectile)
	{
		//투사체 초기 속도(하강 속도 벡터) 직접 세팅
		SpawnedDecoyProjectile->InitVelocity(FinalLaunchVelocity);
		UGameplayStatics::FinishSpawningActor(SpawnedDecoyProjectile, SpawnTransform);
	}
	
	if (CurAmmunition <= 0 && OnAmmoDepletion.IsBound())
	{
		//무기 바닥에 던지는 애니메이션 재생
		OnAmmoDepletion.Broadcast();
	}
}

FVector ADecoyGun::GetProjectileExpectedHitPoint()
{
	FVector MuzzleLocation = Mesh->GetSocketLocation(TEXT("MuzzleSocket"));
	FVector ForwardVector = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	FVector LaunchDirection = ForwardVector + FVector(0.f, 0.f, -0.1f);
	LaunchDirection.Normalize();
	float LaunchSpeed = 1500.f;
	FVector InitialVelocity = LaunchDirection * LaunchSpeed;
	
	//set params
	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = MuzzleLocation;
	PredictParams.LaunchVelocity = InitialVelocity; 
	PredictParams.bTraceWithCollision = true; 
	PredictParams.ProjectileRadius = 15.f; 
	PredictParams.MaxSimTime = 5.f; 
	PredictParams.TraceChannel = ECC_Visibility; 
	PredictParams.ActorsToIgnore.Add(this); 
	PredictParams.ActorsToIgnore.Add(GetInstigator());
	
	FPredictProjectilePathResult PathResult;
	bool bIsHit = UGameplayStatics::PredictProjectilePath(this, PredictParams, PathResult);
	if (bIsHit)
		return PathResult.HitResult.ImpactPoint;

	return PathResult.LastTraceDestination.Location;
}
