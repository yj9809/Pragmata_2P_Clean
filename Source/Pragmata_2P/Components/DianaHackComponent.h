#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "TimerManager.h"
#include "DianaHackComponent.generated.h"

class UEnemyHackingPuzzleComponent;

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UDianaHackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDianaHackComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana|Hack")
	void RequestHack(AActor* TargetActor);

	void RequestHackDirection(EHackDirection Direction);
	void RequestCancelHack(AActor* ExpectedTarget = nullptr);
	bool IsEnemyHackActive() const;
	AActor* GetActiveEnemyHackTarget() const;

	UFUNCTION(BlueprintPure, Category="Pragmata|Diana|Hack")
	float GetHackDamage() const;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana|Hack")
	void SetHackDamageUpgradeLevel(int32 NewLevel);

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestHack(AActor* TargetActor);

	UFUNCTION(Server, Reliable)
	void Server_ProcessEnemyHackDirection(EHackDirection Direction);

	UFUNCTION(Server, Reliable)
	void Server_CancelEnemyHack(AActor* ExpectedTarget);

	void HandleEnemyHackCompleted();
	void HandleDecodeNodeVisited();
	void ClearActiveEnemyHack(bool bCancelPuzzle, bool bClearReplicatedTarget = true);
	void ScheduleCompletedHackHudClear(AActor* CompletedTarget);
	void ClearCompletedHackHudTarget();
	void CancelCompletedHackHudClearTimer();
	void SetReplicatedEnemyHackTarget(AActor* TargetActor) const;
	bool ValidateHackTarget(AActor* TargetActor, FVector& OutCameraOrigin, FVector& OutCameraDirection) const;
	bool HasLineOfSightToTarget(AActor* TargetActor, FVector CameraOrigin) const;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0"))
	float BaseHackDamage = 25.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0"))
	float HackDamagePerUpgrade = 5.f;
	
	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0", ClampMax="1"))
	float OverDrivePerHack = 0.2f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Diana Hack")
	int32 HackDamageUpgradeLevel = 0;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0"))
	float MaxHackDistance = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0"))
	float MaxCameraDistanceFromHugh = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Hack", meta=(ClampMin="0"))
	float CompletedHackHudHoldSeconds = 1.25f;

	UPROPERTY()
	TObjectPtr<AActor> ActiveHackTarget = nullptr;

	UPROPERTY()
	TObjectPtr<UEnemyHackingPuzzleComponent> ActivePuzzleComponent = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> PendingCompletedHackTarget = nullptr;

	FTimerHandle CompletedHackHudClearTimerHandle;
};
