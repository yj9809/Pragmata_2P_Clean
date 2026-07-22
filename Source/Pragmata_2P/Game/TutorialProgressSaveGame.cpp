#include "Game/TutorialProgressSaveGame.h"

bool UTutorialProgressSaveGame::HasShownTutorial(FName TutorialId) const
{
	return TutorialId != NAME_None && ShownTutorialIds.Contains(TutorialId);
}

void UTutorialProgressSaveGame::MarkTutorialShown(FName TutorialId)
{
	if (TutorialId != NAME_None)
	{
		ShownTutorialIds.Add(TutorialId);
	}
}
