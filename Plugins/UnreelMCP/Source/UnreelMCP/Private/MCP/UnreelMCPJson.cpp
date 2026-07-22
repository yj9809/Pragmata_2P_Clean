#include "MCP/UnreelMCPJson.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	bool ShouldExposeAssetAlias(const FUnreelMCPOperation& Operation)
	{
		if (Operation.Id.Contains(TEXT(".create")) || Operation.Id.Contains(TEXT("_create")) || Operation.Id.Contains(TEXT("create_")) || Operation.Id.Contains(TEXT("scratch_pad_script")))
		{
			return false;
		}
		for (const FUnreelMCPParameter& Parameter : Operation.Parameters)
		{
			if (Parameter.Name == TEXT("asset_path"))
			{
				return true;
			}
		}
		return false;
	}
}

namespace UnreelMCPJson
{
	FString RiskToString(EUnreelMCPRisk Risk)
	{
		switch (Risk)
		{
		case EUnreelMCPRisk::ReadOnly:
			return TEXT("read_only");
		case EUnreelMCPRisk::Low:
			return TEXT("low");
		case EUnreelMCPRisk::Medium:
			return TEXT("medium");
		case EUnreelMCPRisk::High:
			return TEXT("high");
		default:
			return TEXT("unknown");
		}
	}

	TSharedPtr<FJsonObject> OperationToJson(const FUnreelMCPOperation& Operation)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("id"), Operation.Id);
		Json->SetStringField(TEXT("domain"), Operation.Domain);
		Json->SetStringField(TEXT("operation"), Operation.Operation);
		Json->SetStringField(TEXT("description"), Operation.Description);
		Json->SetBoolField(TEXT("read_only"), Operation.bReadOnly);
		Json->SetBoolField(TEXT("requires_transaction"), Operation.bRequiresTransaction);
		Json->SetStringField(TEXT("risk"), RiskToString(Operation.Risk));
		if (!Operation.ExampleJson.IsEmpty())
		{
			Json->SetStringField(TEXT("example"), Operation.ExampleJson);
		}
		if (!Operation.SupportNotes.IsEmpty())
		{
			Json->SetStringField(TEXT("support_notes"), Operation.SupportNotes);
		}

		TArray<TSharedPtr<FJsonValue>> KeywordValues;
		for (const FString& Keyword : Operation.Keywords)
		{
			KeywordValues.Add(MakeShared<FJsonValueString>(Keyword));
		}
		Json->SetArrayField(TEXT("keywords"), KeywordValues);

		TArray<TSharedPtr<FJsonValue>> EngineValues;
		for (const FString& Engine : Operation.SupportedEngines)
		{
			EngineValues.Add(MakeShared<FJsonValueString>(Engine));
		}
		Json->SetArrayField(TEXT("supported_engines"), EngineValues);

		TArray<TSharedPtr<FJsonValue>> ParameterValues;
		for (const FUnreelMCPParameter& Parameter : Operation.Parameters)
		{
			TSharedPtr<FJsonObject> ParamJson = MakeShared<FJsonObject>();
			ParamJson->SetStringField(TEXT("name"), Parameter.Name);
			ParamJson->SetStringField(TEXT("type"), Parameter.Type);
			ParamJson->SetStringField(TEXT("description"), Parameter.Description);
			ParamJson->SetBoolField(TEXT("required"), Parameter.bRequired);
			if (!Parameter.DefaultValue.IsEmpty())
			{
				ParamJson->SetStringField(TEXT("default"), Parameter.DefaultValue);
			}
			ParameterValues.Add(MakeShared<FJsonValueObject>(ParamJson));
		}
		if (ShouldExposeAssetAlias(Operation))
		{
			TSharedPtr<FJsonObject> AliasJson = MakeShared<FJsonObject>();
			AliasJson->SetStringField(TEXT("name"), TEXT("asset"));
			AliasJson->SetStringField(TEXT("type"), TEXT("string"));
			AliasJson->SetStringField(TEXT("description"), TEXT("Alias for asset_path when referencing an existing asset."));
			AliasJson->SetBoolField(TEXT("required"), false);
			ParameterValues.Add(MakeShared<FJsonValueObject>(AliasJson));
		}
		Json->SetArrayField(TEXT("parameters"), ParameterValues);
		return Json;
	}

	TSharedPtr<FJsonObject> ResultToJson(const FUnreelMCPResult& Result)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), Result.bSuccess);
		Json->SetStringField(TEXT("message"), Result.Message);
		Json->SetObjectField(TEXT("data"), Result.Data.IsValid() ? Result.Data : MakeShared<FJsonObject>());

		TArray<TSharedPtr<FJsonValue>> WarningValues;
		for (const FString& Warning : Result.Warnings)
		{
			WarningValues.Add(MakeShared<FJsonValueString>(Warning));
		}
		Json->SetArrayField(TEXT("warnings"), WarningValues);

		TArray<TSharedPtr<FJsonValue>> ChangedAssetValues;
		for (const FString& ChangedAsset : Result.ChangedAssets)
		{
			ChangedAssetValues.Add(MakeShared<FJsonValueString>(ChangedAsset));
		}
		Json->SetArrayField(TEXT("changed_assets"), ChangedAssetValues);
		return Json;
	}

	FString Stringify(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return JsonString;
	}

	TSharedPtr<FJsonObject> ParseRequestBody(const TArray<uint8>& Body, FString& OutError)
	{
		if (Body.Num() == 0)
		{
			return MakeShared<FJsonObject>();
		}

		TArray<uint8> NullTerminatedBody = Body;
		NullTerminatedBody.Add(0);
		const FString BodyString = UTF8_TO_TCHAR(reinterpret_cast<const char*>(NullTerminatedBody.GetData()));

		TSharedPtr<FJsonObject> Json;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyString);
		if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
		{
			OutError = TEXT("Invalid JSON request body");
			return nullptr;
		}
		return Json;
	}
}
