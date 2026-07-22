#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

namespace UnreelMCPJson
{
	FString RiskToString(EUnreelMCPRisk Risk);
	TSharedPtr<FJsonObject> OperationToJson(const FUnreelMCPOperation& Operation);
	TSharedPtr<FJsonObject> ResultToJson(const FUnreelMCPResult& Result);
	FString Stringify(const TSharedRef<FJsonObject>& JsonObject);
	TSharedPtr<FJsonObject> ParseRequestBody(const TArray<uint8>& Body, FString& OutError);
}
