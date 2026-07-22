#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/PragmataTypes.h"
#include "PragmataDamageLibrary.generated.h"

UCLASS()
class PRAGMATA_2P_API UPragmataDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 대상 actor에게 프로젝트 공통 damage contract로 피해 전달
	UFUNCTION(BlueprintCallable, Category="Pragmata|Damage")
	static bool DealDamageToActor(AActor* TargetActor, const FPragmataDamageSpec& DamageSpec);
};
