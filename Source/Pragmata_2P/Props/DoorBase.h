// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/TerritoryCombatLockable.h"
#include "DoorBase.generated.h"

class USkeletalMeshComponent;
class UBoxComponent;
class UAnimationAsset;

UCLASS()
class PRAGMATA_2P_API ADoorBase : public APawn, public ITerritoryCombatLockable
{
	GENERATED_BODY()
public:
	ADoorBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void SetTerritoryCombatLocked_Implementation(bool bLocked) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Door State")
	bool bIsAnimating;

	FTimerHandle AnimationTimerHandle;

	UFUNCTION()
	virtual void OnAnimationFinished();

	UFUNCTION(BlueprintCallable, Category = "Door Action")
	virtual void UpdateDoorState();

	UFUNCTION()
	void OnRep_ShouldBeOpen();

	UFUNCTION()
	void OnRep_TerritoryCombatLocked();

	void SetDoorOpenState(bool bNewShouldBeOpen);
	void ApplyDoorOpenState(bool bNewShouldBeOpen);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* ProximityTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimationAsset* OpenAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Door State")
	bool bIsLocked;

	UPROPERTY(ReplicatedUsing=OnRep_TerritoryCombatLocked, BlueprintReadOnly, Category = "Door State")
	bool bTerritoryCombatLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Door State")
	bool bIsOpen;

	UPROPERTY(ReplicatedUsing=OnRep_ShouldBeOpen, BlueprintReadOnly, Category = "Door State")
	bool bReplicatedShouldBeOpen;

	UFUNCTION(BlueprintImplementableEvent, Category="Territory|Combat")
	void OnTerritoryCombatLockStateChanged(bool bLocked);

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION(BlueprintCallable, Category = "Door Action")
	virtual void OpenDoor();

	UFUNCTION(BlueprintCallable, Category = "Door Action")
	virtual void CloseDoor();
};
