#pragma once

#include "CoreMinimal.h"
#include "Props/AnimatedPropInteraction.h"

#include "CheckPoint.generated.h"

UCLASS()
class PRAGMATA_2P_API ACheckPoint : public AAnimatedPropInteraction
{
	GENERATED_BODY()

public:
	ACheckPoint();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintPure, Category="Pragmata|Checkpoint")
	bool IsActivated() const { return bActivated; }

	UFUNCTION(BlueprintPure, Category="Pragmata|Checkpoint")
	FName GetCheckpointId() const { return CheckpointId; }

	FTransform GetRespawnTransform() const;

	void SetLocalInteractionPromptVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;
	virtual bool ShouldCreateInteractionPromptWidget() const override { return false; }

private:
	UPROPERTY(VisibleAnywhere, Category="Pragmata|Checkpoint")
	TObjectPtr<class UWidgetComponent> HoldPromptComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|Checkpoint")
	TSoftClassPtr<class UHoldKeyPromptWidget> HoldPromptWidgetClass;

	// map authored ID와 anchor를 분리해 prop mesh 위치 변경이 respawn 위치를 흔들지 않게 한다.
	UPROPERTY(Replicated, EditInstanceOnly, BlueprintReadOnly, Category="Pragmata|Checkpoint", meta=(AllowPrivateAccess="true"))
	FName CheckpointId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pragmata|Checkpoint", meta=(AllowPrivateAccess="true"))
	TObjectPtr<class USceneComponent> RespawnAnchor = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Pragmata|Checkpoint")
	bool bActivated = false;
};
