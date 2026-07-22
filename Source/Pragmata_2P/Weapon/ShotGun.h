// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "ShotGun.generated.h"

UCLASS()
class PRAGMATA_2P_API AShotGun : public AWeaponBase
{
	GENERATED_BODY()
public:
	AShotGun();
public:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool PullTrigger() override;
	
protected:
	//서버가 탄약을 소모하고, 대미지 역시 서버에서 처리
	void CommitServerShot(UWorld* World, AController* Controller);
	bool DealShotgunDamageToActor(const FHitResult& HitResult, AController* InstigatorController);
	
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactEffect(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal);
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 TotalPellets = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	float ShotAngle = 5.f;
};
