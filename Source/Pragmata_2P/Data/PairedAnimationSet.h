#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/PragmataTypes.h"
#include "PairedAnimationSet.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FPairedAnimationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation")
	EPairedAnimationCueType CueType = EPairedAnimationCueType::HitReact;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation")
	EPragmataHitReactStrength Strength = EPragmataHitReactStrength::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation")
	EPragmataHitDirection Direction = EPragmataHitDirection::Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation")
	TObjectPtr<UAnimMontage> HughMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation")
	TObjectPtr<UAnimMontage> DianaMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation", meta=(ClampMin="0.01"))
	float PlayRate = 1.f;
};

UCLASS(BlueprintType)
class PRAGMATA_2P_API UPairedAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	// Match order: cue+strength+direction, then cue+strength, then cue-only fallback.
	const FPairedAnimationEntry* FindEntry(const FPairedAnimationCue& Cue) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Animation", meta=(AllowPrivateAccess="true"))
	TArray<FPairedAnimationEntry> Entries;
};
