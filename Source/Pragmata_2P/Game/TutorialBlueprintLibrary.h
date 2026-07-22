#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TutorialBlueprintLibrary.generated.h"

class UTutorialStepData;

UCLASS()
class PRAGMATA_2P_API UTutorialBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 안정된 TutorialId를 기준으로 로컬 저장 이력에 없는 tutorial만 표시
	UFUNCTION(BlueprintCallable, Category="Pragmata|Tutorial", meta=(WorldContext="WorldContextObject"))
	static bool ShowTutorialOnce(const UObject* WorldContextObject, FName TutorialId, UTutorialStepData* StepData);

	static bool ShowTutorialOnceWithPresentationSerial(
		const UObject* WorldContextObject,
		FName TutorialId,
		UTutorialStepData* StepData,
		uint32& OutPresentationSerial);
};
