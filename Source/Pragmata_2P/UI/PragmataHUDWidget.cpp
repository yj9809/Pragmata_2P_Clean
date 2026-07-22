// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/PragmataHUDWidget.h"

#include "MVVMSubsystem.h"
#include "UI/BossHPWidget.h"
#include "UI/EnemyHackingWidget.h"
#include "UI/PragmataHUDViewModel.h"
#include "UI/MapHackingWidget.h"
#include "UI/ObjectiveAlertWidget.h"
#include "UI/DialogueWidget.h"
#include "UI/WeaponWheelWidget.h"
#include "Components/Widget.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "View/MVVMView.h"

UPragmataHUDWidget::UPragmataHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPragmataHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnemyHackingWidget = Cast<UEnemyHackingWidget>(GetWidgetFromName(TEXT("WBP_EnemyHackingWidget")));
	MapHackingWidget = Cast<UMapHackingWidget>(GetWidgetFromName(TEXT("WBP_MapHackingWidget")));
	ResolveObjectiveAlertWidget();
	ResolveDialogueWidget();
	ResolveBossHPWidget();
	ResolveWeaponWheelRoot();
}

void UPragmataHUDWidget::NativeDestruct()
{
	if (UWeaponWheelWidget* ResolvedWeaponWheelRoot = ResolveWeaponWheelRoot())
	{
		ResolvedWeaponWheelRoot->UnbindWeaponSource();
	}

	if (BoundHUDViewModel)
	{
		BoundHUDViewModel->UnbindSharedHUDStats();
		BoundHUDViewModel = nullptr;
	}

	if (BossHPWidget)
	{
		BossHPWidget->UnbindFromViewModel();
	}

	Super::NativeDestruct();
}

void UPragmataHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Idle hide는 HUD Motion 여부와 무관하게 동작해야 하므로 먼저 처리
	UpdateIdleHide(InDeltaTime);

	if (BoundHUDViewModel)
	{
		// cooldown progress는 복제된 시작 시간 기준으로 local HUD tick에서 보간
		BoundHUDViewModel->RefreshCleanseCooldownState();
	}

	if (!HUDMotionRoot)
	{
		return;
	}

	if (!bEnableHUDMotion)
	{
		CurrentMotionOffset = FVector2D::ZeroVector;
		LookSwayOffset = FVector2D::ZeroVector;
		MovementSwayOffset = FVector2D::ZeroVector;
		ShakeOffset = FVector2D::ZeroVector;
		bHasPreviousControlRotation = false;
		HUDMotionRoot->SetRenderTranslation(CurrentMotionOffset);
		return;
	}

	UpdateShake(InDeltaTime);

	const FVector2D TargetOffset =
		CalculateLookSway(InDeltaTime)
		+ CalculateMovementSway(InDeltaTime)
		+ ShakeOffset;

	CurrentMotionOffset = FMath::Vector2DInterpTo(
		CurrentMotionOffset,
		TargetOffset,
		InDeltaTime,
		LookSwayInterpSpeed
	);

	HUDMotionRoot->SetRenderTranslation(CurrentMotionOffset);
}

void UPragmataHUDWidget::AddHUDShake(FVector2D ShakeAmount)
{
	ShakeOffset += ShakeAmount;
}

void UPragmataHUDWidget::SetWeaponWheelVisible(bool bNewVisible)
{
	UWeaponWheelWidget* ResolvedWeaponWheelRoot = ResolveWeaponWheelRoot();
	if (!ResolvedWeaponWheelRoot)
	{
		return;
	}

	ResolvedWeaponWheelRoot->SetWeaponWheelVisible(bNewVisible);
}

void UPragmataHUDWidget::HandleActiveEnemyHackTargetChanged(AActor* TargetActor)
{
	// C++ 기본 경로로 enemy hack widget을 연결하고 BP에는 추가 연출 hook을 남김
	if (!EnemyHackingWidget)
	{
		EnemyHackingWidget = Cast<UEnemyHackingWidget>(GetWidgetFromName(TEXT("WBP_EnemyHackingWidget")));
	}

	if (EnemyHackingWidget)
	{
		EnemyHackingWidget->SetHackTarget(TargetActor);
	}

	OnActiveEnemyHackTargetChanged(TargetActor);
}

void UPragmataHUDWidget::BindSharedHUDStats(AHughCharacter* HughCharacter, ADianaOperatorPawn* DianaPawn)
{
	// root HUD별 ViewModel은 다르지만 source는 GameState role pointer로 통일
	if (UWeaponWheelWidget* ResolvedWeaponWheelRoot = ResolveWeaponWheelRoot())
	{
		ResolvedWeaponWheelRoot->BindWeaponSource(HughCharacter);
	}

	UPragmataHUDViewModel* HUDViewModel = ResolveHUDViewModel();
	if (BoundHUDViewModel && BoundHUDViewModel != HUDViewModel)
	{
		BoundHUDViewModel->UnbindSharedHUDStats();
	}

	BoundHUDViewModel = HUDViewModel;
	if (!BoundHUDViewModel)
	{
		return;
	}

	BoundHUDViewModel->BindSharedHUDStats(HughCharacter, DianaPawn);
}

void UPragmataHUDWidget::ShowObjectiveAlert(const FText& ObjectiveText, float DisplaySeconds)
{
	// 목표 생성 판정은 게임 로직이 소유하고, HUD는 전달받은 문구만 현재 로컬 화면에 표시
	if (UObjectiveAlertWidget* ResolvedObjectiveAlertWidget = ResolveObjectiveAlertWidget())
	{
		ResolvedObjectiveAlertWidget->ShowObjective(ObjectiveText, DisplaySeconds);
	}
}

void UPragmataHUDWidget::PlayDialogueLine(const FDialogueLine& Line, float DisplaySeconds)
{
	// 대사 진행 판정은 게임 로직이 소유하고, HUD는 전달받은 한 줄만 현재 로컬 화면에 표시
	if (UDialogueWidget* ResolvedDialogueWidget = ResolveDialogueWidget())
	{
		ResolvedDialogueWidget->PlayLine(Line, DisplaySeconds);
	}
}

void UPragmataHUDWidget::HideDialogue()
{
	if (UDialogueWidget* ResolvedDialogueWidget = ResolveDialogueWidget())
	{
		ResolvedDialogueWidget->HideDialogue();
	}
}

void UPragmataHUDWidget::HandleActiveMapPuzzleActorChanged(AActor* PuzzleActor)
{
	// C++ 기본 경로로 map hack widget을 연결하고 BP에는 추가 연출 hook을 남김
	if (!MapHackingWidget)
	{
		MapHackingWidget = Cast<UMapHackingWidget>(GetWidgetFromName(TEXT("WBP_MapHackingWidget")));
	}

	if (MapHackingWidget)
	{
		MapHackingWidget->SetMapPuzzleActor(PuzzleActor);
	}

	OnActiveMapPuzzleActorChanged(PuzzleActor);
}

// replicated boss target을 root HUD의 boss HP child widget에 전달
void UPragmataHUDWidget::HandleActiveBossTargetChanged(AActor* BossTarget)
{
	// boss HP는 root HUD 안의 고정 child widget으로 표시하고 actor state만 GameState에서 받음
	if (UBossHPWidget* ResolvedBossHPWidget = ResolveBossHPWidget())
	{
		ResolvedBossHPWidget->SetBossTarget(BossTarget);
	}

	OnActiveBossTargetChanged(BossTarget);
}

// HUD 자산의 기존 child 이름을 C++ BindWidget 계약에 맞춰 보정
UWeaponWheelWidget* UPragmataHUDWidget::ResolveWeaponWheelRoot()
{
	if (WeaponWheelRoot)
	{
		return WeaponWheelRoot;
	}

	UWidget* WeaponWheelWidget = GetWidgetFromName(TEXT("WBP_WeaponWheel"));
	WeaponWheelRoot = Cast<UWeaponWheelWidget>(WeaponWheelWidget);
	if (WeaponWheelRoot)
	{
		return WeaponWheelRoot;
	}

	if (!bLoggedMissingWeaponWheelRoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataHUDWidget::ResolveWeaponWheelRoot] Missing weapon wheel widget named WeaponWheelRoot or WBP_WeaponWheel"));
		bLoggedMissingWeaponWheelRoot = true;
	}

	return nullptr;
}

// boss HP WBP를 root HUD 내부 child로 찾아 viewport 중복 생성을 피함
UBossHPWidget* UPragmataHUDWidget::ResolveBossHPWidget()
{
	if (BossHPWidget)
	{
		return BossHPWidget;
	}

	UWidget* BossWidget = GetWidgetFromName(TEXT("WBP_BossHPWidget"));
	BossHPWidget = Cast<UBossHPWidget>(BossWidget);
	return BossHPWidget;
}

UObjectiveAlertWidget* UPragmataHUDWidget::ResolveObjectiveAlertWidget()
{
	if (ObjectiveAlertWidget)
	{
		return ObjectiveAlertWidget;
	}

	UWidget* ObjectiveWidget = GetWidgetFromName(TEXT("WBP_ObjectiveAlertWidget"));
	ObjectiveAlertWidget = Cast<UObjectiveAlertWidget>(ObjectiveWidget);
	if (ObjectiveAlertWidget)
	{
		return ObjectiveAlertWidget;
	}

	if (!bLoggedMissingObjectiveAlertWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataHUDWidget::ResolveObjectiveAlertWidget] Missing objective alert widget named WBP_ObjectiveAlertWidget"));
		bLoggedMissingObjectiveAlertWidget = true;
	}

	return nullptr;
}

UDialogueWidget* UPragmataHUDWidget::ResolveDialogueWidget()
{
	if (DialogueWidget)
	{
		return DialogueWidget;
	}

	UWidget* FoundWidget = GetWidgetFromName(TEXT("WBP_DialogueWidget"));
	DialogueWidget = Cast<UDialogueWidget>(FoundWidget);
	if (DialogueWidget)
	{
		return DialogueWidget;
	}

	if (!bLoggedMissingDialogueWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PragmataHUDWidget::ResolveDialogueWidget] Missing dialogue widget named WBP_DialogueWidget"));
		bLoggedMissingDialogueWidget = true;
	}

	return nullptr;
}

FVector2D UPragmataHUDWidget::CalculateLookSway(float DeltaTime)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		bHasPreviousControlRotation = false;
		return FVector2D::ZeroVector;
	}

	const FRotator CurrentRotation = PlayerController->GetControlRotation();
	if (!bHasPreviousControlRotation)
	{
		PreviousControlRotation = CurrentRotation;
		bHasPreviousControlRotation = true;
		return FVector2D::ZeroVector;
	}

	const FRotator DeltaRotation = (CurrentRotation - PreviousControlRotation).GetNormalized();
	PreviousControlRotation = CurrentRotation;

	const FVector2D TargetLookOffset(
		FMath::Clamp(-DeltaRotation.Yaw * LookSwayAmount, -MaxLookSwayOffset, MaxLookSwayOffset),
		FMath::Clamp(DeltaRotation.Pitch * LookSwayAmount, -MaxLookSwayOffset, MaxLookSwayOffset)
	);

	LookSwayOffset = FMath::Vector2DInterpTo(
		LookSwayOffset,
		TargetLookOffset,
		DeltaTime,
		LookSwayInterpSpeed
	);

	return LookSwayOffset;
}

FVector2D UPragmataHUDWidget::CalculateMovementSway(float DeltaTime)
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		MovementSwayOffset = FMath::Vector2DInterpTo(
			MovementSwayOffset,
			FVector2D::ZeroVector,
			DeltaTime,
			MovementSwayInterpSpeed
		);
		return MovementSwayOffset;
	}

	const FVector LocalVelocity = Pawn->GetActorTransform().InverseTransformVectorNoScale(Pawn->GetVelocity());
	const FVector2D TargetMovementOffset(
		FMath::Clamp(-LocalVelocity.Y * MovementSwayAmount * 0.01f, -MaxMovementSwayOffset, MaxMovementSwayOffset),
		FMath::Clamp(LocalVelocity.X * MovementSwayAmount * 0.005f, -MaxMovementSwayOffset, MaxMovementSwayOffset)
	);

	MovementSwayOffset = FMath::Vector2DInterpTo(
		MovementSwayOffset,
		TargetMovementOffset,
		DeltaTime,
		MovementSwayInterpSpeed
	);

	return MovementSwayOffset;
}

UPragmataHUDViewModel* UPragmataHUDWidget::ResolveHUDViewModel() const
{
	UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(this);
	if (!View)
	{
		return nullptr;
	}

	return Cast<UPragmataHUDViewModel>(View->GetViewModel(TEXT("PragmataHUDViewModel")).GetObject());
}

void UPragmataHUDWidget::UpdateIdleHide(float DeltaTime)
{
	if (!bEnableIdleHide)
	{
		// 기능이 꺼져 있으면 항상 완전히 보이도록 복구
		IdleTimer = 0.f;
		if (CurrentHUDOpacity != 1.f)
		{
			CurrentHUDOpacity = 1.f;
			SetRenderOpacity(1.f);
		}
		bHasPreviousIdleRotation = false;
		return;
	}

	if (DetectPlayerActivity())
	{
		IdleTimer = 0.f;
	}
	else
	{
		IdleTimer += DeltaTime;
	}

	const float TargetOpacity = (IdleTimer >= IdleHideDelay) ? 0.f : 1.f;
	CurrentHUDOpacity = FMath::FInterpTo(CurrentHUDOpacity, TargetOpacity, DeltaTime, IdleFadeSpeed);
	SetRenderOpacity(CurrentHUDOpacity);
}

bool UPragmataHUDWidget::DetectPlayerActivity()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	if (!PlayerController)
	{
		bHasPreviousIdleRotation = false;
		return false;
	}

	const APawn* Pawn = PlayerController->GetPawn();

	// 시야 회전 감지: 휴는 컨트롤 회전, 다이애나는 폰 액터 회전으로 시야를 돌리므로 둘 다 확인
	const FRotator CurrentControlRotation = PlayerController->GetControlRotation();
	const FRotator CurrentPawnRotation = Pawn ? Pawn->GetActorRotation() : FRotator::ZeroRotator;

	bool bActive = false;
	if (bHasPreviousIdleRotation)
	{
		const FRotator ControlDelta = (CurrentControlRotation - PreviousIdleControlRotation).GetNormalized();
		const FRotator PawnDelta = (CurrentPawnRotation - PreviousIdlePawnRotation).GetNormalized();
		if (FMath::Abs(ControlDelta.Yaw) > LookActivityThreshold
			|| FMath::Abs(ControlDelta.Pitch) > LookActivityThreshold
			|| FMath::Abs(PawnDelta.Yaw) > LookActivityThreshold
			|| FMath::Abs(PawnDelta.Pitch) > LookActivityThreshold)
		{
			bActive = true;
		}
	}
	PreviousIdleControlRotation = CurrentControlRotation;
	PreviousIdlePawnRotation = CurrentPawnRotation;
	bHasPreviousIdleRotation = true;

	// 이동 감지 (휴처럼 폰이 실제로 움직이는 경우)
	if (Pawn && Pawn->GetVelocity().SizeSquared() > FMath::Square(MoveActivityThreshold))
	{
		bActive = true;
	}

	return bActive;
}

void UPragmataHUDWidget::UpdateShake(float DeltaTime)
{
	ShakeOffset = FMath::Vector2DInterpTo(
		ShakeOffset,
		FVector2D::ZeroVector,
		DeltaTime,
		ShakeRecoverySpeed
	);
}
