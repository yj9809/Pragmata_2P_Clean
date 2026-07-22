#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/PragmataTypes.h"
#include "TimerManager.h"
#include "PairedAnimationComponent.generated.h"

class UPairedAnimationSet;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent))
class PRAGMATA_2P_API UPairedAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPairedAnimationComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeMeshes(USkeletalMeshComponent* InHughMesh, USkeletalMeshComponent* InDianaMesh);

	UFUNCTION(BlueprintCallable, Category="Pragmata|Animation")
	void TriggerPairedCue(EPairedAnimationCueType CueType, EPragmataHitReactStrength Strength, EPragmataHitDirection Direction);

	UFUNCTION(BlueprintPure, Category="Pragmata|Animation")
	const FPairedAnimationCue& GetLastCue() const { return LastCue; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Animation")
	static bool IsForcedDianaInterruptCue(const FPairedAnimationCue& Cue);

private:
	UFUNCTION()
	void OnRep_LastCue();

	void PlayCue(const FPairedAnimationCue& Cue) const;
	float GetCueStartOffset(const FPairedAnimationCue& Cue) const;
	void LockDianaActionsForCue(const FPairedAnimationCue& Cue);
	void ReleaseDianaActionLock();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPairedAnimationSet> AnimationSet = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Animation", meta=(AllowedClasses="/Script/Pragmata_2P.PairedAnimationSet"))
	TSoftObjectPtr<UPairedAnimationSet> DefaultAnimationSet;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> HughMesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> DianaMesh = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_LastCue)
	FPairedAnimationCue LastCue;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Animation", meta=(ClampMin="0"))
	float DefaultActionLockDuration = 1.f;

	FTimerHandle DianaActionLockTimerHandle;
};
