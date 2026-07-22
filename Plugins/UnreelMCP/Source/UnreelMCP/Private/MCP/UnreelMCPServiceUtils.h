#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
class FProperty;
class UObject;
class UStruct;

namespace UnreelMCPServiceUtils
{
	bool IsCreationOperation(const FString& OperationId);
	void NormalizeAssetAlias(const FString& OperationId, const TSharedRef<FJsonObject>& Params);
	FString MissingAssetPathMessage(const FString& ExamplePath = FString());
	FString ScriptLoadHint(const FString& Path);
	FString LoadFailureMessage(const FString& Kind, const FString& Path);
	bool IsPythonExecutionEnabled();
	TSharedPtr<FJsonObject> BuildPythonStatus();
	FString CoerceImportText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value, const FString& FallbackImportText);
	FString ExportObjectPropertyText(UObject* Target, const FString& PropertyName);
	FString ExportStructPropertyText(const UStruct* Struct, const void* StructMemory, UObject* Owner, const FString& PropertyName);
	TArray<FString> WritablePropertyNames(const UStruct* Struct);
	TArray<FString> SimilarPropertyNames(const UStruct* Struct, const FString& RequestedName);
	TSharedPtr<FJsonObject> BuildPropertyChange(const FString& Target, const FString& PropertyName, const FString& Before, const FString& After);
}
