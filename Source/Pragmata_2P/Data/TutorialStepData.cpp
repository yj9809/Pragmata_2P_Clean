#include "Data/TutorialStepData.h"

FPrimaryAssetId UTutorialStepData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TutorialStep"), GetFName());
}
