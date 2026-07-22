#include "Data/PragmataGameFlowConfig.h"

#include "Data/PragmataGameFlowSettings.h"
#include "Engine/World.h"
#include "UI/PragmataGameClearWidget.h"
#include "UObject/SoftObjectPath.h"

// C++ fallback은 BP_GameFlow_Default가 아직 없어도 Title->Lobby->LunarMap_T 흐름을 유지
UPragmataGameFlowConfig::UPragmataGameFlowConfig()
{
	LobbyMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/MatchLobby.MatchLobby")));
	LobbyGameMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/LunarMap_T.LunarMap_T")));
	TitleMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Title.Title")));
	GameplayGameMode = TSoftClassPtr<AGameModeBase>(FSoftClassPath(TEXT("/Game/Blueprints/BP_PragGameMode.BP_PragGameMode_C")));
	GameClearWidgetClass = TSoftClassPtr<UPragmataGameClearWidget>(FSoftClassPath(TEXT("/Game/UI/Widget/WBP_GameClear.WBP_GameClear_C")));
}

// Project Settings가 지정한 Blueprint config CDO를 우선 사용하고 없으면 C++ 기본값 사용
const UPragmataGameFlowConfig* UPragmataGameFlowConfig::Get(const UObject* /*WorldContextObject*/)
{
	if (const UPragmataGameFlowSettings* GameFlowSettings = GetDefault<UPragmataGameFlowSettings>())
	{
		if (const UPragmataGameFlowConfig* GameFlowConfig = GameFlowSettings->GetGameFlowConfig())
		{
			return GameFlowConfig;
		}
	}

	return GetDefault<UPragmataGameFlowConfig>();
}

FString UPragmataGameFlowConfig::GetLobbyMapPath() const
{
	return ResolveMapPath(LobbyMap);
}

FString UPragmataGameFlowConfig::GetLobbyGameMapPath() const
{
	return ResolveMapPath(LobbyGameMap);
}

FString UPragmataGameFlowConfig::GetTitleMapPath() const
{
	return ResolveMapPath(TitleMap);
}

FString UPragmataGameFlowConfig::GetGameplayGameModePath() const
{
	return GameplayGameMode.IsNull() ? FString() : GameplayGameMode.ToSoftObjectPath().ToString();
}

FString UPragmataGameFlowConfig::ResolveMapPath(const TSoftObjectPtr<UWorld>& MapReference)
{
	return MapReference.IsNull() ? FString() : MapReference.ToSoftObjectPath().GetLongPackageName();
}
