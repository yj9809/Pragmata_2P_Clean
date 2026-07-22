// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "DecoyGun.generated.h"

class ADecoyProjectile;
UCLASS()
class PRAGMATA_2P_API ADecoyGun : public AWeaponBase
{
	GENERATED_BODY()
public:
	ADecoyGun();
public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool PullTrigger() override;
protected:
	//서버가 탄약을 소모하고, 대미지 역시 서버에서 처리
	void CommitServerShot(UWorld* World, AController* Controller);
	FVector GetProjectileExpectedHitPoint();
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	float ShotAngle = 5.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decoy")
	TSubclassOf<ADecoyProjectile> DecoyProjectileClass;
};
