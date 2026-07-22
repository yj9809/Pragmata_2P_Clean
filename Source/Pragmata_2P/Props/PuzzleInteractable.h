#pragma once

#include "CoreMinimal.h"
#include "Props/PropInteractionBase.h"
#include "PuzzleTargetInterface.h"
#include "Puzzle/MapHackingPuzzleComponent.h"

#include "PuzzleInteractable.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;

UCLASS()
class PRAGMATA_2P_API APuzzleInteractable : public APropInteractionBase
{
	GENERATED_BODY()

public:
	APuzzleInteractable();

	// 상호작용 키를 누를 때 호출 (UI 진입부)
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FPragmataScanResult GetPragmataScanResult_Implementation(AActor* ScanRequester) const override;

	// 퍼즐이 해결되었을 때 호출 (성공 처리부)
	UFUNCTION(BlueprintCallable, Category = "Puzzle System")
	virtual void CompletePuzzle();

	UFUNCTION(BlueprintCallable, Category = "Puzzle System")
	void ResetPuzzleForNextAttempt();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Diana interaction 후보 판정용 완료 가능 상태
	UFUNCTION(BlueprintPure, Category="Puzzle System")
	bool CanInteractWithPuzzle() const { return bInteractionEnabled && !bIsSolved && HasRemainingHackAttempts(); }

	UFUNCTION(BlueprintPure, Category="Puzzle System")
	bool HasRemainingHackAttempts() const { return !bIsRepeatable || CompletedHackCount < MaxHackCount; }

	UFUNCTION(BlueprintCallable, Category="Puzzle System")
	void SetPuzzleInteractionEnabled(bool bNewEnabled);

	void ConfigureRepeatHacking(bool bNewRepeatable, int32 NewMaxHackCount);

	// 현재 시도가 완료되어 재무장 전에는 탐색 후보에서 제외
	UFUNCTION(BlueprintPure, Category="Puzzle System")
	bool IsPuzzleSolved() const { return bIsSolved; }

	// 대형 parent actor 안 puzzle node의 허용 interaction surface
	USkeletalMeshComponent* GetPuzzleInteractionMesh() const { return PuzzleMesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> PuzzleMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UMapHackingPuzzleComponent> PuzzleComponent = nullptr;

	// 퍼즐 해결 신호를 받을 범용 타겟들 (문, 화물 박스, 레이저 등)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Puzzle System")
	TArray<AActor*> TargetActors;

	// true인 퍼즐만 재무장 후 최대 횟수 내에서 재해킹 가능
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Puzzle System|Settings", meta=(DisplayName="Allow Repeated Hacking"))
	bool bIsRepeatable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Puzzle System|Settings", meta=(EditCondition="bIsRepeatable", EditConditionHides, ClampMin="1", UIMin="1", DisplayName="Max Hack Count"))
	int32 MaxHackCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle System|Animation")
	TObjectPtr<UAnimationAsset> CompleteAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle System|Animation", meta = (ClampMin = "0.01"))
	float CompleteAnimationPlayRate = 0.5f;

	// 클라이언트가 완료 상태를 받으면 표시 매터리얼을 갱신
	UFUNCTION()
	void OnRep_IsSolved();

	UFUNCTION()
	void OnRep_InteractionEnabled();

	UFUNCTION()
	void OnRep_CompleteAnimationSerial();

	UFUNCTION()
	void OnRep_ResetAnimationSerial();

	UFUNCTION(BlueprintCallable, Category = "Puzzle System|Animation")
	virtual void PlayCompleteAnimation();

	UFUNCTION(BlueprintCallable, Category = "Puzzle System|Animation")
	virtual void PlayCompleteAnimationReverse();

	// BP 자식에서 빨강/파랑 표시 전환을 구현
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle System|State")
	void OnPuzzleSolvedChanged(bool bNewIsSolved);

	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle System|State")
	void OnPuzzleInteractionEnabledChanged(bool bNewEnabled);

	// 퍼즐 해결 완료 상태 (세이브 연동 가능)
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_IsSolved, BlueprintReadOnly, Category = "Puzzle System|State")
	bool bIsSolved;

	UPROPERTY(SaveGame, Replicated, BlueprintReadOnly, Category = "Puzzle System|State")
	int32 CompletedHackCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_InteractionEnabled, BlueprintReadOnly, Category = "Puzzle System|State")
	bool bInteractionEnabled = true;

	UPROPERTY(ReplicatedUsing = OnRep_CompleteAnimationSerial)
	int32 CompleteAnimationSerial = 0;

	UPROPERTY(ReplicatedUsing = OnRep_ResetAnimationSerial)
	int32 ResetAnimationSerial = 0;
};
