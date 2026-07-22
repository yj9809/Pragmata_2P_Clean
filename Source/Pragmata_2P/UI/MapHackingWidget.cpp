// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/MapHackingWidget.h"

#include "Components/Widget.h"
#include "GameFramework/Actor.h"
#include "MVVMSubsystem.h"
#include "Puzzle/MapHackingPuzzleComponent.h"
#include "UI/HackingPuzzleViewModel.h"
#include "View/MVVMView.h"

void UMapHackingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetMapHackingVisible(false);
}

void UMapHackingWidget::SetMapHackingVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	if (Root)
	{
		Root->SetVisibility(NewVisibility);
		return;
	}

	if (MapHackingRoot)
	{
		MapHackingRoot->SetVisibility(NewVisibility);
		return;
	}

	SetVisibility(NewVisibility);
}

void UMapHackingWidget::SetMapPuzzleActor(AActor* PuzzleActor)
{
	// 복제된 puzzle actor를 MVVM에 바인딩하고 root visibility도 C++ 경로에서 보장
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		SetMapHackingVisible(false);
		return;
	}

	View->InitializeSources();
	UHackingPuzzleViewModel* ViewModel = Cast<UHackingPuzzleViewModel>(
		View->GetViewModel(TEXT("HackingPuzzleViewModel")).GetObject()
	);
	if (!ViewModel)
	{
		SetMapHackingVisible(false);
		return;
	}

	UMapHackingPuzzleComponent* PuzzleComponent = PuzzleActor ? PuzzleActor->FindComponentByClass<UMapHackingPuzzleComponent>() : nullptr;
	if (!PuzzleComponent)
	{
		ViewModel->UnbindFromComponent();
		SetMapHackingVisible(false);
		return;
	}

	ViewModel->BindToComponent(PuzzleComponent);
	SetMapHackingVisible(true);
}
