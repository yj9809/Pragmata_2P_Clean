#include "Props/PuzzleLockedDoor.h"

#include "Net/UnrealNetwork.h"

APuzzleLockedDoor::APuzzleLockedDoor()
{
	bReplicates = true;
	bIsLocked = true;
	bReplicatedLocked = bIsLocked;
	RequiredPuzzleCount = 3;
	CurrentSolvedCount = 0;
}

void APuzzleLockedDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(APuzzleLockedDoor, RequiredPuzzleCount);
	DOREPLIFETIME(APuzzleLockedDoor, CurrentSolvedCount);
	DOREPLIFETIME(APuzzleLockedDoor, bReplicatedLocked);
}

void APuzzleLockedDoor::OnPuzzleTriggered_Implementation()
{
	if (!HasAuthority() || !bIsLocked) return;

	RequiredPuzzleCount = (RequiredPuzzleCount >= 2 && RequiredPuzzleCount <= 3) ? RequiredPuzzleCount : 3;

	// 퍼즐별 위치는 구분하지 않고 완료 개수만 문 표시와 잠금 해제에 사용
	CurrentSolvedCount = FMath::Min(CurrentSolvedCount + 1, RequiredPuzzleCount);
	const bool bShouldUnlock = CurrentSolvedCount >= RequiredPuzzleCount;

	if (bShouldUnlock)
	{
		bIsLocked = false;
	}
	bReplicatedLocked = bIsLocked;

	OnPuzzleProgressChanged(CurrentSolvedCount, RequiredPuzzleCount, !bIsLocked);
	ForceNetUpdate();

	if (!bIsLocked)
	{
		UpdateDoorState();
	}
}

void APuzzleLockedDoor::OnRep_PuzzleDoorProgress()
{
	bIsLocked = bReplicatedLocked;
	OnPuzzleProgressChanged(CurrentSolvedCount, RequiredPuzzleCount, !bIsLocked);
}
