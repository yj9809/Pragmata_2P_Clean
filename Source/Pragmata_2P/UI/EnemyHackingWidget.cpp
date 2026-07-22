// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/EnemyHackingWidget.h"

#include "Components/Widget.h"
#include "Engine/World.h"
#include "MVVMSubsystem.h"
#include "Puzzle/EnemyHackingPuzzleComponent.h"
#include "UI/EnemyHackingPuzzleViewModel.h"
#include "View/MVVMView.h"

void UEnemyHackingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetEnemyHackingVisible(false);
}

void UEnemyHackingWidget::NativeDestruct()
{
	// widget 제거 시 ViewModel delegate를 먼저 해제해 stale 완료 callback을 막음
	StopHackCountdown(true);
	UnbindFromViewModel();

	Super::NativeDestruct();
}

void UEnemyHackingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshHackCountdown();
}

void UEnemyHackingWidget::SetEnemyHackingVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	SetVisibility(NewVisibility);

	if (Root)
	{
		Root->SetVisibility(NewVisibility);
	}
}

void UEnemyHackingWidget::SetHackTarget(AActor* TargetActor)
{
	// 복제된 target을 MVVM에 바인딩하고 root visibility도 C++ 경로에서 보장
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		StopHackCountdown(true);
		UnbindFromViewModel();
		SetEnemyHackingVisible(false);
		return;
	}

	View->InitializeSources();
	UEnemyHackingPuzzleViewModel* ViewModel = Cast<UEnemyHackingPuzzleViewModel>(
		View->GetViewModel(TEXT("EnemyHackingPuzzleViewModel")).GetObject()
	);
	if (!ViewModel)
	{
		StopHackCountdown(true);
		UnbindFromViewModel();
		SetEnemyHackingVisible(false);
		return;
	}

	const bool bHasPuzzleTarget = TargetActor && TargetActor->FindComponentByClass<UEnemyHackingPuzzleComponent>();
	if (bHasPuzzleTarget)
	{
		StopHackCountdown(true);
		BindToViewModel(ViewModel);
	}
	else
	{
		StopHackCountdown(true);
		UnbindFromViewModel();
	}

	ViewModel->SetHackTarget(TargetActor);
	SetEnemyHackingVisible(bHasPuzzleTarget);
}

void UEnemyHackingWidget::SetHackCountdownDurationSeconds(float NewDurationSeconds)
{
	// (deprecated) 지속시간은 몬스터 복제 상태가 소유하므로 게이지에 영향 없음. BP 호환용 no-op 유지.
	HackCountdownDurationSeconds = FMath::Max(0.1f, NewDurationSeconds);
}

void UEnemyHackingWidget::HandleHackCompleted(float /*FinalDamage*/)
{
	// 완료 복제 직후 target null 정리보다 먼저 BP 성공 애니메이션을 시작.
	// 게이지 잔량은 NativeTick의 RefreshHackCountdown이 몬스터 복제 상태에서 매 틱 갱신.
	SetEnemyHackingVisible(true);
	OnHackCompleted();
}

void UEnemyHackingWidget::BindToViewModel(UEnemyHackingPuzzleViewModel* ViewModel)
{
	if (BoundViewModel == ViewModel)
	{
		return;
	}

	UnbindFromViewModel();
	if (!ViewModel)
	{
		return;
	}

	BoundViewModel = ViewModel;
	BoundViewModel->OnHackCompletedWithDamage.AddUniqueDynamic(this, &UEnemyHackingWidget::HandleHackCompleted);
}

void UEnemyHackingWidget::UnbindFromViewModel()
{
	if (BoundViewModel)
	{
		BoundViewModel->OnHackCompletedWithDamage.RemoveDynamic(this, &UEnemyHackingWidget::HandleHackCompleted);
		BoundViewModel = nullptr;
	}
}

void UEnemyHackingWidget::StopHackCountdown(bool bResetViewModel)
{
	// 게이지 잔량만 0으로 리셋 — 타이밍 자체는 몬스터 복제 상태가 소유하므로 로컬 상태 없음.
	if (bResetViewModel && BoundViewModel)
	{
		BoundViewModel->SetHackTimeRemaining(0.f, 1.f, 1.f);
	}
}

void UEnemyHackingWidget::RefreshHackCountdown()
{
	if (!BoundViewModel)
	{
		return;
	}

	// 몬스터의 복제된 지속시간이 유효하면 카운트다운으로 구동. 만료/퍼즐 중이면 카운트다운 값을 0으로
	// 밀어 ViewModel의 DoorGaugeRatio 갱신을 트리거 — 만료 시 게이지가 비워지고, 퍼즐 중엔
	// DoorGaugeRatio가 노드 통과 채움(DurationFillRatio)을 그대로 표시한다.
	const float Duration = BoundViewModel->GetHackTargetDuration();
	if (Duration > UE_KINDA_SMALL_NUMBER)
	{
		const float Remaining = BoundViewModel->GetHackTargetRemainingSeconds();
		const float MaxDuration = FMath::Max(BoundViewModel->GetMaxHackDurationSeconds(), UE_KINDA_SMALL_NUMBER);
		const float FullGaugeRatio = FMath::Clamp(Duration / MaxDuration, 0.f, 1.f);
		PushHackCountdown(Remaining, Duration, FullGaugeRatio);
	}
	else
	{
		PushHackCountdown(0.f, 1.f, 0.f);
	}
}

void UEnemyHackingWidget::PushHackCountdown(float RemainingSeconds, float DurationSeconds, float FullGaugeRatio)
{
	const float SafeDuration = FMath::Max(DurationSeconds, UE_KINDA_SMALL_NUMBER);
	const float ClampedRemaining = FMath::Clamp(RemainingSeconds, 0.f, SafeDuration);

	if (BoundViewModel)
	{
		BoundViewModel->SetHackTimeRemaining(ClampedRemaining, SafeDuration, FullGaugeRatio);
	}
}
