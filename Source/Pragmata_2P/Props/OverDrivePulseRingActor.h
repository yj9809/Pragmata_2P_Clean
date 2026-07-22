#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OverDrivePulseRingActor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(NotBlueprintable)
class PRAGMATA_2P_API AOverDrivePulseRingActor : public AActor
{
	GENERATED_BODY()

public:
	AOverDrivePulseRingActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void UpdateRing(float Radius);

	UPROPERTY(VisibleAnywhere, Category="Pragmata|OverDrive")
	TObjectPtr<UInstancedStaticMeshComponent> RingSegments = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="0.1", Units="s"))
	float ExpansionDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="1.0", Units="cm"))
	float MaximumRadius = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category="Pragmata|OverDrive", meta=(ClampMin="8", ClampMax="96"))
	int32 SegmentCount = 48;

	float StartTimeSeconds = 0.f;
};
