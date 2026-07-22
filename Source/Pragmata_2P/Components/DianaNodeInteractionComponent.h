#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "DianaNodeInteractionComponent.generated.h"

class APuzzleInteractable;
class UMapHackingPuzzleComponent;

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UDianaNodeInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDianaNodeInteractionComponent();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Diana|Node Interaction")
	void RequestNodeInteraction();

	void RequestMapHackDirection(EHackDirection Direction);
	void RequestCancelMapHack();
	void CancelActiveMapHackFromServer();
	bool IsMapHackActive() const;
	bool TryStartMapHackFromInteractable(APuzzleInteractable* PuzzleActor);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION(Server, Reliable)
	void Server_RequestNodeInteraction(AActor* RequestedTarget, FVector_NetQuantizeNormal RequestedCameraDirection);

	UFUNCTION(Server, Reliable)
	void Server_ProcessMapHackDirection(EHackDirection Direction);

	UFUNCTION(Server, Reliable)
	void Server_CancelMapHack();

	UFUNCTION(Client, Reliable)
	void Client_ReportNodeInteractionComplete(AActor* CompletedActor, const FString& NodeDisplayName);

	UFUNCTION(Client, Reliable)
	void Client_ReportNodeInteractionBlocked(AActor* BlockedActor, const FString& Reason);

	UFUNCTION(Client, Reliable)
	void Client_ReportMapHackEnded(AActor* EndedActor);

	bool StartMapHack(APuzzleInteractable* PuzzleActor, UMapHackingPuzzleComponent* PuzzleComponent);
	void HandleMapHackCompleted();
	void HandleMapHackReset();
	void ClearActiveMapHack(bool bCancelPuzzle);
	void SetReplicatedMapPuzzleActor(AActor* PuzzleActor) const;
	void RefreshFocusedNodeLog();
	void TryRequestFocusedMapHackNode();
	AActor* FindBestNodeInteractionTarget() const;
	AActor* FindBestNodeInteractionTarget(const FVector& CameraDirection) const;
	bool CanOwnerUseNodeInteraction() const;
	bool IsNodeCandidateValid(AActor* Candidate, const FVector& CameraOrigin, const FVector& InteractionAnchorLocation, const FHitResult& AimHit) const;
	bool TryGetSupportedPOIInteractionData(AActor* Candidate, FPragmataScanResult& OutInteractionData) const;
	bool IsNodeComplete(AActor* Candidate) const;
	void SetFocusedNodeActor(AActor* NewFocusedActor);
	void UpdateLocalCheckpointFocusPrompt();
	void LogNodeMessage(const FString& Prefix, AActor* NodeActor, const FString& FallbackName = FString()) const;
	FString GetNodeDisplayName(AActor* NodeActor) const;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Node Interaction", meta=(ClampMin="0"))
	float InteractionTraceDistance = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Diana Node Interaction")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Visibility;

	UPROPERTY()
	TObjectPtr<AActor> CurrentFocusedActor = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> LastRequestedMapHackActor = nullptr;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> LocallyCompletedActors;

	UPROPERTY()
	TObjectPtr<APuzzleInteractable> ActiveMapPuzzleActor = nullptr;

	UPROPERTY()
	TObjectPtr<UMapHackingPuzzleComponent> ActiveMapPuzzleComponent = nullptr;

	TWeakObjectPtr<class ACheckPoint> LocalFocusedCheckpointPrompt;

	FTimerHandle FocusRefreshTimerHandle;
};
