#include "MCP/UnreelMCPEngineCompat.h"

#include "Misc/EngineVersion.h"

namespace UnreelMCPEngineCompat
{
	FString CurrentEngineMinor()
	{
		const FEngineVersion Version = FEngineVersion::Current();
		return FString::Printf(TEXT("%d.%d"), Version.GetMajor(), Version.GetMinor());
	}

	TArray<FString> DefaultSupportedEngines()
	{
		return { TEXT("5.8") };
	}

	bool IsCurrentEngineSupported(const TArray<FString>& SupportedEngines)
	{
		return SupportedEngines.Num() == 0 || SupportedEngines.Contains(CurrentEngineMinor());
	}
}
