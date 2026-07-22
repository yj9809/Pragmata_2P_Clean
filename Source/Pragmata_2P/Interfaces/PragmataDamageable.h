#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/PragmataTypes.h"
#include "PragmataDamageable.generated.h"

UINTERFACE(BlueprintType)
class PRAGMATA_2P_API UPragmataDamageable : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API IPragmataDamageable
{
	GENERATED_BODY()

public:
	// 대상이 현재 피해를 받을 수 있는지 확인
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
	bool CanReceiveDamage(const FPragmataDamageSpec& DamageSpec) const;

	// 대상 actor가 피해를 수신
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Damage")
	bool ReceiveDamage(const FPragmataDamageSpec& DamageSpec);
};
