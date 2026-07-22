#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PragmataHackable.generated.h"

UINTERFACE(BlueprintType)
class PRAGMATA_2P_API UPragmataHackable : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API IPragmataHackable
{
	GENERATED_BODY()

public:
	// HackInstigator는 hack을 요청한 actor이며 target 유효성은 구현체가 판단
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Hack")
	bool CanBeHacked(AActor* HackInstigator) const;

	// 성공한 puzzle 결과와 노드별 hacked 유지 시간을 gameplay effect로 적용
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Hack")
	bool ApplySuccessfulHack(AActor* HackInstigator, float HackDamage, float ResolvedHackDurationSeconds);
};
