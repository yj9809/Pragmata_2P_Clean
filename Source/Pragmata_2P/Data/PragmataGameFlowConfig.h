#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/Object.h"
#include "PragmataGameFlowConfig.generated.h"

class UWorld;
class UPragmataGameClearWidget;

UCLASS(BlueprintType, Blueprintable)
class PRAGMATA_2P_API UPragmataGameFlowConfig : public UObject
{
	GENERATED_BODY()

public:
	UPragmataGameFlowConfig();

	static const UPragmataGameFlowConfig* Get(const UObject* WorldContextObject);

	FString GetLobbyMapPath() const;
	FString GetLobbyGameMapPath() const;
	FString GetTitleMapPath() const;
	FString GetGameplayGameModePath() const;
	TSoftClassPtr<UPragmataGameClearWidget> GetGameClearWidgetClass() const { return GameClearWidgetClass; }

private:
	static FString ResolveMapPath(const TSoftObjectPtr<UWorld>& MapReference);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UWorld> LobbyMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UWorld> LobbyGameMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftObjectPtr<UWorld> TitleMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<AGameModeBase> GameplayGameMode;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pragmata|GameFlow", meta=(AllowPrivateAccess="true"))
	TSoftClassPtr<UPragmataGameClearWidget> GameClearWidgetClass;
};
