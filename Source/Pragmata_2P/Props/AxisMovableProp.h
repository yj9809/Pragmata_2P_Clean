#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AxisMovableProp.generated.h"

class APropInteractionBase;
class UChildActorComponent;
class USceneComponent;

UCLASS()
class PRAGMATA_2P_API AAxisMovableProp : public AActor
{
	GENERATED_BODY()

public:
	AAxisMovableProp();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category="Pragmata|Movable Prop")
	void ToggleMoved();

	UFUNCTION(BlueprintCallable, Category="Pragmata|Movable Prop")
	void SetMoved(bool bNewMoved);

	UFUNCTION(BlueprintPure, Category="Pragmata|Movable Prop")
	bool IsMoved() const { return bIsAtTarget; }

protected:
	virtual bool IsMovementInteractionEnabled() const { return true; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UChildActorComponent> InteractionActorComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Pragmata|Movable Prop")
	FVector MoveDirection = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Pragmata|Movable Prop", meta=(ClampMin="0"))
	float MoveDistance = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Pragmata|Movable Prop", meta=(ClampMin="0"))
	float MoveDuration = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pragmata|Movable Prop")
	bool bStartMoved = false;

private:
	UFUNCTION()
	void OnRep_IsAtTarget();

	UFUNCTION()
	void HandleInteractionActivated(AActor* Interactor);

	void CacheInitialLocationIfNeeded();
	FVector GetMoveOffset() const;
	FVector GetLocationForState(bool bMoved) const;
	void StartMoveToState(bool bMoved);
	void FinishMovement();
	APropInteractionBase* ResolveInteractionActor() const;

	UPROPERTY(ReplicatedUsing=OnRep_IsAtTarget)
	bool bIsAtTarget = false;

	FVector InitialLocation = FVector::ZeroVector;
	FVector MoveStartLocation = FVector::ZeroVector;
	FVector MoveTargetLocation = FVector::ZeroVector;
	float MoveElapsedTime = 0.f;
	bool bIsMoving = false;
	bool bHasInitialLocation = false;
};
