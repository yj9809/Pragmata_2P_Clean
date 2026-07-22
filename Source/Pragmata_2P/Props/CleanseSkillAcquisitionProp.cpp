#include "Props/CleanseSkillAcquisitionProp.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Data/TutorialStepData.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/TutorialBlueprintLibrary.h"
#include "Net/UnrealNetwork.h"
#include "UI/ScanIconWidget.h"

ACleanseSkillAcquisitionProp::ACleanseSkillAcquisitionProp()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	bCanBeScanned = false;
	ScanTargetKind = EPragmataScanTargetKind::Unknown;
	ScanTargetId = NAME_None;
	CleanseTutorialStep = TSoftObjectPtr<UTutorialStepData>(
		FSoftObjectPath(TEXT("/Game/Data/Tutorial/DA_CleanseAcquisitionTutorial.DA_CleanseAcquisitionTutorial")));
}

void ACleanseSkillAcquisitionProp::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint에 NoCollision이 저장되어 있어도 Diana Visibility trace가 기계 표면을 찾도록 보정
	if (AnimatedMesh)
	{
		AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		AnimatedMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		AnimatedMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

void ACleanseSkillAcquisitionProp::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority() || bAcquired)
	{
		return;
	}

	const APawn* DianaPawn = Cast<APawn>(Interactor);
	APragmataPlayerController* Controller = DianaPawn ? Cast<APragmataPlayerController>(DianaPawn->GetController()) : nullptr;
	APragmataPlayerState* PlayerState = Controller ? Controller->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PlayerState || PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CleanseAcquisition] Interaction rejected: Diana PlayerState is missing or role is invalid"));
		return;
	}

	UTutorialStepData* TutorialStep = CleanseTutorialStep.LoadSynchronous();
	if (!TutorialStep)
	{
		UE_LOG(LogTemp, Error, TEXT("[CleanseAcquisition] Tutorial data is missing"));
		return;
	}

	// 정화 해금과 튜토리얼 요청을 확정한 뒤 deadfilamentbox 자체 상호작용 애니메이션을 복제한다.
	bAcquired = true;
	PlayerState->UnlockCleanse();
	// server GameState multicast를 사용해 None target tutorial을 Hugh/Diana 양쪽 local viewport에 전달
	UTutorialBlueprintLibrary::ShowTutorialOnce(this, TEXT("CleanseAcquisition"), TutorialStep);
	AAnimatedPropInteraction::Interact_Implementation(Interactor);
	ForceNetUpdate();

	UE_LOG(LogTemp, Display, TEXT("[CleanseAcquisition] Cleanse unlocked without cinematic"));
}

void ACleanseSkillAcquisitionProp::SetVicinityIndicatorVisible(bool bVisible, float DistanceCentimeters)
{
	if (UWidgetComponent* Widget = FindComponentByClass<UWidgetComponent>())
	{
		if (UScanIconWidget* Icon = Cast<UScanIconWidget>(Widget->GetUserWidgetObject()))
		{
			Icon->SetScanIconVisible(bVisible && !bAcquired);
			if (bVisible)
			{
				Icon->SetDistanceText(FText::AsNumber(FMath::RoundToInt(DistanceCentimeters / 100.f)));
			}
		}
	}
}

bool ACleanseSkillAcquisitionProp::ShouldShowInteractionPromptOnDianaFocus() const
{
	return !bAcquired;
}

void ACleanseSkillAcquisitionProp::OnRep_Acquired()
{
	if (bAcquired)
	{
		HideInteractionPromptWidget();
	}
}

void ACleanseSkillAcquisitionProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACleanseSkillAcquisitionProp, bAcquired);
}
