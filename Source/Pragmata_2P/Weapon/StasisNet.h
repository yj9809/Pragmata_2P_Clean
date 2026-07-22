// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapon/WeaponBase.h"
#include "StasisNet.generated.h"

class AStasisNetField;
class AHughCharacter;
class UWorld;

UCLASS()
class PRAGMATA_2P_API AStasisNet : public AWeaponBase
{
	GENERATED_BODY()

public:
	AStasisNet();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool PullTrigger() override;

protected:
	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis")
	TSubclassOf<AStasisNetField> StasisFieldClass;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float FieldRadius = 450.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float FieldDuration = 10.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float StunDuration = 5.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float DamageInterval = 1.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis", meta=(ClampMin="0.0", UIMin="0.0"))
	float HackedDamage = 30.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis|Muzzle")
	FName MuzzleSocketName = TEXT("muzzle");

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis|Muzzle")
	FVector MuzzleLocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis|Debug", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float AimDebugSphereRadius = 24.f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis|Debug", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float AimDebugLifetime = 0.03f;

	UPROPERTY(EditAnywhere, Category="Weapon Data|Stasis|Debug", meta=(ClampMin="0.0", UIMin="0.0"))
	float AimDebugLineThickness = 1.5f;

private:
	const AHughCharacter* ResolvePreviewHughOwner() const;
	bool IsLocalPreviewOwner() const;
	FVector ResolveMuzzleTraceStart() const;
	bool ResolveAimTrace(UWorld* World, AController* OwnerController, FHitResult& OutHit, FVector& OutTraceStart, FVector& OutTraceEnd) const;
	void DrawAimPreview() const;
	void SpawnStasisField(UWorld* World, AController* OwnerController, const FVector& FieldLocation);

};
