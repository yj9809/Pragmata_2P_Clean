#include "MCP/Services/UnreelMCPEditorService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "MCP/Services/UnreelMCPTransactionService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	FUnreelMCPParameter Param(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FString AssetDataClassString(const FAssetData& AssetData)
	{
		return AssetData.AssetClassPath.ToString();
	}
}

FString FUnreelMCPEditorService::GetServiceName() const
{
	return TEXT("editor");
}

TArray<FUnreelMCPOperation> FUnreelMCPEditorService::GetOperations() const
{
	return {
		FUnreelMCPOperation{
			TEXT("actor.list"),
			TEXT("actor"),
			TEXT("list"),
			TEXT("List actors in the current editor world with name, label, class, and transform."),
			{ TEXT("actor"), TEXT("scene"), TEXT("level"), TEXT("list") },
			{ Param(TEXT("name_filter"), TEXT("string"), TEXT("Optional substring filter against actor name or label.")),
			  Param(TEXT("limit"), TEXT("integer"), TEXT("Maximum actors to return."), false, TEXT("100")) },
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"actor.list\",\"params\":{\"name_filter\":\"Player\",\"limit\":50}}")
		},
		FUnreelMCPOperation{
			TEXT("actor.spawn"),
			TEXT("actor"),
			TEXT("spawn"),
			TEXT("Spawn an actor in the current editor world. Defaults to /Script/Engine.Actor when no class is provided."),
			{ TEXT("actor"), TEXT("scene"), TEXT("spawn"), TEXT("create") },
			{ Param(TEXT("class_path"), TEXT("string"), TEXT("Actor class path, e.g. /Script/Engine.PointLight.")),
			  Param(TEXT("label"), TEXT("string"), TEXT("Optional editor label for the new actor.")),
			  Param(TEXT("location"), TEXT("object"), TEXT("Spawn location {x,y,z}.")),
			  Param(TEXT("rotation"), TEXT("object"), TEXT("Spawn rotation {x,y,z}; maps to pitch,yaw,roll.")) },
			false,
			true,
			EUnreelMCPRisk::Medium,
			TEXT("{\"operation_id\":\"actor.spawn\",\"params\":{\"class_path\":\"/Script/Engine.PointLight\",\"label\":\"KeyLight\",\"location\":{\"x\":0,\"y\":0,\"z\":300}}}")
		},
		FUnreelMCPOperation{
			TEXT("asset.search"),
			TEXT("asset"),
			TEXT("search"),
			TEXT("Search Content Browser assets by directory, class, and name substring."),
			{ TEXT("asset"), TEXT("content browser"), TEXT("search"), TEXT("find") },
			{ Param(TEXT("directory"), TEXT("string"), TEXT("Package directory to search."), false, TEXT("/Game")),
			  Param(TEXT("class_filter"), TEXT("string"), TEXT("Optional class name or class path substring.")),
			  Param(TEXT("name_filter"), TEXT("string"), TEXT("Optional asset name substring.")),
			  Param(TEXT("limit"), TEXT("integer"), TEXT("Maximum assets to return."), false, TEXT("100")) },
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"asset.search\",\"params\":{\"directory\":\"/Game\",\"class_filter\":\"Blueprint\",\"name_filter\":\"Player\"}}")
		},
		FUnreelMCPOperation{
			TEXT("asset.info"),
			TEXT("asset"),
			TEXT("info"),
			TEXT("Return basic metadata for one asset path."),
			{ TEXT("asset"), TEXT("metadata"), TEXT("info") },
			{ Param(TEXT("asset_path"), TEXT("string"), TEXT("Asset object path or package path."), true) },
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"asset.info\",\"params\":{\"asset_path\":\"/Game/Blueprints/BP_Player\"}}")
		},
		FUnreelMCPOperation{
			TEXT("asset.save"),
			TEXT("asset"),
			TEXT("save"),
			TEXT("Save a loaded asset package by path."),
			{ TEXT("asset"), TEXT("asset.save"), TEXT("save"), TEXT("save asset"), TEXT("save asset package"), TEXT("package"), TEXT("save package") },
			{ Param(TEXT("asset_path"), TEXT("string"), TEXT("Asset object path or package path."), true) },
			false,
			true,
			EUnreelMCPRisk::Low,
			TEXT("{\"operation_id\":\"asset.save\",\"params\":{\"asset_path\":\"/Game/Blueprints/BP_Player\"}}")
		},
		FUnreelMCPOperation{
			TEXT("log.tail"),
			TEXT("log"),
			TEXT("tail"),
			TEXT("Read the tail of the current project log file."),
			{ TEXT("log"), TEXT("output"), TEXT("tail"), TEXT("diagnostics") },
			{ Param(TEXT("max_chars"), TEXT("integer"), TEXT("Maximum characters from the end of the log."), false, TEXT("12000")) },
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"log.tail\",\"params\":{\"max_chars\":8000}}")
		},
		FUnreelMCPOperation{
			TEXT("viewport.screenshot"),
			TEXT("viewport"),
			TEXT("screenshot"),
			TEXT("Request a viewport screenshot and return the target file path."),
			{ TEXT("viewport"), TEXT("screenshot"), TEXT("capture") },
			{ Param(TEXT("filename"), TEXT("string"), TEXT("Optional absolute or Saved/Screenshots-relative filename.")) },
			false,
			false,
			EUnreelMCPRisk::Low,
			TEXT("{\"operation_id\":\"viewport.screenshot\",\"params\":{\"filename\":\"UnreelMCP_Capture.png\"}}")
		}
	};
}

FUnreelMCPResult FUnreelMCPEditorService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("actor.list")) return ListActors(Params);
	if (OperationId == TEXT("actor.spawn")) return SpawnActor(Params);
	if (OperationId == TEXT("asset.search")) return SearchAssets(Params);
	if (OperationId == TEXT("asset.info")) return GetAssetInfo(Params);
	if (OperationId == TEXT("asset.save")) return SaveAsset(Params);
	if (OperationId == TEXT("log.tail")) return TailLog(Params);
	if (OperationId == TEXT("viewport.screenshot")) return RequestScreenshot(Params);
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown editor operation: %s"), *OperationId));
}

UWorld* FUnreelMCPEditorService::GetEditorWorld(FString& OutError) const
{
	if (!GEditor)
	{
		OutError = TEXT("GEditor is not available");
		return nullptr;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		OutError = TEXT("Editor world is not available");
	}
	return World;
}

FVector FUnreelMCPEditorService::ReadVector(const TSharedRef<FJsonObject>& Params, const FString& FieldName, const FVector& DefaultValue) const
{
	const TSharedPtr<FJsonObject>* Object = nullptr;
	if (!Params->TryGetObjectField(FieldName, Object) || !Object || !Object->IsValid())
	{
		return DefaultValue;
	}

	double X = DefaultValue.X;
	double Y = DefaultValue.Y;
	double Z = DefaultValue.Z;
	(*Object)->TryGetNumberField(TEXT("x"), X);
	(*Object)->TryGetNumberField(TEXT("y"), Y);
	(*Object)->TryGetNumberField(TEXT("z"), Z);
	return FVector(X, Y, Z);
}

FUnreelMCPResult FUnreelMCPEditorService::ListActors(const TSharedRef<FJsonObject>& Params) const
{
	FString Error;
	UWorld* World = GetEditorWorld(Error);
	if (!World) return FUnreelMCPResult::Error(Error);

	FString NameFilter;
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);
	int32 Limit = 100;
	double LimitValue = Limit;
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	Limit = static_cast<int32>(LimitValue);
	Limit = FMath::Clamp(Limit, 1, 1000);

	TArray<TSharedPtr<FJsonValue>> ActorValues;
	int32 Total = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		const FString Name = Actor->GetName();
		const FString Label = Actor->GetActorLabel();
		if (!NameFilter.IsEmpty() && !Name.Contains(NameFilter) && !Label.Contains(NameFilter))
		{
			continue;
		}

		Total++;
		if (ActorValues.Num() >= Limit)
		{
			continue;
		}

		TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("name"), Name);
		ActorJson->SetStringField(TEXT("label"), Label);
		ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
		const FVector Location = Actor->GetActorLocation();
		ActorJson->SetNumberField(TEXT("x"), Location.X);
		ActorJson->SetNumberField(TEXT("y"), Location.Y);
		ActorJson->SetNumberField(TEXT("z"), Location.Z);
		ActorValues.Add(MakeShared<FJsonValueObject>(ActorJson));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("total"), Total);
	Data->SetArrayField(TEXT("actors"), ActorValues);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Found %d actor(s)"), Total), Data);
}

FUnreelMCPResult FUnreelMCPEditorService::SpawnActor(const TSharedRef<FJsonObject>& Params) const
{
	FString Error;
	UWorld* World = GetEditorWorld(Error);
	if (!World) return FUnreelMCPResult::Error(Error);

	FString ClassPath = TEXT("/Script/Engine.Actor");
	Params->TryGetStringField(TEXT("class_path"), ClassPath);

	UClass* ActorClass = StaticLoadClass(AActor::StaticClass(), nullptr, *ClassPath);
	if (!ActorClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("actor class"), ClassPath));
	}

	const FVector Location = ReadVector(Params, TEXT("location"), FVector::ZeroVector);
	const FVector RotationVector = ReadVector(Params, TEXT("rotation"), FVector::ZeroVector);
	const FRotator Rotation(RotationVector.X, RotationVector.Y, RotationVector.Z);

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SpawnActor", "UnreelMCP Spawn Actor"));
	AActor* Actor = World->SpawnActor<AActor>(ActorClass, Location, Rotation);
	if (!Actor)
	{
		return FUnreelMCPResult::Error(TEXT("SpawnActor returned null"));
	}

	FString Label;
	if (Params->TryGetStringField(TEXT("label"), Label) && !Label.IsEmpty())
	{
		Actor->SetActorLabel(Label);
	}
	Actor->Modify();
	World->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Actor->GetName());
	Data->SetStringField(TEXT("label"), Actor->GetActorLabel());
	Data->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
	return FUnreelMCPResult::Success(TEXT("Actor spawned"), Data);
}

FUnreelMCPResult FUnreelMCPEditorService::SearchAssets(const TSharedRef<FJsonObject>& Params) const
{
	FString Directory = TEXT("/Game");
	FString ClassFilter;
	FString NameFilter;
	int32 Limit = 100;
	Params->TryGetStringField(TEXT("directory"), Directory);
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);
	double LimitValue = Limit;
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	Limit = static_cast<int32>(LimitValue);
	Limit = FMath::Clamp(Limit, 1, 1000);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*Directory), Assets, true);

	TArray<TSharedPtr<FJsonValue>> AssetValues;
	int32 Total = 0;
	for (const FAssetData& Asset : Assets)
	{
		const FString ClassString = AssetDataClassString(Asset);
		const FString AssetName = Asset.AssetName.ToString();
		if (!ClassFilter.IsEmpty() && !ClassString.Contains(ClassFilter))
		{
			continue;
		}
		if (!NameFilter.IsEmpty() && !AssetName.Contains(NameFilter))
		{
			continue;
		}

		Total++;
		if (AssetValues.Num() >= Limit)
		{
			continue;
		}

		TSharedPtr<FJsonObject> AssetJson = MakeShared<FJsonObject>();
		AssetJson->SetStringField(TEXT("name"), AssetName);
		AssetJson->SetStringField(TEXT("class"), ClassString);
		AssetJson->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
		AssetJson->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
		AssetValues.Add(MakeShared<FJsonValueObject>(AssetJson));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("total"), Total);
	Data->SetArrayField(TEXT("assets"), AssetValues);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Found %d asset(s)"), Total), Data);
}

FUnreelMCPResult FUnreelMCPEditorService::GetAssetInfo(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage(TEXT("/Game/Blueprints/BP_Player")));
	}

	FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPath);
	if (!AssetData.IsValid())
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	Data->SetStringField(TEXT("class"), AssetDataClassString(AssetData));
	Data->SetStringField(TEXT("object_path"), AssetData.GetObjectPathString());
	Data->SetStringField(TEXT("package_name"), AssetData.PackageName.ToString());
	Data->SetStringField(TEXT("package_path"), AssetData.PackagePath.ToString());
	return FUnreelMCPResult::Success(TEXT("Asset info loaded"), Data);
}

FUnreelMCPResult FUnreelMCPEditorService::SaveAsset(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage(TEXT("/Game/Blueprints/BP_Player")));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SaveAsset", "UnreelMCP Save Asset"));
	const bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, false);
	if (!bSaved)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to save asset: %s"), *AssetPath));
	}

	FUnreelMCPResult Result = FUnreelMCPResult::Success(FString::Printf(TEXT("Saved asset: %s"), *AssetPath));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPEditorService::TailLog(const TSharedRef<FJsonObject>& Params) const
{
	int32 MaxChars = 12000;
	double MaxCharsValue = MaxChars;
	Params->TryGetNumberField(TEXT("max_chars"), MaxCharsValue);
	MaxChars = static_cast<int32>(MaxCharsValue);
	MaxChars = FMath::Clamp(MaxChars, 100, 200000);

	const FString ProjectLogPath = FPaths::Combine(FPaths::ProjectLogDir(), FString(FApp::GetProjectName()) + TEXT(".log"));
	FString Contents;
	if (!FFileHelper::LoadFileToString(Contents, *ProjectLogPath))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to read log file: %s"), *ProjectLogPath));
	}

	if (Contents.Len() > MaxChars)
	{
		Contents = Contents.Right(MaxChars);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("log_path"), ProjectLogPath);
	Data->SetStringField(TEXT("text"), Contents);
	return FUnreelMCPResult::Success(TEXT("Log tail loaded"), Data);
}

FUnreelMCPResult FUnreelMCPEditorService::RequestScreenshot(const TSharedRef<FJsonObject>& Params) const
{
	FString Filename;
	if (!Params->TryGetStringField(TEXT("filename"), Filename) || Filename.IsEmpty())
	{
		Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), TEXT("UnreelMCP_Capture.png"));
	}
	else if (FPaths::IsRelative(Filename))
	{
		Filename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Screenshots"), Filename);
	}

	FScreenshotRequest::RequestScreenshot(Filename, false, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("filename"), Filename);
	return FUnreelMCPResult::Success(TEXT("Screenshot requested"), Data);
}
