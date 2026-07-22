#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TutorialProgressSaveGame.generated.h"

UCLASS()
class PRAGMATA_2P_API UTutorialProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	bool HasShownTutorial(FName TutorialId) const;
	void MarkTutorialShown(FName TutorialId);

private:
	UPROPERTY(SaveGame)
	TSet<FName> ShownTutorialIds;
};
