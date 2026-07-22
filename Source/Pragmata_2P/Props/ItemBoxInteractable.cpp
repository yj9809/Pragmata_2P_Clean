#include "Props/ItemBoxInteractable.h"

#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Components/PragmataPlayerStatsComponent.h"
#include "Characters/HughCharacter.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"

AItemBoxInteractable::AItemBoxInteractable()
{
	bCanBeScanned = true;
	ScanTargetKind = EPragmataScanTargetKind::ItemBox;
	ScanTargetId = TEXT("ItemBox");
	ScanDisplayText = NSLOCTEXT("Pragmata", "ItemBoxScanDisplayText", "Item Box");
	DianaInteractionDistance = 750.f;
}

void AItemBoxInteractable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AItemBoxInteractable, bIsOpened);
}

bool AItemBoxInteractable::ShouldShowInteractionPromptOnDianaFocus() const
{
	// ItemBox 계열은 Diana가 직접 조준할 때만 click guide를 표시
	return true;
}

void AItemBoxInteractable::Interact_Implementation(AActor* Interactor)
{
	// 아이템 지급과 상자 열림 상태는 서버에서 Diana 입력만 받아 한 번만 확정
	if (!HasAuthority() || bIsOpened || !IsDianaInteractor(Interactor))
	{
		return;
	}

	bIsOpened = true;
	if (APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr)
	{
		if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
		{
			DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::ItemBoxOpened);
		}
	}

	APragmataGameState* GS = GetWorld()->GetGameState<APragmataGameState>();
	
	if (AHughCharacter* HughCharacter = GS->GetHughCharacter())
	{
		if (UPragmataPlayerStatsComponent* Stats = HughCharacter->GetPlayerStatsComponent())
		{
			Stats->AddLunafilament(Amount);
		}
	}
	
	Super::Interact_Implementation(Interactor);
}

FPragmataScanResult AItemBoxInteractable::GetPragmataScanResult_Implementation(AActor* ScanRequester) const
{
	// 열린 상자는 더 이상 POI marker로 반복 안내하지 않음
	if (bIsOpened)
	{
		return FPragmataScanResult();
	}

	return Super::GetPragmataScanResult_Implementation(ScanRequester);
}

bool AItemBoxInteractable::ShouldCreateInteractionPromptWidget() const
{
	// ItemBox prompt는 Diana local viewport에만 노출
	const APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);
	const APragmataPlayerState* PragmataPlayerState = LocalPlayerController ? LocalPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	return PragmataPlayerState && PragmataPlayerState->GetAssignedRole() == EPragmataPlayerRole::Diana;
}

bool AItemBoxInteractable::IsDianaInteractor(const AActor* Interactor) const
{
	// 서버 상호작용 호출자가 Diana role인지 확인
	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	const APragmataPlayerController* PragmataPlayerController = InteractorPawn ? Cast<APragmataPlayerController>(InteractorPawn->GetController()) : Cast<APragmataPlayerController>(Interactor);
	const APragmataPlayerState* PragmataPlayerState = PragmataPlayerController ? PragmataPlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	return PragmataPlayerState && PragmataPlayerState->GetAssignedRole() == EPragmataPlayerRole::Diana;
}
