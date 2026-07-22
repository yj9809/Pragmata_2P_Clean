#include "Props/CheckPoint.h"

#include "Characters/HughCharacter.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataGameMode.h"
#include "Components/WidgetComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "UI/HoldKeyPromptWidget.h"

ACheckPoint::ACheckPoint()
{
	bCanBeScanned = true;
	ScanTargetKind = EPragmataScanTargetKind::Checkpoint;
	ScanTargetId = TEXT("Checkpoint");
	ScanDisplayText = NSLOCTEXT("Pragmata", "CheckpointScanDisplayText", "Checkpoint");
	DianaInteractionDistance = 5000.f;
	HoldPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HoldPromptComponent"));
	HoldPromptComponent->SetupAttachment(RootComponent);
	HoldPromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HoldPromptComponent->SetDrawSize(FVector2D(160.f, 160.f));
	HoldPromptComponent->SetRelativeLocation(FVector(0.f, 0.f, 140.f));
	HoldPromptComponent->SetVisibility(false);
	HoldPromptWidgetClass = TSoftClassPtr<UHoldKeyPromptWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/Player/WBP_HoldKeyPrompt.WBP_HoldKeyPrompt_C")));
	RespawnAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnAnchor"));
	RespawnAnchor->SetupAttachment(RootComponent);
}

void ACheckPoint::BeginPlay()
{
	// checkpoint는 viewport prompt 대신 prop WidgetComponent만 준비
	Super::BeginPlay();
	if (HoldPromptComponent && HoldPromptWidgetClass.LoadSynchronous())
	{
		HoldPromptComponent->SetWidgetClass(HoldPromptWidgetClass.Get());
	}
	if (HasAuthority() && CheckpointId.IsNone())
	{
		CheckpointId = FName(*GetPathName());
	}
}

void ACheckPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACheckPoint, bActivated);
	DOREPLIFETIME(ACheckPoint, CheckpointId);
}

FTransform ACheckPoint::GetRespawnTransform() const
{
	return RespawnAnchor ? RespawnAnchor->GetComponentTransform() : GetActorTransform();
}

void ACheckPoint::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bWasActivated = bActivated;
	bActivated = true;
	if (APragmataGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<APragmataGameMode>() : nullptr)
	{
		GameMode->SetActiveCheckpoint(this);
		if (bWasActivated)
		{
			GameMode->OpenCheckpointUpgradeSession();
		}
	}
	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		if (AHughCharacter* HughCharacter = GameState->GetHughCharacter())
		{
			HughCharacter->ReplenishCheckpointResources();
		}
	}

	BroadcastInteractionActivated(Interactor);
}

void ACheckPoint::SetLocalInteractionPromptVisible(bool bVisible)
{
	UHoldKeyPromptWidget* PromptWidget = HoldPromptComponent
		? Cast<UHoldKeyPromptWidget>(HoldPromptComponent->GetUserWidgetObject())
		: nullptr;
	if (!PromptWidget)
	{
		return;
	}

	HoldPromptComponent->SetVisibility(bVisible);
	// camera ray click interaction은 focus guide만 사용하고 hold progress를 표시하지 않음
	PromptWidget->SetHolding(false);
	if (!bVisible)
	{
		PromptWidget->ResetHoldProgress();
		return;
	}
	PromptWidget->SetHoldProgress(0.f);
}
