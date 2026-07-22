#include "Props/Elevator.h"

#include "Net/UnrealNetwork.h"

void AElevator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AElevator, bTerritoryCombatLocked);
}

void AElevator::SetTerritoryCombatLocked_Implementation(bool bLocked)
{
	// Territory가 전투 중인 동안에는 새 이동 상호작용만 막고 진행 중 이동은 유지
	if (!HasAuthority() || bTerritoryCombatLocked == bLocked)
	{
		return;
	}

	bTerritoryCombatLocked = bLocked;
	OnTerritoryCombatLockStateChanged(bTerritoryCombatLocked);
	ForceNetUpdate();
}

void AElevator::OnRep_TerritoryCombatLocked()
{
	// client에도 BP 잠금 연출을 갱신
	OnTerritoryCombatLockStateChanged(bTerritoryCombatLocked);
}
