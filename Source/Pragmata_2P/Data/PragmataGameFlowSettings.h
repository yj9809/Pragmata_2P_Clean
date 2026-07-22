#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PragmataGameFlowSettings.generated.h"

class UPragmataGameFlowConfig;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Pragmata Game Flow"))
class PRAGMATA_2P_API UPragmataGameFlowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPragmataGameFlowSettings();

	const UPragmataGameFlowConfig* GetGameFlowConfig() const;

private:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<UPragmataGameFlowConfig> GameFlowClass;
};
