// Fill out your copyright notice in the Description page of Project Settings.

#include "Puzzle/MapHackingPuzzleComponent.h"

#include "Net/UnrealNetwork.h"

UMapHackingPuzzleComponent::UMapHackingPuzzleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMapHackingPuzzleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMapHackingPuzzleComponent, TotalDepths);
	DOREPLIFETIME(UMapHackingPuzzleComponent, PuzzleState);
	DOREPLIFETIME(UMapHackingPuzzleComponent, CurrentDepth);
	DOREPLIFETIME(UMapHackingPuzzleComponent, CompletedDepthCount);
	DOREPLIFETIME(UMapHackingPuzzleComponent, DepthSequence);
	DOREPLIFETIME(UMapHackingPuzzleComponent, DepthSolvedCounter);
	DOREPLIFETIME(UMapHackingPuzzleComponent, ResetCounter);
}

bool UMapHackingPuzzleComponent::StartHack()
{
	if (!HasPuzzleAuthority()) return false;
	if (PuzzleState != EMapHackState::Idle) return false;

	GenerateSequence();
	CurrentDepth = 0;
	CompletedDepthCount = 0;
	PuzzleState = EMapHackState::Active;

	BroadcastStarted();
	return true;
}

void UMapHackingPuzzleComponent::ProcessDirectionInput(EHackDirection InputDirection)
{
	if (!HasPuzzleAuthority()) return;
	if (PuzzleState != EMapHackState::Active) return;
	if (!DepthSequence.IsValidIndex(CurrentDepth)) return;

	if (InputDirection == DepthSequence[CurrentDepth])
	{
		NotifyDepthSolved(CurrentDepth);

		if (CurrentDepth + 1 >= TotalDepths)
		{
			CompleteHack();
		}
		else
		{
			++CurrentDepth;
			NotifyDepthChanged();
		}
	}
	else
	{
		ResetHack();
	}
}

void UMapHackingPuzzleComponent::CancelHack()
{
	if (!HasPuzzleAuthority()) return;

	PuzzleState = EMapHackState::Idle;
	CurrentDepth = 0;
	CompletedDepthCount = 0;
}

EHackDirection UMapHackingPuzzleComponent::GetCurrentDirection() const
{
	return DepthSequence.IsValidIndex(CurrentDepth) ? DepthSequence[CurrentDepth] : EHackDirection::None;
}

void UMapHackingPuzzleComponent::GenerateSequence()
{
	DepthSequence.Reset();
	const int32 ClampedTotalDepths = FMath::Clamp(TotalDepths, 1, 3);
	TotalDepths = ClampedTotalDepths;

	for (int32 i = 0; i < ClampedTotalDepths; ++i)
	{
		DepthSequence.Add(static_cast<EHackDirection>(FMath::RandRange(0, 3)));
	}
}

void UMapHackingPuzzleComponent::ResetHack()
{
	PuzzleState = EMapHackState::Idle;
	CurrentDepth = 0;
	CompletedDepthCount = 0;
	++ResetCounter;

	OnHackReset.Broadcast();
}

void UMapHackingPuzzleComponent::CompleteHack()
{
	PuzzleState = EMapHackState::Solved;
	OnHackCompleted.Broadcast();
}

void UMapHackingPuzzleComponent::OnRep_PuzzleState(EMapHackState OldPuzzleState)
{
	if (PuzzleState == OldPuzzleState) return;

	if (PuzzleState == EMapHackState::Active && OldPuzzleState != EMapHackState::Active)
	{
		BroadcastStarted();
	}
	else if (PuzzleState == EMapHackState::Solved)
	{
		OnHackCompleted.Broadcast();
	}
}

void UMapHackingPuzzleComponent::OnRep_CurrentDepth()
{
	NotifyDepthChanged();
}

void UMapHackingPuzzleComponent::OnRep_DepthSolvedCounter()
{
	if (CompletedDepthCount <= 0) return;

	OnDepthSolved.Broadcast(CompletedDepthCount - 1);
}

void UMapHackingPuzzleComponent::OnRep_ResetCounter()
{
	OnHackReset.Broadcast();
}

bool UMapHackingPuzzleComponent::HasPuzzleAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UMapHackingPuzzleComponent::BroadcastStarted()
{
	OnSequenceReady.Broadcast(DepthSequence);
	OnHackStarted.Broadcast();
	NotifyDepthChanged();
}

void UMapHackingPuzzleComponent::NotifyDepthSolved(int32 SolvedDepth)
{
	CompletedDepthCount = FMath::Clamp(SolvedDepth + 1, 0, TotalDepths);
	++DepthSolvedCounter;

	OnDepthSolved.Broadcast(SolvedDepth);
}

void UMapHackingPuzzleComponent::NotifyDepthChanged()
{
	if (PuzzleState == EMapHackState::Active)
	{
		OnDepthChanged.Broadcast(CurrentDepth, GetCurrentDirection());
	}
}
