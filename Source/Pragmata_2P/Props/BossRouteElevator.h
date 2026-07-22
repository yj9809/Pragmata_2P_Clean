#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/TerritoryCombatLockable.h"
#include "Props/PuzzleTargetInterface.h"
#include "BossRouteElevator.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class APuzzleInteractable;
class AMonsterBase;

UENUM(BlueprintType)
enum class EBossRouteElevatorState : uint8
{
	WaitingForPuzzle UMETA(DisplayName="Waiting For Puzzle"),
	Moving UMETA(DisplayName="Moving"),
	WaitingForCombat UMETA(DisplayName="Waiting For Combat"),
	Completed UMETA(DisplayName="Completed")
};

USTRUCT(BlueprintType)
struct FBossRouteElevatorStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator", meta=(ClampMin="0"))
	float MoveDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator", meta=(ClampMin="0.01"))
	float MoveDuration = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator")
	bool bRequiresCombatClear = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator|Legacy", meta=(DeprecatedProperty, DeprecationMessage="Use MonsterTerritoryActor combat lock targets and monster registration instead."))
	TArray<TObjectPtr<AActor>> RequiredCombatActors;

};

UCLASS()
class PRAGMATA_2P_API ABossRouteElevator : public AActor, public IPuzzleTargetInterface, public ITerritoryCombatLockable
{
	GENERATED_BODY()

public:
	ABossRouteElevator();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnPuzzleTriggered_Implementation() override;
	virtual void SetTerritoryCombatLocked_Implementation(bool bLocked) override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Boss Route Elevator")
	void NotifyCombatCleared();

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Route Elevator")
	EBossRouteElevatorState GetElevatorState() const { return ElevatorState; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Boss Route Elevator")
	int32 GetCurrentStageIndex() const { return CurrentStageIndex; }

	UStaticMeshComponent* GetElevatorMesh() const { return ElevatorMesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> ElevatorMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator")
	TArray<FBossRouteElevatorStage> Stages;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator")
	FVector MoveDirection = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator")
	bool bStartPuzzleAvailable = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator")
	TArray<TObjectPtr<APuzzleInteractable>> ControlledPuzzleActors;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator|Puzzle", meta=(DisplayName="Allow Repeated Puzzle Hacking"))
	bool bAllowRepeatedPuzzleHacking = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Boss Route Elevator|Puzzle", meta=(EditCondition="bAllowRepeatedPuzzleHacking", EditConditionHides, ClampMin="1", UIMin="1", DisplayName="Max Puzzle Hack Count"))
	int32 MaxPuzzleHackCount = 2;

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnPuzzleAvailabilityChanged(bool bNewAvailable);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnElevatorStateChanged(EBossRouteElevatorState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnStageMovementStarted(int32 StageIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnStageMovementFinished(int32 StageIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnCombatStarted(int32 StageIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnCombatCleared(int32 StageIndex);

	UFUNCTION(BlueprintImplementableEvent, Category="Pragmata|Boss Route Elevator")
	void OnElevatorCompleted();

private:
	UFUNCTION()
	void OnRep_ElevatorState();

	UFUNCTION()
	void OnRep_CurrentStageIndex();

	UFUNCTION()
	void OnRep_TargetLocation();

	UFUNCTION()
	void OnRep_PuzzleAvailable();

	UFUNCTION()
	void HandleControlledPuzzleActivated(AActor* Interactor);

	UFUNCTION()
	void HandleRequiredEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleRequiredEnemyDeath();

	void SetElevatorState(EBossRouteElevatorState NewState);
	void SetPuzzleAvailable(bool bNewAvailable);
	void StartCurrentStageMove();
	void FinishCurrentStageMove();
	void FinishCurrentStage();
	void StartStageCombat();
	void ClearStageEnemyBindings();
	void CheckCombatClearCondition();
	void CollectRequiredCombatMonsters(AActor* CombatActor);
	void RegisterRequiredCombatMonster(AMonsterBase* MonsterActor);
	FVector GetStageMoveOffset(const FBossRouteElevatorStage& Stage) const;
	void ApplyReplicatedLocationState();
	void CollectAttachedPuzzleActors();
	void BindControlledPuzzleActors();
	void ApplyRepeatHackSettingsToControlledPuzzleActors() const;
	void ResetControlledPuzzleActorsForNextStage() const;
	void ApplyPuzzleAvailabilityToControlledActors() const;

	UPROPERTY(ReplicatedUsing=OnRep_ElevatorState)
	EBossRouteElevatorState ElevatorState = EBossRouteElevatorState::WaitingForPuzzle;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentStageIndex)
	int32 CurrentStageIndex = 0;

	UPROPERTY(ReplicatedUsing=OnRep_TargetLocation)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(ReplicatedUsing=OnRep_PuzzleAvailable)
	bool bPuzzleAvailable = true;

	FVector MoveStartLocation = FVector::ZeroVector;
	FVector MoveTargetLocation = FVector::ZeroVector;
	float MoveElapsedTime = 0.f;
	float ActiveMoveDuration = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AMonsterBase>> ActiveRequiredEnemies;

	bool bTerritoryCombatLocked = false;
};
