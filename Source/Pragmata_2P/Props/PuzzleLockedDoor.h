#pragma once

#include "CoreMinimal.h"
#include "Props/DoorBase.h"
#include "PuzzleTargetInterface.h" // 퍼즐 관련 인터페이스는 자식에서만 포함
#include "PuzzleLockedDoor.generated.h"

UCLASS()
class PRAGMATA_2P_API APuzzleLockedDoor : public ADoorBase, public IPuzzleTargetInterface
{
	GENERATED_BODY()

public:
	APuzzleLockedDoor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 인터페이스 함수 오버라이드
	virtual void OnPuzzleTriggered_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Door State|Puzzle")
	int32 GetRequiredPuzzleCount() const { return RequiredPuzzleCount; }

	UFUNCTION(BlueprintPure, Category = "Door State|Puzzle")
	int32 GetCurrentSolvedCount() const { return CurrentSolvedCount; }

protected:
	UFUNCTION()
	void OnRep_PuzzleDoorProgress();

	UFUNCTION(BlueprintImplementableEvent, Category = "Door State|Puzzle")
	void OnPuzzleProgressChanged(int32 NewSolvedCount, int32 NewRequiredCount, bool bNewUnlocked);

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_PuzzleDoorProgress, BlueprintReadWrite, Category = "Door State|Puzzle", meta = (ClampMin = "2", ClampMax = "3", UIMin = "2", UIMax = "3"))
	int32 RequiredPuzzleCount;

	UPROPERTY(ReplicatedUsing=OnRep_PuzzleDoorProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Door State|Puzzle")
	int32 CurrentSolvedCount;

	UPROPERTY(ReplicatedUsing=OnRep_PuzzleDoorProgress)
	bool bReplicatedLocked;
};
