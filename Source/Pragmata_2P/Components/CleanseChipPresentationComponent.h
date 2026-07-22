#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPtr.h"
#include "CleanseChipPresentationComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(Pragmata), meta=(BlueprintSpawnableComponent=false))
class PRAGMATA_2P_API UCleanseChipPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void Configure(USkeletalMeshComponent* InDianaMesh, const TSoftObjectPtr<USkeletalMesh>& InChipMesh);
	void BeginChipHold();
	void EndChipHold();
	void ResetPresentation();

private:
	bool EnsureChipMeshComponent();

	TWeakObjectPtr<USkeletalMeshComponent> DianaMesh;
	TSoftObjectPtr<USkeletalMesh> ChipMesh;
	TObjectPtr<USkeletalMeshComponent> ChipMeshComponent = nullptr;
};
