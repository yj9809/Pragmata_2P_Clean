#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Engine/DataAsset.h"
#include "LobbyPreviewAnimationSet.generated.h"

class UAnimationAsset;

USTRUCT(BlueprintType)
struct PRAGMATA_2P_API FLobbyPreviewAnimationSequence
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby")
	EPragmataPlayerRole PreviewRole = EPragmataPlayerRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby")
	EPragmataPlayerRole FocusedRole = EPragmataPlayerRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby")
	TSoftObjectPtr<UAnimationAsset> StartAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby")
	TSoftObjectPtr<UAnimationAsset> LoopAnimation;
};

UCLASS(BlueprintType)
class PRAGMATA_2P_API ULobbyPreviewAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	bool FindSequence(EPragmataPlayerRole PreviewRole, EPragmataPlayerRole FocusedRole, FLobbyPreviewAnimationSequence& OutSequence) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Lobby", meta=(AllowPrivateAccess="true"))
	TArray<FLobbyPreviewAnimationSequence> Sequences;
};
