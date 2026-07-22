#include "Components/PragmataFactionComponent.h"

#include "Net/UnrealNetwork.h"

UPragmataFactionComponent::UPragmataFactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UPragmataFactionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPragmataFactionComponent, Faction);
}

void UPragmataFactionComponent::SetFaction(EPragmataFaction NewFaction)
{
	if (AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		Faction = NewFaction;
	}
}

bool UPragmataFactionComponent::IsEnemyFaction(EPragmataFaction OtherFaction) const
{
	return Faction != EPragmataFaction::Neutral && OtherFaction != EPragmataFaction::Neutral && Faction != OtherFaction;
}
