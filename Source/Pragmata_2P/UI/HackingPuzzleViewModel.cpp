// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HackingPuzzleViewModel.h"
#include "Puzzle/MapHackingPuzzleComponent.h"

void UHackingPuzzleViewModel::BindToComponent(UMapHackingPuzzleComponent* PuzzleComp)
{
	UnbindFromComponent();

	if (!PuzzleComp) return;

	BoundPuzzleComp = PuzzleComp;

	PuzzleComp->OnHackStarted.AddUObject(this, &UHackingPuzzleViewModel::HandleHackStarted);
	PuzzleComp->OnDepthSolved.AddUObject(this, &UHackingPuzzleViewModel::HandleDepthSolved);
	PuzzleComp->OnDepthChanged.AddUObject(this, &UHackingPuzzleViewModel::HandleDepthChanged);
	PuzzleComp->OnSequenceReady.AddUObject(this, &UHackingPuzzleViewModel::HandleSequenceReady);
	PuzzleComp->OnHackReset.AddUObject(this, &UHackingPuzzleViewModel::HandleHackReset);
	PuzzleComp->OnHackCompleted.AddUObject(this, &UHackingPuzzleViewModel::HandleHackCompleted);

	SyncFromComponent();
}

void UHackingPuzzleViewModel::UnbindFromComponent()
{
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->OnHackStarted.RemoveAll(this);
		BoundPuzzleComp->OnDepthSolved.RemoveAll(this);
		BoundPuzzleComp->OnDepthChanged.RemoveAll(this);
		BoundPuzzleComp->OnSequenceReady.RemoveAll(this);
		BoundPuzzleComp->OnHackReset.RemoveAll(this);
		BoundPuzzleComp->OnHackCompleted.RemoveAll(this);
	}
	BoundPuzzleComp.Reset();
	SetIsActive(false);
	SetCurrentDepth(0);
	SetCurrentDirection(EHackDirection::None);
	SetIsSolved(false);
	SetCompletedDepthCount(0);
	SetDirection0(EHackDirection::None);
	SetDirection1(EHackDirection::None);
	SetDirection2(EHackDirection::None);
}

void UHackingPuzzleViewModel::StartMapHacking(UMapHackingPuzzleComponent* PuzzleComp)
{
	BindToComponent(PuzzleComp);

	// deprecated BP wrapper의 기존 standalone 동작 유지
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->StartHack();
	}
}

void UHackingPuzzleViewModel::StopMapHacking()
{
	// deprecated BP wrapper의 기존 hard-cancel 동작 유지
	if (BoundPuzzleComp.IsValid())
	{
		BoundPuzzleComp->CancelHack();
	}

	UnbindFromComponent();
}

void UHackingPuzzleViewModel::HandleHackStarted()
{
	SetIsActive(true);
	SetIsSolved(false);
	SetCompletedDepthCount(0);
	SetStartedTrigger(!bStartedTrigger);
}

void UHackingPuzzleViewModel::HandleDepthSolved(int32 SolvedDepth)
{
	SetCompletedDepthCount(SolvedDepth + 1);
}

void UHackingPuzzleViewModel::HandleDepthChanged(int32 Depth, EHackDirection Direction)
{
	SetCurrentDepth(Depth);
	SetCurrentDirection(Direction);
}

void UHackingPuzzleViewModel::HandleSequenceReady(const TArray<EHackDirection>& Sequence)
{
	SetDirection0(Sequence.IsValidIndex(0) ? Sequence[0] : EHackDirection::None);
	SetDirection1(Sequence.IsValidIndex(1) ? Sequence[1] : EHackDirection::None);
	SetDirection2(Sequence.IsValidIndex(2) ? Sequence[2] : EHackDirection::None);
}

void UHackingPuzzleViewModel::HandleHackReset()
{
	SetResetTrigger(!bResetTrigger);
}

void UHackingPuzzleViewModel::HandleHackCompleted()
{
	SetIsSolved(true);
	SetSolvedTrigger(!bSolvedTrigger);
	// SetIsActive(false)는 OnSuccessAnimationFinished()에서 호출
}

void UHackingPuzzleViewModel::SyncFromComponent()
{
	if (!BoundPuzzleComp.IsValid()) return;

	const EMapHackState PuzzleState = BoundPuzzleComp->GetPuzzleState();
	const TArray<EHackDirection> Sequence = BoundPuzzleComp->GetDepthSequence();

	HandleSequenceReady(Sequence);
	SetCompletedDepthCount(BoundPuzzleComp->GetCompletedDepthCount());
	SetCurrentDepth(BoundPuzzleComp->GetCurrentDepth());
	SetCurrentDirection(BoundPuzzleComp->GetCurrentDirection());
	SetIsSolved(PuzzleState == EMapHackState::Solved);
	SetIsActive(PuzzleState != EMapHackState::Idle);
	if (PuzzleState == EMapHackState::Active)
	{
		// listen server HUD는 시작 delegate를 놓칠 수 있어 현재 active 상태 동기화 때도 시작 trigger를 갱신
		SetStartedTrigger(!bStartedTrigger);
	}

	if (PuzzleState == EMapHackState::Solved)
	{
		SetSolvedTrigger(!bSolvedTrigger);
	}
}

void UHackingPuzzleViewModel::OnSuccessAnimationFinished()
{
	SetIsActive(false);
}

void UHackingPuzzleViewModel::OnFailureAnimationFinished()
{
	UnbindFromComponent();
}

void UHackingPuzzleViewModel::SetIsActive(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsActive, bNewValue);
	UE_MVVM_SET_PROPERTY_VALUE(IsActiveVisibility, bNewValue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UHackingPuzzleViewModel::SetStartedTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bStartedTrigger, bNewValue);
}

void UHackingPuzzleViewModel::SetCurrentDepth(int32 NewDepth)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentDepth, NewDepth);
}

void UHackingPuzzleViewModel::SetCurrentDirection(EHackDirection NewDir)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentDirection, NewDir);
}

void UHackingPuzzleViewModel::SetIsSolved(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsSolved, bNewValue);
}

void UHackingPuzzleViewModel::SetResetTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bResetTrigger, bNewValue);
}

void UHackingPuzzleViewModel::SetSolvedTrigger(bool bNewValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSolvedTrigger, bNewValue);
}

void UHackingPuzzleViewModel::SetCompletedDepthCount(int32 NewCount)
{
	const int32 NewCompletedDepthCount = FMath::Clamp(NewCount, 0, 3);
	constexpr float SolvedDirectionOpacity = 0.5f;

	UE_MVVM_SET_PROPERTY_VALUE(CompletedDepthCount, NewCompletedDepthCount);
	UE_MVVM_SET_PROPERTY_VALUE(Depth1SuccessRingVisibility, NewCompletedDepthCount >= 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(Depth2SuccessRingVisibility, NewCompletedDepthCount >= 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(Depth3SuccessRingVisibility, NewCompletedDepthCount >= 3 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	UE_MVVM_SET_PROPERTY_VALUE(Depth1DirectionOpacity, NewCompletedDepthCount >= 1 ? SolvedDirectionOpacity : 1.f);
	UE_MVVM_SET_PROPERTY_VALUE(Depth2DirectionOpacity, NewCompletedDepthCount >= 2 ? SolvedDirectionOpacity : 1.f);
	UE_MVVM_SET_PROPERTY_VALUE(Depth3DirectionOpacity, NewCompletedDepthCount >= 3 ? SolvedDirectionOpacity : 1.f);
}

void UHackingPuzzleViewModel::SetDirection0(EHackDirection NewDir)
{
	UE_MVVM_SET_PROPERTY_VALUE(Direction0, NewDir);
	UE_MVVM_SET_PROPERTY_VALUE(Direction0Index, DirectionToWidgetIndex(NewDir));
}

void UHackingPuzzleViewModel::SetDirection1(EHackDirection NewDir)
{
	UE_MVVM_SET_PROPERTY_VALUE(Direction1, NewDir);
	UE_MVVM_SET_PROPERTY_VALUE(Direction1Index, DirectionToWidgetIndex(NewDir));
}

void UHackingPuzzleViewModel::SetDirection2(EHackDirection NewDir)
{
	UE_MVVM_SET_PROPERTY_VALUE(Direction2, NewDir);
	UE_MVVM_SET_PROPERTY_VALUE(Direction2Index, DirectionToWidgetIndex(NewDir));
}

int32 UHackingPuzzleViewModel::DirectionToWidgetIndex(EHackDirection Direction)
{
	switch (Direction)
	{
	case EHackDirection::Up:
		return 0;
	case EHackDirection::Down:
		return 1;
	case EHackDirection::Left:
		return 2;
	case EHackDirection::Right:
		return 3;
	case EHackDirection::None:
	default:
		return 0;
	}
}
