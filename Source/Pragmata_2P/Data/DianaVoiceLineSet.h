#pragma once

#include "CoreMinimal.h"
#include "Data/PragmataTypes.h"
#include "Engine/DataAsset.h"
#include "DianaVoiceLineSet.generated.h"

// Diana 행동별 음성 풀을 한 에셋으로 보관해 음성 추가와 cooldown 조정을 코드 변경 없이 처리
UCLASS(BlueprintType)
class PRAGMATA_2P_API UDianaVoiceLineSet : public UDataAsset
{
	GENERATED_BODY()

public:
	const FDianaVoiceActionPool* FindPool(EDianaVoiceAction Action) const
	{
		return VoicePools.FindByPredicate([Action](const FDianaVoiceActionPool& Pool)
		{
			return Pool.Action == Action;
		});
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pragmata|Diana Voice")
	TArray<FDianaVoiceActionPool> VoicePools;
};
