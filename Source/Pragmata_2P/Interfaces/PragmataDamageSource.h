#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PragmataDamageSource.generated.h"

UINTERFACE(BlueprintType)
class PRAGMATA_2P_API UPragmataDamageSource : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API IPragmataDamageSource
{
	GENERATED_BODY()

public:
	// 공격 source actor의 현재 공격 damage 제공
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
	float GetCurrentAttackDamage() const;
};
