// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "MapHackingPuzzleComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnHackPuzzleStarted);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHackDepthSolved, int32);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnHackDepthChanged, int32, EHackDirection);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHackSequenceReady, const TArray<EHackDirection>&);
DECLARE_MULTICAST_DELEGATE(FOnHackPuzzleReset);
DECLARE_MULTICAST_DELEGATE(FOnHackPuzzleCompleted);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UMapHackingPuzzleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMapHackingPuzzleComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 총 뎁스 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Pragmata|Puzzle", meta=(ClampMin="1", ClampMax="3", UIMin="1", UIMax="3"))
	int32 TotalDepths = 3;

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	bool StartHack();

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	void ProcessDirectionInput(EHackDirection InputDirection);

	UFUNCTION(BlueprintCallable, Category = "Pragmata|Puzzle")
	void CancelHack();

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	EMapHackState GetPuzzleState() const { return PuzzleState; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetCurrentDepth() const { return CurrentDepth; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	EHackDirection GetCurrentDirection() const;

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	int32 GetCompletedDepthCount() const { return CompletedDepthCount; }

	UFUNCTION(BlueprintPure, Category = "Pragmata|Puzzle")
	TArray<EHackDirection> GetDepthSequence() const { return DepthSequence; }

	// ViewModel이 구독하는 내부 델리게이트
	FOnHackPuzzleStarted OnHackStarted;
	FOnHackDepthSolved OnDepthSolved;
	FOnHackDepthChanged OnDepthChanged;
	FOnHackSequenceReady OnSequenceReady;
	FOnHackPuzzleReset OnHackReset;
	FOnHackPuzzleCompleted OnHackCompleted;

private:
	UFUNCTION()
	void OnRep_PuzzleState(EMapHackState OldPuzzleState);

	UFUNCTION()
	void OnRep_CurrentDepth();

	UFUNCTION()
	void OnRep_DepthSolvedCounter();

	UFUNCTION()
	void OnRep_ResetCounter();

	bool HasPuzzleAuthority() const;
	void BroadcastStarted();
	void NotifyDepthSolved(int32 SolvedDepth);
	void NotifyDepthChanged();

	UPROPERTY(ReplicatedUsing = OnRep_PuzzleState)
	EMapHackState PuzzleState = EMapHackState::Idle;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentDepth)
	int32 CurrentDepth = 0;

	UPROPERTY(Replicated)
	int32 CompletedDepthCount = 0;

	UPROPERTY(Replicated)
	TArray<EHackDirection> DepthSequence;

	UPROPERTY(ReplicatedUsing = OnRep_DepthSolvedCounter)
	uint8 DepthSolvedCounter = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ResetCounter)
	uint8 ResetCounter = 0;

	void GenerateSequence();
	void ResetHack();
	void CompleteHack();
};
