// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OverDriveComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOverDriveChangedSignature, float, OverDrivePercent);

UCLASS( ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent) )
class PRAGMATA_2P_API UOverDriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UOverDriveComponent();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|OverDrive")
	void AddOverDrive(float Amount);
	
	UFUNCTION(BlueprintCallable, Category="Pragmata|OverDrive")
	void SetOverDrivePercent(float NewPercent);
	
	UFUNCTION(BlueprintPure, Category="Pragmata|OverDrive")
	float GetOverDrivePercent() const
	{
		return OverDrivePercent;
	}

	UFUNCTION(BlueprintCallable, Category="Pragmata|OverDrive")
	void RequestOverDrive();
	
	// 델리게이트.
public:
	UPROPERTY(BlueprintAssignable, Category="Pragmata|OverDrive")
	FOverDriveChangedSignature OnOverDriveChanged;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
private:
	UFUNCTION(Server, Reliable)
	void Server_RequestOverDrive();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayOverDriveCinematic();

	void PlayOverDriveCinematic();
	void PlayLocalDianaOverDrivePresentation();
	void EndLocalDianaOverDrivePresentation();
	void SpawnOverDriveRing(const FVector& Center);
	void FinishOverDrive();
	void AbortOverDrive();

	UFUNCTION()
	void HandleOverDriveCinematicFinished();

	UFUNCTION()
	void OnRep_OverDrivePercent();
	
	void BroadcastOverDriveChanged();
	
private:
	UPROPERTY(ReplicatedUsing=OnRep_OverDrivePercent, VisibleInstanceOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.0", ClampMax="1.0"))
	float OverDrivePercent = 0.f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|OverDrive")
	bool bActivationPending = false;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.1", Units="s"))
	float CinematicDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.1", Units="s"))
	float TimedHackDuration = 20.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.1", Units="cm"))
	float EffectRadius = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="1.0", Units="cm"))
	float RingHeightOffset = 75.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive")
	TSoftObjectPtr<class UAnimSequenceBase> OverDriveBodyAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive")
	TSoftObjectPtr<class UAnimSequenceBase> OverDriveFaceAnimation;

	TWeakObjectPtr<class APawn> CinematicInputLockedPawn;

	FTimerHandle CinematicFallbackTimerHandle;

		
};
