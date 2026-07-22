#include "Data/PragmataGameFlowSettings.h"

#include "Data/PragmataGameFlowConfig.h"

UPragmataGameFlowSettings::UPragmataGameFlowSettings()
{
	GameFlowClass = TSoftClassPtr<UPragmataGameFlowConfig>(FSoftClassPath(TEXT("/Game/Blueprints/GameFlow/BP_GameFlow_Default.BP_GameFlow_Default_C")));
}

// Project Settings의 soft class를 runtime에 해석해 map 전환 설정을 Blueprint CDO에서 읽음
const UPragmataGameFlowConfig* UPragmataGameFlowSettings::GetGameFlowConfig() const
{
	const UClass* ResolvedGameFlowClass = GameFlowClass.IsNull()
		? UPragmataGameFlowConfig::StaticClass()
		: GameFlowClass.LoadSynchronous();
	return ResolvedGameFlowClass ? GetDefault<UPragmataGameFlowConfig>(ResolvedGameFlowClass) : nullptr;
}
