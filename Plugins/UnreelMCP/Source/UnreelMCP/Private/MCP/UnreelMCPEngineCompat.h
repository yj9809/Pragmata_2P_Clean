#pragma once

#include "CoreMinimal.h"

namespace UnreelMCPEngineCompat
{
	FString CurrentEngineMinor();
	TArray<FString> DefaultSupportedEngines();
	bool IsCurrentEngineSupported(const TArray<FString>& SupportedEngines);
}
