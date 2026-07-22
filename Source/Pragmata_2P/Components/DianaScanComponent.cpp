#include "Components/DianaScanComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/DianaAnimInstance.h"
#include "Characters/HughCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Engine/World.h"
#include "Interfaces/PragmataScannable.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPragmataScan, Log, All);

UDianaScanComponent::UDianaScanComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	ScanAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(
		TEXT("/Game/Character/Diana/Animations/General2/AS_Diana_General2_2000_Scan_VerA.AS_Diana_General2_2000_Scan_VerA")));
}

void UDianaScanComponent::RequestScan()
{
	// Diana 입력은 intent만 보내고 서버가 POI 후보와 cooldown을 판정
	UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::RequestScan] Local scan request reached component"));
	Server_RequestScan();
}

void UDianaScanComponent::Server_RequestScan_Implementation()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	FString RejectReason;
	if (!CanStartAreaScan(CurrentTime, &RejectReason))
	{
		UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::Server_RequestScan] Rejected: %s"), *RejectReason);
		return;
	}

	FVector ScanOrigin = FVector::ZeroVector;
	if (!ResolveAreaScanOrigin(ScanOrigin))
	{
		return;
	}

	TArray<FPragmataScanResult> ScanResults;
	CollectAreaScanResults(ScanOrigin, ScanResults);
	SendAreaScanResultsToPlayers(ScanResults, ScanOrigin);
	Multicast_PlayScanAnimation();
	if (ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>())
	{
		DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::Scan);
	}

	NextAllowedScanTime = CurrentTime + ScanDuration + ScanCooldownAfterDuration;
	UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::Server_RequestScan] Accepted: Results=%d NextAllowed=%.2f"), ScanResults.Num(), NextAllowedScanTime);
}

void UDianaScanComponent::Multicast_PlayScanAnimation_Implementation()
{
	PlayScanAnimation();
}

void UDianaScanComponent::PlayScanAnimation()
{
	// scan animation은 mount mesh slot을 사용해 양 client에서 같은 Diana body pose를 보임
	const APragmataGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APragmataGameState>() : nullptr;
	const AHughCharacter* HughCharacter = GameState ? GameState->GetHughCharacter() : nullptr;
	USkeletalMeshComponent* DianaMesh = HughCharacter ? HughCharacter->GetDianaAvatarMesh() : nullptr;
	if (UDianaAnimInstance* DianaAnim = DianaMesh ? Cast<UDianaAnimInstance>(DianaMesh->GetAnimInstance()) : nullptr)
	{
		DianaAnim->PlaySkillBodyAnimation(ScanAnimation.LoadSynchronous());
	}
}

bool UDianaScanComponent::ResolveAreaScanOrigin(FVector& OutScanOrigin) const
{
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!DianaPawn)
	{
		return false;
	}

	OutScanOrigin = DianaPawn->GetActorLocation();
	return true;
}

bool UDianaScanComponent::CanStartAreaScan(float CurrentTime, FString* OutRejectReason) const
{
	// 서버가 Diana role과 scan cooldown을 함께 검증
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!GameState)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Missing GameState");
		}
		return false;
	}
	if (GameState->AreDianaActionsLocked())
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Diana actions locked");
		}
		return false;
	}
	if (GameState->IsEnemyCombatActive())
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Enemy combat active");
		}
		return false;
	}
	if (CurrentTime < NextAllowedScanTime)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = FString::Printf(TEXT("Cooldown active %.2fs remaining"), NextAllowedScanTime - CurrentTime);
		}
		return false;
	}

	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	const APragmataPlayerController* PlayerController = DianaPawn ? Cast<APragmataPlayerController>(DianaPawn->GetController()) : nullptr;
	const APragmataPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!DianaPawn)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Owner is not Diana pawn");
		}
		return false;
	}
	if (!PlayerController)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Diana pawn has no PragmataPlayerController");
		}
		return false;
	}
	if (!PlayerState)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = TEXT("Missing PragmataPlayerState");
		}
		return false;
	}
	if (PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana)
	{
		if (OutRejectReason)
		{
			*OutRejectReason = FString::Printf(TEXT("AssignedRole is not Diana: %d"), static_cast<int32>(PlayerState->GetAssignedRole()));
		}
		return false;
	}

	const EDianaOperatorMode OperatorMode = DianaPawn->GetOperatorMode();
	const bool bAllowedMode = OperatorMode == EDianaOperatorMode::Roaming;
	if (!bAllowedMode && OutRejectReason)
	{
		*OutRejectReason = FString::Printf(TEXT("OperatorMode does not allow scan: %d"), static_cast<int32>(OperatorMode));
	}
	return bAllowedMode;
}

void UDianaScanComponent::CollectAreaScanResults(const FVector& ScanOrigin, TArray<FPragmataScanResult>& OutScanResults) const
{
	// registry 후보만 훑어 scan 시점의 월드 전체 순회를 피함
	OutScanResults.Reset();

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	TArray<AActor*> CandidateActors;
	GameState->GetRegisteredScannablePOIs(CandidateActors);
	int32 InRadiusCount = 0;
	int32 ValidPOICount = 0;
	for (AActor* Candidate : CandidateActors)
	{
		if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UPragmataScannable::StaticClass()))
		{
			continue;
		}

		const float Distance = FVector::Dist(ScanOrigin, Candidate->GetActorLocation());
		if (Distance > ScanRadius)
		{
			continue;
		}
		++InRadiusCount;

		FPragmataScanResult ScanResult = IPragmataScannable::Execute_GetPragmataScanResult(Candidate, GetOwner());
		if (!ScanResult.bIsValid)
		{
			continue;
		}

		if (ScanResult.TargetKind != EPragmataScanTargetKind::MapHackNode
			&& ScanResult.TargetKind != EPragmataScanTargetKind::ItemBox
			&& ScanResult.TargetKind != EPragmataScanTargetKind::Checkpoint)
		{
			continue;
		}

		if (!ScanResult.TargetActor)
		{
			ScanResult.TargetActor = Candidate;
		}
		ScanResult.TargetWorldLocation = Candidate->GetActorLocation();
		ScanResult.Distance = Distance;
		ScanResult.RevealDelay = ScanRadius > UE_KINDA_SMALL_NUMBER ? (Distance / ScanRadius) * ScanDuration : 0.f;
		OutScanResults.Add(ScanResult);
		++ValidPOICount;
	}

	OutScanResults.Sort([](const FPragmataScanResult& Left, const FPragmataScanResult& Right)
	{
		return Left.Distance < Right.Distance;
	});

	if (OutScanResults.Num() > MaxScanResults)
	{
		OutScanResults.SetNum(MaxScanResults);
	}

	for (int32 ResultIndex = 0; ResultIndex < OutScanResults.Num(); ++ResultIndex)
	{
		const FPragmataScanResult& ScanResult = OutScanResults[ResultIndex];
		UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::CollectAreaScanResults] Result[%d] Actor=%s Kind=%d Distance=%.0fcm RevealDelay=%.2fs"), ResultIndex, *GetNameSafe(ScanResult.TargetActor.Get()), static_cast<int32>(ScanResult.TargetKind), ScanResult.Distance, ScanResult.RevealDelay);
	}
	if (OutScanResults.Num() > 0)
	{
		UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::CollectAreaScanResults] Candidates=%d InRadius=%d ValidPOI=%d Returned=%d"), CandidateActors.Num(), InRadiusCount, ValidPOICount, OutScanResults.Num());
	}
}

void UDianaScanComponent::SendAreaScanResultsToPlayers(const TArray<FPragmataScanResult>& ScanResults, const FVector& ScanOrigin) const
{
	// scan marker는 Hugh와 Diana의 owning client HUD에만 전달
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!GameState)
	{
		UE_LOG(LogPragmataScan, Warning, TEXT("[DianaScanComponent::SendAreaScanResultsToPlayers] Missing GameState"));
		return;
	}

	if (AHughCharacter* HughCharacter = GameState->GetHughCharacter())
	{
		if (APragmataPlayerController* HughController = Cast<APragmataPlayerController>(HughCharacter->GetController()))
		{
			if (ScanResults.Num() > 0)
			{
				UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::SendAreaScanResultsToPlayers] Sending %d scan results to Hugh"), ScanResults.Num());
			}
			HughController->Client_ShowPOIScanResults(ScanResults, ScanDuration);
			HughController->Client_PlayDianaScanPulse(ScanOrigin, ScanRadius, 1.5f);
		}
	}

	if (ADianaOperatorPawn* DianaPawn = GameState->GetDianaOperatorPawn())
	{
		if (APragmataPlayerController* DianaController = Cast<APragmataPlayerController>(DianaPawn->GetController()))
		{
			if (ScanResults.Num() > 0)
			{
				UE_LOG(LogPragmataScan, Log, TEXT("[DianaScanComponent::SendAreaScanResultsToPlayers] Sending %d scan results to Diana"), ScanResults.Num());
			}
			DianaController->Client_ShowPOIScanResults(ScanResults, ScanDuration);
			DianaController->Client_PlayDianaScanPulse(ScanOrigin, ScanRadius, 1.5f);
		}
	}
}
