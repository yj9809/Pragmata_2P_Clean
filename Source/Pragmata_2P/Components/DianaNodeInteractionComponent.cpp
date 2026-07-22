#include "Components/DianaNodeInteractionComponent.h"

#include "Engine/Engine.h"
#include "Characters/HughCharacter.h"
#include "Game/PragmataGameState.h"
#include "Game/PragmataPlayerController.h"
#include "Game/PragmataPlayerState.h"
#include "Game/PragmataTraceUtils.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PragmataScannable.h"
#include "Pawns/DianaOperatorPawn.h"
#include "Components/DianaVoiceComponent.h"
#include "Puzzle/MapHackingPuzzleComponent.h"
#include "Props/InteractionPowerSupply.h"
#include "Props/BossRouteElevator.h"
#include "Props/PropInteractionBase.h"
#include "Props/PropInteractableInterface.h"
#include "Props/PuzzleInteractable.h"
#include "Props/CheckPoint.h"
#include "Props/CleanseSkillAcquisitionProp.h"
#include "Components/StaticMeshComponent.h"

UDianaNodeInteractionComponent::UDianaNodeInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDianaNodeInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		GetWorld()->GetTimerManager().SetTimer(FocusRefreshTimerHandle, this, &UDianaNodeInteractionComponent::RefreshFocusedNodeLog, 0.25f, true);
	}
}

void UDianaNodeInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FocusRefreshTimerHandle);
	}

	CurrentFocusedActor = nullptr;
	LastRequestedMapHackActor = nullptr;
	if (ACheckPoint* CheckPoint = LocalFocusedCheckpointPrompt.Get())
	{
		CheckPoint->SetLocalInteractionPromptVisible(false);
	}
	LocalFocusedCheckpointPrompt = nullptr;
	LocallyCompletedActors.Reset();

	Super::EndPlay(EndPlayReason);
}

void UDianaNodeInteractionComponent::RequestNodeInteraction()
{
	// click 입력은 client 의도만 보내고 server가 camera ray target을 다시 판정
	if (!CanOwnerUseNodeInteraction())
	{
		return;
	}

	AActor* RequestedTarget = FindBestNodeInteractionTarget();
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	if (!RequestedTarget || !DianaPawn)
	{
		return;
	}

	Server_RequestNodeInteraction(RequestedTarget, DianaPawn->GetAuthoritativeCameraDirection().GetSafeNormal());
}

void UDianaNodeInteractionComponent::UpdateLocalCheckpointFocusPrompt()
{
	// checkpoint surface를 조준할 때 click guide를 표시
	ACheckPoint* FocusedCheckpoint = Cast<ACheckPoint>(CurrentFocusedActor);
	const bool bShouldShow = FocusedCheckpoint != nullptr;
	ACheckPoint* PreviousCheckpoint = LocalFocusedCheckpointPrompt.Get();
	if (!bShouldShow)
	{
		if (PreviousCheckpoint)
		{
			PreviousCheckpoint->SetLocalInteractionPromptVisible(false);
		}
		LocalFocusedCheckpointPrompt = nullptr;
		return;
	}

	if (PreviousCheckpoint && PreviousCheckpoint != FocusedCheckpoint)
	{
		PreviousCheckpoint->SetLocalInteractionPromptVisible(false);
	}

	FocusedCheckpoint->SetLocalInteractionPromptVisible(true);
	LocalFocusedCheckpointPrompt = FocusedCheckpoint;
}

void UDianaNodeInteractionComponent::RequestMapHackDirection(EHackDirection Direction)
{
	if (IsMapHackActive())
	{
		Server_ProcessMapHackDirection(Direction);
	}
}

void UDianaNodeInteractionComponent::RequestCancelMapHack()
{
	if (IsMapHackActive())
	{
		Server_CancelMapHack();
	}
}

void UDianaNodeInteractionComponent::CancelActiveMapHackFromServer()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClearActiveMapHack(true);
	}
}

bool UDianaNodeInteractionComponent::IsMapHackActive() const
{
	if (ActiveMapPuzzleActor && ActiveMapPuzzleComponent)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	return GameState && GameState->GetActiveMapPuzzleActor();
}

void UDianaNodeInteractionComponent::Server_RequestNodeInteraction_Implementation(AActor* RequestedTarget, FVector_NetQuantizeNormal RequestedCameraDirection)
{
	// click RPC의 최신 camera direction으로 server trace를 재실행해 unreliable view update 지연을 우회
	if (!CanOwnerUseNodeInteraction())
	{
		return;
	}

	AActor* TargetActor = FindBestNodeInteractionTarget(RequestedCameraDirection);
	if (!TargetActor || TargetActor != RequestedTarget
		|| !TargetActor->GetClass()->ImplementsInterface(UPropInteractableInterface::StaticClass()))
	{
		return;
	}

	// checkpoint와 click-required POI는 같은 server-validated click 경로로 즉시 실행
	IPropInteractableInterface::Execute_Interact(TargetActor, GetOwner());
	if (!TargetActor->IsA<APuzzleInteractable>() || IsNodeComplete(TargetActor))
	{
		Client_ReportNodeInteractionComplete(TargetActor, GetNodeDisplayName(TargetActor));
	}
}

void UDianaNodeInteractionComponent::Server_ProcessMapHackDirection_Implementation(EHackDirection Direction)
{
	// combat 전환/aim 해제 후에는 active map puzzle도 즉시 취소
	if (!CanOwnerUseNodeInteraction())
	{
		ClearActiveMapHack(true);
		return;
	}

	// 방향 입력은 서버가 저장한 active map puzzle에만 적용
	if (ActiveMapPuzzleComponent)
	{
		ActiveMapPuzzleComponent->ProcessDirectionInput(Direction);
	}
}

void UDianaNodeInteractionComponent::Server_CancelMapHack_Implementation()
{
	ClearActiveMapHack(true);
}

bool UDianaNodeInteractionComponent::TryStartMapHackFromInteractable(APuzzleInteractable* PuzzleActor)
{
	// APuzzleInteractable::Interact_Implementation만 map hack 시작 진입점으로 사용
	if (!GetOwner() || !GetOwner()->HasAuthority() || !CanOwnerUseNodeInteraction() || !PuzzleActor)
	{
		return false;
	}

	UMapHackingPuzzleComponent* PuzzleComponent = PuzzleActor->FindComponentByClass<UMapHackingPuzzleComponent>();
	if (!PuzzleComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[DianaNodeInteractionComponent::TryStartMapHackFromInteractable] MapHackComponent missing: %s"), *GetNameSafe(PuzzleActor));
		Client_ReportNodeInteractionBlocked(PuzzleActor, TEXT("MapHackComponent missing"));
		return false;
	}

	if (!StartMapHack(PuzzleActor, PuzzleComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("[DianaNodeInteractionComponent::TryStartMapHackFromInteractable] Map hack start failed: %s"), *GetNameSafe(PuzzleActor));
		Client_ReportNodeInteractionBlocked(PuzzleActor, TEXT("MapHackStart failed"));
		return false;
	}

	return true;
}

bool UDianaNodeInteractionComponent::StartMapHack(APuzzleInteractable* PuzzleActor, UMapHackingPuzzleComponent* PuzzleComponent)
{
	// 기존 즉시 완료 node와 달리 puzzle component가 있는 노드는 서버 puzzle부터 시작
	if (!PuzzleActor || !PuzzleComponent || !PuzzleActor->CanInteractWithPuzzle())
	{
		return false;
	}

	ClearActiveMapHack(true);
	if (!PuzzleComponent->StartHack())
	{
		return false;
	}

	if (ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>())
	{
		DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::Hacking);
	}

	PuzzleComponent->OnHackCompleted.RemoveAll(this);
	PuzzleComponent->OnHackReset.RemoveAll(this);
	PuzzleComponent->OnHackCompleted.AddUObject(this, &UDianaNodeInteractionComponent::HandleMapHackCompleted);
	PuzzleComponent->OnHackReset.AddUObject(this, &UDianaNodeInteractionComponent::HandleMapHackReset);

	ActiveMapPuzzleActor = PuzzleActor;
	ActiveMapPuzzleComponent = PuzzleComponent;
	SetReplicatedMapPuzzleActor(PuzzleActor);
	return true;
}

void UDianaNodeInteractionComponent::HandleMapHackCompleted()
{
	// 완료 콜백은 현재 puzzle owner와 actor가 일치할 때만 장치 trigger
	APuzzleInteractable* PuzzleActor = ActiveMapPuzzleActor;
	UMapHackingPuzzleComponent* PuzzleComponent = ActiveMapPuzzleComponent;
	if (!PuzzleActor || !PuzzleComponent || PuzzleComponent->GetOwner() != PuzzleActor)
	{
		ClearActiveMapHack(false);
		return;
	}

	PuzzleActor->CompletePuzzle();
	Client_ReportNodeInteractionComplete(PuzzleActor, GetNodeDisplayName(PuzzleActor));
	ClearActiveMapHack(false);
}

void UDianaNodeInteractionComponent::HandleMapHackReset()
{
	if (ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>())
	{
		DianaPawn->GetVoiceComponent()->PlayVoiceAction(EDianaVoiceAction::HackingFail);
	}
	Client_ReportMapHackEnded(ActiveMapPuzzleActor);
	ClearActiveMapHack(false);
}

void UDianaNodeInteractionComponent::ClearActiveMapHack(bool bCancelPuzzle)
{
	if (ActiveMapPuzzleComponent)
	{
		ActiveMapPuzzleComponent->OnHackCompleted.RemoveAll(this);
		ActiveMapPuzzleComponent->OnHackReset.RemoveAll(this);
		if (bCancelPuzzle)
		{
			ActiveMapPuzzleComponent->CancelHack();
		}
	}

	ActiveMapPuzzleActor = nullptr;
	ActiveMapPuzzleComponent = nullptr;
	SetReplicatedMapPuzzleActor(nullptr);
}

void UDianaNodeInteractionComponent::SetReplicatedMapPuzzleActor(AActor* PuzzleActor) const
{
	UWorld* World = GetWorld();
	APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (GameState && GameState->HasAuthority())
	{
		GameState->SetActiveMapPuzzleActor(PuzzleActor);
	}
}

void UDianaNodeInteractionComponent::Client_ReportNodeInteractionComplete_Implementation(AActor* CompletedActor, const FString& NodeDisplayName)
{
	if (CompletedActor)
	{
		LocallyCompletedActors.Add(CompletedActor);
		if (LastRequestedMapHackActor == CompletedActor)
		{
			LastRequestedMapHackActor = nullptr;
		}
		if (CurrentFocusedActor == CompletedActor)
		{
			// 완료된 focus prop의 local key guide도 같은 전환 경로로 정리
			SetFocusedNodeActor(nullptr);
		}
	}

	LogNodeMessage(TEXT("Done"), CompletedActor, NodeDisplayName);
}

void UDianaNodeInteractionComponent::Client_ReportNodeInteractionBlocked_Implementation(AActor* BlockedActor, const FString& Reason)
{
	LogNodeMessage(Reason, BlockedActor);
}

void UDianaNodeInteractionComponent::Client_ReportMapHackEnded_Implementation(AActor* EndedActor)
{
	// reset된 map hack node는 같은 focus 상태에서도 다시 자동 요청할 수 있게 함
	if (!EndedActor || LastRequestedMapHackActor == EndedActor)
	{
		LastRequestedMapHackActor = nullptr;
	}
}

void UDianaNodeInteractionComponent::RefreshFocusedNodeLog()
{
	// local log는 서버 판정과 같은 mode/aim gate를 써서 잘못된 hint를 줄임
	if (!CanOwnerUseNodeInteraction())
	{
		if (IsMapHackActive())
		{
			Server_CancelMapHack();
		}
		SetFocusedNodeActor(nullptr);
		return;
	}

	SetFocusedNodeActor(FindBestNodeInteractionTarget());
	UpdateLocalCheckpointFocusPrompt();
	TryRequestFocusedMapHackNode();
}

void UDianaNodeInteractionComponent::TryRequestFocusedMapHackNode()
{
	// MapHackNode는 enemy hack처럼 조준만으로 시작하고 ItemBox는 클릭 입력을 유지
	if (!CurrentFocusedActor || IsMapHackActive() || LastRequestedMapHackActor == CurrentFocusedActor)
	{
		return;
	}

	FPragmataScanResult InteractionData;
	if (!TryGetSupportedPOIInteractionData(CurrentFocusedActor, InteractionData)
		|| InteractionData.TargetKind != EPragmataScanTargetKind::MapHackNode)
	{
		return;
	}

	LastRequestedMapHackActor = CurrentFocusedActor;
	RequestNodeInteraction();
}

AActor* UDianaNodeInteractionComponent::FindBestNodeInteractionTarget() const
{
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	return DianaPawn ? FindBestNodeInteractionTarget(DianaPawn->GetAuthoritativeCameraDirection()) : nullptr;
}

AActor* UDianaNodeInteractionComponent::FindBestNodeInteractionTarget(const FVector& RequestedCameraDirection) const
{
	// camera ray가 실제 node surface에 맞은 경우만 선택해 root 위치 기반 오판정을 막음
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	const AActor* HughActor = GameState ? GameState->GetHughCharacter() : nullptr;
	if (!DianaPawn || !World || !GameState || GameState->AreDianaActionsLocked() || !HughActor)
	{
		return nullptr;
	}

	const FVector CameraOrigin = DianaPawn->GetAuthoritativeCameraOrigin();
	const FVector CameraDirection = RequestedCameraDirection.GetSafeNormal();
	if (CameraDirection.IsNearlyZero())
	{
		return nullptr;
	}

	const FVector InteractionAnchorLocation = HughActor->GetActorLocation();
	FHitResult AimHit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DianaNodeInteractionAim), false, GetOwner());
	bool bHitNodeSurface = World->LineTraceSingleByChannel(
		AimHit,
		CameraOrigin,
		CameraOrigin + CameraDirection * InteractionTraceDistance,
		InteractionTraceChannel,
		Params);
	if (!bHitNodeSurface)
	{
		return nullptr;
	}

	// Elevator floor component만 제외해 뒤의 PuzzleMesh를 재검사하고 다른 blocker는 그대로 유지
	if (const ABossRouteElevator* Elevator = Cast<ABossRouteElevator>(AimHit.GetActor()))
	{
		if (AimHit.GetComponent() == Elevator->GetElevatorMesh())
		{
			Params.AddIgnoredComponent(Elevator->GetElevatorMesh());
			bHitNodeSurface = World->LineTraceSingleByChannel(
				AimHit,
				CameraOrigin,
				CameraOrigin + CameraDirection * InteractionTraceDistance,
				InteractionTraceChannel,
				Params);
		}
	}
	if (!bHitNodeSurface || !AimHit.bBlockingHit)
	{
		return nullptr;
	}

	TArray<AActor*> CandidateActors;
	GameState->GetRegisteredScannablePOIs(CandidateActors);
	// scan 제외 acquisition prop은 실제 camera hit일 때만 interaction 후보에 합류
	if (Cast<ACleanseSkillAcquisitionProp>(AimHit.GetActor()))
	{
		CandidateActors.AddUnique(AimHit.GetActor());
	}
	for (AActor* Candidate : CandidateActors)
	{
		if (IsNodeCandidateValid(Candidate, CameraOrigin, InteractionAnchorLocation, AimHit))
		{
			return Candidate;
		}
	}

	return nullptr;
}

bool UDianaNodeInteractionComponent::CanOwnerUseNodeInteraction() const
{
	// map hack은 Diana role의 Roaming + non-combat aim 전용
	const ADianaOperatorPawn* DianaPawn = GetOwner<ADianaOperatorPawn>();
	const UWorld* World = GetWorld();
	const APragmataGameState* GameState = World ? World->GetGameState<APragmataGameState>() : nullptr;
	if (!DianaPawn || !GameState || GameState->AreDianaActionsLocked() || GameState->IsEnemyCombatActive())
	{
		return false;
	}

	const APragmataPlayerController* PlayerController = Cast<APragmataPlayerController>(DianaPawn->GetController());
	const APragmataPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<APragmataPlayerState>() : nullptr;
	if (!PlayerState || PlayerState->GetAssignedRole() != EPragmataPlayerRole::Diana)
	{
		return false;
	}

	return DianaPawn->GetOperatorMode() == EDianaOperatorMode::Roaming
		&& DianaPawn->GetIsAiming();
}

bool UDianaNodeInteractionComponent::IsNodeCandidateValid(AActor* Candidate, const FVector& CameraOrigin, const FVector& InteractionAnchorLocation, const FHitResult& AimHit) const
{
	// 후보 actor는 POI metadata, Hugh 기준 거리, camera ray의 실제 surface hit를 모두 통과해야 함
	FPragmataScanResult InteractionData;
	if (!TryGetSupportedPOIInteractionData(Candidate, InteractionData) || IsNodeComplete(Candidate))
	{
		return false;
	}

	const FVector CandidateLocation = Candidate->GetActorLocation();
	if (FVector::Dist(InteractionAnchorLocation, CandidateLocation) > InteractionData.DianaInteractionDistance)
	{
		return false;
	}

	if (AimHit.Distance > InteractionTraceDistance)
	{
		return false;
	}

	if (const APuzzleInteractable* PuzzleActor = Cast<APuzzleInteractable>(Candidate))
	{
		// floor를 통과한 뒤에도 child PuzzleMesh 자체를 맞은 경우만 interaction 허용
		if (Cast<ABossRouteElevator>(Candidate->GetAttachParentActor())
			&& AimHit.GetComponent() != PuzzleActor->GetPuzzleInteractionMesh())
		{
			return false;
		}
	}

	return PragmataTraceUtils::IsHitActorWithinCandidate(AimHit, Candidate);
}

bool UDianaNodeInteractionComponent::TryGetSupportedPOIInteractionData(AActor* Candidate, FPragmataScanResult& OutInteractionData) const
{
	// interaction도 scan과 같은 typed metadata를 읽어 class별 중복 판정을 피함
	OutInteractionData = FPragmataScanResult();
	if (const ACleanseSkillAcquisitionProp* AcquisitionProp = Cast<ACleanseSkillAcquisitionProp>(Candidate))
	{
		// Cleanse 획득 prop은 scan 결과를 만들지 않지만 Diana click interaction metadata는 제공
		OutInteractionData.bIsValid = true;
		OutInteractionData.TargetActor = Candidate;
		OutInteractionData.TargetKind = EPragmataScanTargetKind::ItemBox;
		OutInteractionData.DianaInteractionDistance = AcquisitionProp->GetDianaInteractionDistance();
		return OutInteractionData.DianaInteractionDistance > 0.f;
	}
	if (!Candidate
		|| !Candidate->GetClass()->ImplementsInterface(UPropInteractableInterface::StaticClass())
		|| !Candidate->GetClass()->ImplementsInterface(UPragmataScannable::StaticClass()))
	{
		return false;
	}

	OutInteractionData = IPragmataScannable::Execute_GetPragmataScanResult(Candidate, GetOwner());
	if (!OutInteractionData.bIsValid || !OutInteractionData.TargetActor || OutInteractionData.DianaInteractionDistance <= 0.f)
	{
		return false;
	}

	return OutInteractionData.TargetKind == EPragmataScanTargetKind::MapHackNode
		|| OutInteractionData.TargetKind == EPragmataScanTargetKind::ItemBox
		|| OutInteractionData.TargetKind == EPragmataScanTargetKind::Checkpoint;
}

bool UDianaNodeInteractionComponent::IsNodeComplete(AActor* Candidate) const
{
	if (!Candidate)
	{
		return true;
	}

	// 전원 공급장치는 PuzzleInteractable 상속과 무관하게 기존 영구 완료 상태를 우선
	if (const AInteractionPowerSupply* PowerSupply = Cast<AInteractionPowerSupply>(Candidate))
	{
		return PowerSupply->IsInteractionComplete();
	}

	// 재사용 PuzzleInteractable은 local 완료 cache가 아닌 복제된 재무장 상태로 판정
	if (const APuzzleInteractable* Puzzle = Cast<APuzzleInteractable>(Candidate))
	{
		return !Puzzle->CanInteractWithPuzzle();
	}

	if (LocallyCompletedActors.Contains(Candidate))
	{
		return true;
	}

	FPragmataScanResult InteractionData;
	if (!TryGetSupportedPOIInteractionData(Candidate, InteractionData))
	{
		return true;
	}

	if (InteractionData.TargetKind == EPragmataScanTargetKind::ItemBox)
	{
		return false;
	}

	if (InteractionData.TargetKind == EPragmataScanTargetKind::Checkpoint)
	{
		return false;
	}

	if (InteractionData.TargetKind == EPragmataScanTargetKind::MapHackNode)
	{
	}

	return true;
}

void UDianaNodeInteractionComponent::SetFocusedNodeActor(AActor* NewFocusedActor)
{
	if (CurrentFocusedActor == NewFocusedActor)
	{
		return;
	}

	// focus 전환에 맞춰 이전 prop prompt를 먼저 숨겨 화면에 하나만 유지
	if (APropInteractionBase* PreviousFocusedProp = Cast<APropInteractionBase>(CurrentFocusedActor);
		PreviousFocusedProp && PreviousFocusedProp->ShouldShowInteractionPromptOnDianaFocus())
	{
		PreviousFocusedProp->HideInteractionPromptWidget();
	}

	CurrentFocusedActor = NewFocusedActor;
	LastRequestedMapHackActor = nullptr;
	if (APropInteractionBase* FocusedProp = Cast<APropInteractionBase>(CurrentFocusedActor);
		FocusedProp && FocusedProp->ShouldShowInteractionPromptOnDianaFocus())
	{
		FocusedProp->ShowInteractionPromptWidget();
	}

	if (CurrentFocusedActor)
	{
		LogNodeMessage(TEXT("Interactable"), CurrentFocusedActor);
	}
}

void UDianaNodeInteractionComponent::LogNodeMessage(const FString& Prefix, AActor* NodeActor, const FString& FallbackName) const
{
	if (!GEngine)
	{
		return;
	}

	const FString NodeName = !FallbackName.IsEmpty() ? FallbackName : GetNodeDisplayName(NodeActor);
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::White, FString::Printf(TEXT("%s: %s"), *Prefix, *NodeName));
}

FString UDianaNodeInteractionComponent::GetNodeDisplayName(AActor* NodeActor) const
{
	if (!NodeActor)
	{
		return TEXT("UnknownNode");
	}

#if WITH_EDITOR
	const FString ActorLabel = NodeActor->GetActorLabel();
	if (!ActorLabel.IsEmpty())
	{
		return ActorLabel;
	}
#endif

	return NodeActor->GetName();
}
