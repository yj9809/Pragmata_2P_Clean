#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/PragmataTypes.h"
#include "PragmataScannable.generated.h"

UINTERFACE(BlueprintType)
class PRAGMATA_2P_API UPragmataScannable : public UInterface
{
	GENERATED_BODY()
};

class PRAGMATA_2P_API IPragmataScannable
{
	GENERATED_BODY()

public:
	// ScanRequester는 scan을 요청한 actor이며 거리/LOS 유효성은 scan component가 검증
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Pragmata|Scan")
	FPragmataScanResult GetPragmataScanResult(AActor* ScanRequester) const;
};
