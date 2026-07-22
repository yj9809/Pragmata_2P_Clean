#include "MCP/UnreelMCPServiceUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Char.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
	FString ScalarToImportText(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid() || Value->IsNull())
		{
			return TEXT("");
		}
		FString StringValue;
		if (Value->TryGetString(StringValue))
		{
			return StringValue;
		}
		bool BoolValue = false;
		if (Value->TryGetBool(BoolValue))
		{
			return BoolValue ? TEXT("True") : TEXT("False");
		}
		double NumberValue = 0.0;
		if (Value->TryGetNumber(NumberValue))
		{
			return FString::SanitizeFloat(NumberValue);
		}
		return TEXT("");
	}
}

namespace UnreelMCPServiceUtils
{
	bool IsCreationOperation(const FString& OperationId)
	{
		return OperationId.Contains(TEXT(".create"))
			|| OperationId.Contains(TEXT("_create"))
			|| OperationId.Contains(TEXT("create_"));
	}

	void NormalizeAssetAlias(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
	{
		FString ExistingAssetPath;
		if (IsCreationOperation(OperationId) || Params->TryGetStringField(TEXT("asset_path"), ExistingAssetPath))
		{
			return;
		}

		FString AssetAlias;
		if (Params->TryGetStringField(TEXT("asset"), AssetAlias) && !AssetAlias.IsEmpty())
		{
			Params->SetStringField(TEXT("asset_path"), AssetAlias);
		}
	}

	FString MissingAssetPathMessage(const FString& ExamplePath)
	{
		if (ExamplePath.IsEmpty())
		{
			return TEXT("Missing required parameter: asset_path (alias: asset). Example: {\"asset_path\":\"/Game/Path/Asset\"}");
		}
		return FString::Printf(TEXT("Missing required parameter: asset_path (alias: asset). Example: {\"asset_path\":\"%s\"}"), *ExamplePath);
	}

	FString ScriptLoadHint(const FString& Path)
	{
		if (!Path.StartsWith(TEXT("/Script/")))
		{
			return FString();
		}
		return TEXT(" If this is a project C++ type, rebuild the project, make sure stale editor DLLs are not loaded, and restart the editor.");
	}

	FString LoadFailureMessage(const FString& Kind, const FString& Path)
	{
		return FString::Printf(TEXT("Failed to load %s: %s.%s"), *Kind, *Path, *ScriptLoadHint(Path));
	}

	bool IsPythonExecutionEnabled()
	{
		return FPlatformMisc::GetEnvironmentVariable(TEXT("UNREEL_MCP_ENABLE_PYTHON")) == TEXT("1");
	}

	TSharedPtr<FJsonObject> BuildPythonStatus()
	{
		const bool bEnabled = IsPythonExecutionEnabled();
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("enabled"), bEnabled);
		Data->SetStringField(TEXT("enable_with"), TEXT("UNREEL_MCP_ENABLE_PYTHON=1"));
		Data->SetBoolField(TEXT("requires_editor_relaunch"), !bEnabled);
		return Data;
	}

	FString CoerceImportText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value, const FString& FallbackImportText)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || !StructProperty->Struct || !StructProperty->Struct->GetName().StartsWith(TEXT("ValueOrBBKey_")))
		{
			return FallbackImportText;
		}

		const FString ScalarText = ScalarToImportText(Value);
		if (ScalarText.IsEmpty())
		{
			return FallbackImportText;
		}
		return FString::Printf(TEXT("(DefaultValue=%s,Key=\"\")"), *ScalarText);
	}

	FString ExportObjectPropertyText(UObject* Target, const FString& PropertyName)
	{
		if (!Target)
		{
			return FString();
		}
		FProperty* Property = Target->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			return FString();
		}
		FString Exported;
		Property->ExportText_Direct(Exported, Property->ContainerPtrToValuePtr<void>(Target), nullptr, Target, PPF_None);
		return Exported;
	}

	FString ExportStructPropertyText(const UStruct* Struct, const void* StructMemory, UObject* Owner, const FString& PropertyName)
	{
		if (!Struct || !StructMemory)
		{
			return FString();
		}
		FProperty* Property = Struct->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			return FString();
		}
		FString Exported;
		Property->ExportText_Direct(Exported, Property->ContainerPtrToValuePtr<void>(const_cast<void*>(StructMemory)), nullptr, Owner, PPF_None);
		return Exported;
	}

	TArray<FString> WritablePropertyNames(const UStruct* Struct)
	{
		TArray<FString> Names;
		if (!Struct)
		{
			return Names;
		}
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_DisableEditOnInstance))
			{
				Names.Add(Property->GetName());
			}
		}
		Names.Sort();
		return Names;
	}

	TArray<FString> SimilarPropertyNames(const UStruct* Struct, const FString& RequestedName)
	{
		TArray<FString> Similar;
		const FString RequestedLower = RequestedName.ToLower();
		for (const FString& Name : WritablePropertyNames(Struct))
		{
			const FString LowerName = Name.ToLower();
			if (LowerName.Contains(RequestedLower) || RequestedLower.Contains(LowerName))
			{
				Similar.Add(Name);
			}
			if (Similar.Num() >= 12)
			{
				break;
			}
		}
		if (Similar.IsEmpty())
		{
			TArray<FString> AllNames = WritablePropertyNames(Struct);
			const int32 Count = FMath::Min(12, AllNames.Num());
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Similar.Add(AllNames[Index]);
			}
		}
		return Similar;
	}

	TSharedPtr<FJsonObject> BuildPropertyChange(const FString& Target, const FString& PropertyName, const FString& Before, const FString& After)
	{
		TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
		Change->SetStringField(TEXT("target"), Target);
		Change->SetStringField(TEXT("property"), PropertyName);
		Change->SetStringField(TEXT("before"), Before);
		Change->SetStringField(TEXT("after"), After);
		return Change;
	}
}
