#include "Props/PropInteractionBase.h"

#include "Engine/World.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Game/PragmataGameState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Props/PropInteractionComponent.h"
#include "UI/ScanIconWidget.h"
#include "UI/HoldKeyPromptWidget.h"

APropInteractionBase::APropInteractionBase()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionComponent = CreateDefaultSubobject<UPropInteractionComponent>(TEXT("InteractionComponent"));
	ScanIconComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ScanIconComponent"));
	ScanIconComponent->SetupAttachment(RootComponent);
	ScanIconComponent->SetWidgetSpace(EWidgetSpace::Screen);
	ScanIconComponent->SetDrawSize(FVector2D(128.f, 96.f));
	ScanIconComponent->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	ScanIconWidgetClass = TSoftClassPtr<UScanIconWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/Scan/WBP_ScanIconWidget.WBP_ScanIconWidget_C")));
}

void APropInteractionBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureHoldKeyPromptWidget();
	if (ScanIconComponent && ScanIconWidgetClass.LoadSynchronous())
	{
		// 이전 Blueprint/native default의 hidden 상태가 남아 있어도 widget 내부 visibility만 사용
		ScanIconComponent->SetVisibility(true);
		ScanIconComponent->SetWidgetClass(ScanIconWidgetClass.Get());
		ScanIconComponent->InitWidget();
		// component는 render 상태를 유지하고 실제 icon 초기 상태는 widget API로 숨김
		if (UScanIconWidget* ScanIconWidget = Cast<UScanIconWidget>(ScanIconComponent->GetUserWidgetObject()))
		{
			ScanIconWidget->SetScanIconVisible(false);
		}
	}

	if (bCanBeScanned)
	{
		if (APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
		{
			PragmataGameState->RegisterScannablePOI(this);
		}
	}
}

// Diana local focus 안내는 prop의 scan 표시와 분리된 world-space prompt로 유지
void APropInteractionBase::ShowInteractionPromptWidget()
{
	// Diana focus에서만 BP prop의 world-space key guide를 표시
	if (UHoldKeyPromptWidget* PromptWidget = EnsureHoldKeyPromptWidget())
	{
		if (InteractionPromptWidgetComponent)
		{
			InteractionPromptWidgetComponent->SetVisibility(true);
		}

		PromptWidget->SetVisibility(ESlateVisibility::Visible);
	}
}

void APropInteractionBase::HideInteractionPromptWidget()
{
	// focus 해제 시 prop guide를 즉시 숨겨 기본 Q 안내가 남지 않게 유지
	if (InteractionPromptWidgetComponent)
	{
		InteractionPromptWidgetComponent->SetVisibility(false);
	}

	if (HoldKeyPromptWidget)
	{
		HoldKeyPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UHoldKeyPromptWidget* APropInteractionBase::EnsureHoldKeyPromptWidget()
{
	if (!ShouldCreateInteractionPromptWidget() || !ShouldShowInteractionPromptOnDianaFocus())
	{
		return nullptr;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	if (!InteractionPromptWidgetComponent)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
		for (UWidgetComponent* WidgetComponent : WidgetComponents)
		{
			if (WidgetComponent != ScanIconComponent
				&& WidgetComponent->GetWidgetClass()
				&& WidgetComponent->GetWidgetClass()->IsChildOf(UHoldKeyPromptWidget::StaticClass()))
			{
				InteractionPromptWidgetComponent = WidgetComponent;
				break;
			}
		}
	}

	if (!InteractionPromptWidgetComponent)
	{
		return nullptr;
	}

	if (!HoldKeyPromptWidget)
	{
		// BP prop에 배치된 world-space prompt만 사용해 viewport 중앙에 별도 안내가 생기지 않게 유지
		InteractionPromptWidgetComponent->InitWidget();
		HoldKeyPromptWidget = Cast<UHoldKeyPromptWidget>(InteractionPromptWidgetComponent->GetUserWidgetObject());
	}

	if (HoldKeyPromptWidget && InteractionPromptKeyTexture)
	{
		HoldKeyPromptWidget->SetKeyTexture(InteractionPromptKeyTexture);
	}

	if (HoldKeyPromptWidget)
	{
		InteractionPromptWidgetComponent->SetVisibility(false);
		HoldKeyPromptWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	return HoldKeyPromptWidget;
}

bool APropInteractionBase::ShouldShowInteractionPromptOnDianaFocus() const
{
	// 기본 prop은 focus key guide를 직접 소유하지 않음
	return false;
}

bool APropInteractionBase::ShouldCreateInteractionPromptWidget() const
{
	return true;
}

void APropInteractionBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APragmataGameState* PragmataGameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		PragmataGameState->UnregisterScannablePOI(this);
	}

	Super::EndPlay(EndPlayReason);
}

void APropInteractionBase::Interact_Implementation(AActor* Interactor)
{
	// 기본 상호작용 액터는 연결된 동작 액터에 발동 신호만 전달
	BroadcastInteractionActivated(Interactor);
}

FPragmataScanResult APropInteractionBase::GetPragmataScanResult_Implementation(AActor* ScanRequester) const
{
	// interaction 실행 여부와 POI 표시 metadata를 분리해 scan 진입점을 하나로 유지
	FPragmataScanResult Result;
	if (!bCanBeScanned)
	{
		return Result;
	}

	Result.bIsValid = true;
	Result.TargetActor = const_cast<APropInteractionBase*>(this);
	Result.TargetWorldLocation = GetActorLocation();
	Result.TargetId = ScanTargetId.IsNone() ? FName(*GetName()) : ScanTargetId;
	Result.TargetKind = ScanTargetKind;
	Result.DisplayText = ScanDisplayText.IsEmpty() ? FText::FromName(Result.TargetId) : ScanDisplayText;
	Result.Distance = ScanRequester ? FVector::Dist(ScanRequester->GetActorLocation(), GetActorLocation()) : 0.f;
	Result.DianaInteractionDistance = DianaInteractionDistance;
	return Result;
}

void APropInteractionBase::BroadcastInteractionActivated(AActor* Interactor)
{
	OnInteractionActivated.Broadcast(Interactor);
}
