#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace UnreelMCPScriptHistory
{
	void Record(const FString& Code, bool bAccepted, const FString& Message);
	TArray<TSharedPtr<FJsonValue>> ToJson(int32 Limit);
	void Clear();
}
