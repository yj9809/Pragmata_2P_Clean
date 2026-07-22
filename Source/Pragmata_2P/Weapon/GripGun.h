// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "GripGun.generated.h"

class UWorld;

UCLASS()
class PRAGMATA_2P_API AGripGun : public AWeaponBase
{
	GENERATED_BODY()
public:
	AGripGun();
public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool PullTrigger() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
public:
	FORCEINLINE bool GetIsReloading() const { return bIsReloading; }
	void CancelServerReload();
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(VisibleAnywhere, Category="Weapon Data")
	float ReplenishGap = 1.f;

	// server reload state를 모든 peer presentation에 전달
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_IsReloading, Category="Weapon Data")
	bool bIsReloading = false;
	UPROPERTY()
	FTimerHandle AmmoReplenishTimerHandle;
	UPROPERTY()
	FTimerHandle ReloadTimerHandle;

private:
	// 서버가 ammo 소모, trace, damage, reload 전환을 한 번에 확정
	void CommitServerShot(UWorld* World, AController* OwnerController);

	// 서버 reload gameplay state를 열고 소유자에게 reload presentation을 요청
	void BeginServerReload();
	void FinishServerReload();

	void ReplenishAmmo();
	void StartAutoReplenish();
	void StopAutoReplenish();

	UFUNCTION()
	void OnRep_IsReloading();

	// 서버 trace 결과 impact VFX를 모든 peer에 표시
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactEffect(FVector_NetQuantize ImpactLocation, FVector_NetQuantizeNormal ImpactNormal);

	void SpawnImpactEffectAtLocation(const FVector& ImpactLocation, const FVector& ImpactNormal) const;
};
