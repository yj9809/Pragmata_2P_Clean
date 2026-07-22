#include "MCP/Services/UnreelMCPBreadthService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "MCP/Services/UnreelMCPScriptHistory.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

namespace
{
	FUnreelMCPParameter BreadthParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	TArray<FString> SplitClassList(const FString& ClassList)
	{
		TArray<FString> Parts;
		ClassList.ParseIntoArray(Parts, TEXT(","), true);
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
		}
		return Parts;
	}

	bool AssetMatchesClassList(const FAssetData& Asset, const TArray<FString>& ClassFilters)
	{
		if (ClassFilters.Num() == 0)
		{
			return true;
		}
		const FString ClassString = Asset.AssetClassPath.ToString();
		for (const FString& Filter : ClassFilters)
		{
			if (ClassString.Contains(Filter) || Asset.AssetName.ToString().Contains(Filter))
			{
				return true;
			}
		}
		return false;
	}

	FString ScriptLoadHint(const FString& Path)
	{
		if (!Path.StartsWith(TEXT("/Script/")))
		{
			return FString();
		}
		return TEXT(" Project class paths under /Script may require rebuilding the project module, clearing stale editor DLLs, and restarting Unreal Editor before reflection can load the class.");
	}

	FString PythonPathLiteral(FString Path)
	{
		FPaths::NormalizeFilename(Path);
		Path.ReplaceInline(TEXT("'"), TEXT("\\'"));
		return FString::Printf(TEXT("r'%s'"), *Path);
	}

	bool IsAllowedPythonScriptPath(const FString& InPath)
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeFilename(FullPath);
		FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FString SavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		FPaths::NormalizeFilename(ProjectDir);
		FPaths::NormalizeFilename(SavedDir);
		return FPaths::IsUnderDirectory(FullPath, ProjectDir) || FPaths::IsUnderDirectory(FullPath, SavedDir);
	}
}

FString FUnreelMCPBreadthService::GetServiceName() const
{
	return TEXT("breadth");
}

FUnreelMCPOperation FUnreelMCPBreadthService::MakeSearchOperation(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FString>& Keywords, const FString& DefaultClasses) const
{
	FUnreelMCPOperation Op;
	Op.Id = Id;
	Op.Domain = Domain;
	Op.Operation = Operation;
	Op.Description = Description;
	Op.Keywords = Keywords;
	Op.Parameters = {
		BreadthParam(TEXT("directory"), TEXT("string"), TEXT("Package directory to search."), false, TEXT("/Game")),
		BreadthParam(TEXT("name_filter"), TEXT("string"), TEXT("Optional asset name substring.")),
		BreadthParam(TEXT("class_filter"), TEXT("string"), TEXT("Comma-separated class substrings. Defaults to domain classes."), false, DefaultClasses),
		BreadthParam(TEXT("limit"), TEXT("integer"), TEXT("Maximum assets to return."), false, TEXT("100"))
	};
	Op.bReadOnly = true;
	Op.Risk = EUnreelMCPRisk::ReadOnly;
	Op.ExampleJson = FString::Printf(TEXT("{\"operation_id\":\"%s\",\"params\":{\"directory\":\"/Game\",\"limit\":50}}"), *Id);
	return Op;
}

TArray<FUnreelMCPOperation> FUnreelMCPBreadthService::GetOperations() const
{
	TArray<FUnreelMCPOperation> Ops;
	Ops.Add(MakeSearchOperation(TEXT("blueprint.search"), TEXT("blueprint"), TEXT("search"), TEXT("Search Blueprint assets."), { TEXT("blueprint"), TEXT("graph"), TEXT("class") }, TEXT("Blueprint")));
	Ops.Add(MakeSearchOperation(TEXT("material.search"), TEXT("material"), TEXT("search"), TEXT("Search material and material-instance assets."), { TEXT("material"), TEXT("shader"), TEXT("graph") }, TEXT("Material,MaterialInstance")));
	Ops.Add(MakeSearchOperation(TEXT("widget.search"), TEXT("widget"), TEXT("search"), TEXT("Search UMG Widget Blueprint assets."), { TEXT("umg"), TEXT("widget"), TEXT("ui"), TEXT("mvvm") }, TEXT("WidgetBlueprint")));
	Ops.Add(MakeSearchOperation(TEXT("animation.search"), TEXT("animation"), TEXT("search"), TEXT("Search animation assets including sequences, montages, blend spaces, and animation blueprints."), { TEXT("animation"), TEXT("animsequence"), TEXT("montage"), TEXT("animblueprint") }, TEXT("AnimSequence,AnimMontage,BlendSpace,AnimBlueprint")));
	Ops.Add(MakeSearchOperation(TEXT("niagara.search"), TEXT("niagara"), TEXT("search"), TEXT("Search Niagara system, emitter, and related FX assets."), { TEXT("niagara"), TEXT("fx"), TEXT("emitter"), TEXT("particle") }, TEXT("Niagara")));
	Ops.Add(MakeSearchOperation(TEXT("landscape.search"), TEXT("landscape"), TEXT("search"), TEXT("Search landscape, foliage, and foliage type assets."), { TEXT("landscape"), TEXT("foliage"), TEXT("terrain") }, TEXT("Landscape,Foliage")));
	Ops.Add(MakeSearchOperation(TEXT("audio.search"), TEXT("audio"), TEXT("search"), TEXT("Search audio assets including sound waves, cues, mixes, and MetaSound assets."), { TEXT("audio"), TEXT("soundcue"), TEXT("metasound") }, TEXT("Sound,MetaSound")));
	Ops.Add(MakeSearchOperation(TEXT("statetree.search"), TEXT("statetree"), TEXT("search"), TEXT("Search StateTree assets."), { TEXT("statetree"), TEXT("ai"), TEXT("state machine") }, TEXT("StateTree")));
	Ops.Add(MakeSearchOperation(TEXT("input.search"), TEXT("input"), TEXT("search"), TEXT("Search Enhanced Input actions and mapping contexts."), { TEXT("input"), TEXT("inputaction"), TEXT("mappingcontext") }, TEXT("InputAction,InputMappingContext")));
	Ops.Add(MakeSearchOperation(TEXT("data.search"), TEXT("data"), TEXT("search"), TEXT("Search DataTable, CurveTable, StringTable, and DataAsset assets."), { TEXT("datatable"), TEXT("dataasset"), TEXT("curvetable"), TEXT("stringtable") }, TEXT("DataTable,DataAsset,CurveTable,StringTable")));
	Ops.Add(MakeSearchOperation(TEXT("mesh.search"), TEXT("mesh"), TEXT("search"), TEXT("Search static mesh, skeletal mesh, skeleton, and physics assets."), { TEXT("mesh"), TEXT("skeletal"), TEXT("skeleton"), TEXT("uv") }, TEXT("StaticMesh,SkeletalMesh,Skeleton,PhysicsAsset")));
	Ops.Add(MakeSearchOperation(TEXT("map.search"), TEXT("map"), TEXT("search"), TEXT("Search map/world assets."), { TEXT("map"), TEXT("level"), TEXT("world") }, TEXT("World")));

	FUnreelMCPOperation AssetDescribe;
	AssetDescribe.Id = TEXT("reflection.asset_info");
	AssetDescribe.Domain = TEXT("reflection");
	AssetDescribe.Operation = TEXT("asset_info");
	AssetDescribe.Description = TEXT("Describe one asset with class, path, tags, and loaded UObject property names.");
	AssetDescribe.Keywords = { TEXT("reflection"), TEXT("asset"), TEXT("properties"), TEXT("metadata"), TEXT("class path"), TEXT("script class") };
	AssetDescribe.Parameters = { BreadthParam(TEXT("asset_path"), TEXT("string"), TEXT("Asset object path or package path."), true) };
	AssetDescribe.bReadOnly = true;
	AssetDescribe.Risk = EUnreelMCPRisk::ReadOnly;
	AssetDescribe.ExampleJson = TEXT("{\"operation_id\":\"reflection.asset_info\",\"params\":{\"asset_path\":\"/Game/BP_Player\"}}");
	Ops.Add(AssetDescribe);

	FUnreelMCPOperation ClassDescribe;
	ClassDescribe.Id = TEXT("reflection.class_info");
	ClassDescribe.Domain = TEXT("reflection");
	ClassDescribe.Operation = TEXT("class_info");
	ClassDescribe.Description = TEXT("Describe a loaded Unreal class with parent class and reflected property/function names.");
	ClassDescribe.Keywords = { TEXT("reflection"), TEXT("class"), TEXT("uclass"), TEXT("properties"), TEXT("functions") };
	ClassDescribe.Parameters = { BreadthParam(TEXT("class_path"), TEXT("string"), TEXT("Class path such as /Script/Engine.Actor."), true) };
	ClassDescribe.bReadOnly = true;
	ClassDescribe.Risk = EUnreelMCPRisk::ReadOnly;
	ClassDescribe.ExampleJson = TEXT("{\"operation_id\":\"reflection.class_info\",\"params\":{\"class_path\":\"/Script/Engine.Actor\"}}");
	Ops.Add(ClassDescribe);

	FUnreelMCPOperation Settings;
	Settings.Id = TEXT("settings.project_info");
	Settings.Domain = TEXT("settings");
	Settings.Operation = TEXT("project_info");
	Settings.Description = TEXT("Return project, engine, directory, and plugin configuration context.");
	Settings.Keywords = { TEXT("settings"), TEXT("project"), TEXT("engine"), TEXT("config") };
	Settings.bReadOnly = true;
	Settings.Risk = EUnreelMCPRisk::ReadOnly;
	Settings.ExampleJson = TEXT("{\"operation_id\":\"settings.project_info\",\"params\":{}}");
	Ops.Add(Settings);

	FUnreelMCPOperation ConfigScan;
	ConfigScan.Id = TEXT("config.scan");
	ConfigScan.Domain = TEXT("config");
	ConfigScan.Operation = TEXT("scan");
	ConfigScan.Description = TEXT("Read selected project config file names and matching lines for tags, input, rendering, or gameplay settings.");
	ConfigScan.Keywords = { TEXT("config"), TEXT("gameplaytags"), TEXT("settings"), TEXT("ini") };
	ConfigScan.Parameters = {
		BreadthParam(TEXT("pattern"), TEXT("string"), TEXT("Case-insensitive substring to match in config lines."), false, TEXT("GameplayTag")),
		BreadthParam(TEXT("limit"), TEXT("integer"), TEXT("Maximum matching lines."), false, TEXT("100"))
	};
	ConfigScan.bReadOnly = true;
	ConfigScan.Risk = EUnreelMCPRisk::ReadOnly;
	ConfigScan.ExampleJson = TEXT("{\"operation_id\":\"config.scan\",\"params\":{\"pattern\":\"GameplayTag\",\"limit\":50}}");
	Ops.Add(ConfigScan);

	FUnreelMCPOperation GameplayTags;
	GameplayTags.Id = TEXT("gameplaytags.search");
	GameplayTags.Domain = TEXT("gameplaytags");
	GameplayTags.Operation = TEXT("search");
	GameplayTags.Description = TEXT("Search project config for GameplayTags definitions and references.");
	GameplayTags.Keywords = { TEXT("gameplaytags"), TEXT("tags"), TEXT("config"), TEXT("ini") };
	GameplayTags.Parameters = {
		BreadthParam(TEXT("pattern"), TEXT("string"), TEXT("Case-insensitive substring to match in config lines."), false, TEXT("GameplayTag")),
		BreadthParam(TEXT("limit"), TEXT("integer"), TEXT("Maximum matching lines."), false, TEXT("100"))
	};
	GameplayTags.bReadOnly = true;
	GameplayTags.Risk = EUnreelMCPRisk::ReadOnly;
	GameplayTags.ExampleJson = TEXT("{\"operation_id\":\"gameplaytags.search\",\"params\":{\"pattern\":\"GameplayTag\",\"limit\":50}}");
	Ops.Add(GameplayTags);

	FUnreelMCPOperation PythonStatus;
	PythonStatus.Id = TEXT("python.status");
	PythonStatus.Domain = TEXT("python");
	PythonStatus.Operation = TEXT("status");
	PythonStatus.Description = TEXT("Report whether high-risk Python execution is enabled for this editor session.");
	PythonStatus.Keywords = { TEXT("python"), TEXT("script"), TEXT("automation") };
	PythonStatus.bReadOnly = true;
	PythonStatus.Risk = EUnreelMCPRisk::ReadOnly;
	PythonStatus.ExampleJson = TEXT("{\"operation_id\":\"python.status\",\"params\":{}}");
	Ops.Add(PythonStatus);

	FUnreelMCPOperation PythonExec;
	PythonExec.Id = TEXT("python.execute");
	PythonExec.Domain = TEXT("python");
	PythonExec.Operation = TEXT("execute");
	PythonExec.Description = TEXT("Execute a single-line Python command through Unreal's editor console only when UNREEL_MCP_ENABLE_PYTHON=1 is set.");
	PythonExec.Keywords = { TEXT("python"), TEXT("script"), TEXT("automation"), TEXT("high risk") };
	PythonExec.Parameters = { BreadthParam(TEXT("code"), TEXT("string"), TEXT("Single-line Python command to execute."), true) };
	PythonExec.bReadOnly = false;
	PythonExec.bRequiresTransaction = true;
	PythonExec.Risk = EUnreelMCPRisk::High;
	PythonExec.ExampleJson = TEXT("{\"operation_id\":\"python.execute\",\"params\":{\"code\":\"print('hello from Unreal')\"}}");
	Ops.Add(PythonExec);

	FUnreelMCPOperation PythonScript;
	PythonScript.Id = TEXT("python.execute_script");
	PythonScript.Domain = TEXT("python");
	PythonScript.Operation = TEXT("execute_script");
	PythonScript.Description = TEXT("Execute multiline Python from code, code_base64, or a safe script_path when UNREEL_MCP_ENABLE_PYTHON=1 is set. Optional capture_output returns stdout/stderr.");
	PythonScript.Keywords = { TEXT("python"), TEXT("script"), TEXT("multiline"), TEXT("code_base64"), TEXT("script_path"), TEXT("stdout"), TEXT("stderr"), TEXT("automation"), TEXT("high risk") };
	PythonScript.Parameters = {
		BreadthParam(TEXT("code"), TEXT("string"), TEXT("Multiline Python code. Mutually exclusive with code_base64 and script_path.")),
		BreadthParam(TEXT("code_base64"), TEXT("string"), TEXT("Base64-encoded Python code. Mutually exclusive with code and script_path.")),
		BreadthParam(TEXT("script_path"), TEXT("string"), TEXT("Existing .py file under the project directory or Saved directory. Mutually exclusive with code and code_base64.")),
		BreadthParam(TEXT("capture_output"), TEXT("boolean"), TEXT("Capture stdout and stderr into response fields."), false, TEXT("false"))
	};
	PythonScript.bReadOnly = false;
	PythonScript.bRequiresTransaction = true;
	PythonScript.Risk = EUnreelMCPRisk::High;
	PythonScript.ExampleJson = TEXT("{\"operation_id\":\"python.execute_script\",\"params\":{\"code_base64\":\"cHJpbnQoJ2hlbGxvJyk=\",\"capture_output\":true}}");
	Ops.Add(PythonScript);

	return Ops;
}

FUnreelMCPResult FUnreelMCPBreadthService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("reflection.asset_info")) return DescribeAsset(Params);
	if (OperationId == TEXT("reflection.class_info")) return DescribeClass(Params);
	if (OperationId == TEXT("settings.project_info")) return ProjectSettings();
	if (OperationId == TEXT("config.scan")) return ScanConfig(Params);
	if (OperationId == TEXT("gameplaytags.search")) return ScanConfig(Params);
	if (OperationId == TEXT("python.status")) return PythonStatus();
	if (OperationId == TEXT("python.execute")) return ExecutePython(Params);
	if (OperationId == TEXT("python.execute_script")) return ExecutePythonScript(Params);
	if (OperationId == TEXT("map.search")) return ListMaps(Params);

	const TMap<FString, FString> Defaults = {
		{ TEXT("blueprint.search"), TEXT("Blueprint") },
		{ TEXT("material.search"), TEXT("Material,MaterialInstance") },
		{ TEXT("widget.search"), TEXT("WidgetBlueprint") },
		{ TEXT("animation.search"), TEXT("AnimSequence,AnimMontage,BlendSpace,AnimBlueprint") },
		{ TEXT("niagara.search"), TEXT("Niagara") },
		{ TEXT("landscape.search"), TEXT("Landscape,Foliage") },
		{ TEXT("audio.search"), TEXT("Sound,MetaSound") },
		{ TEXT("statetree.search"), TEXT("StateTree") },
		{ TEXT("input.search"), TEXT("InputAction,InputMappingContext") },
		{ TEXT("data.search"), TEXT("DataTable,DataAsset,CurveTable,StringTable") },
		{ TEXT("mesh.search"), TEXT("StaticMesh,SkeletalMesh,Skeleton,PhysicsAsset") }
	};

	if (const FString* DefaultClasses = Defaults.Find(OperationId))
	{
		return SearchByClasses(Params, *DefaultClasses, OperationId);
	}
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown breadth operation: %s"), *OperationId));
}

TArray<FAssetData> FUnreelMCPBreadthService::QueryAssets(const FString& Directory, int32 Limit) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*Directory), Assets, true);
	return Assets;
}

FUnreelMCPResult FUnreelMCPBreadthService::SearchByClasses(const TSharedRef<FJsonObject>& Params, const FString& DefaultClasses, const FString& Label) const
{
	FString Directory = TEXT("/Game");
	FString NameFilter;
	FString ClassFilter = DefaultClasses;
	double LimitValue = 100;
	Params->TryGetStringField(TEXT("directory"), Directory);
	Params->TryGetStringField(TEXT("name_filter"), NameFilter);
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	const int32 Limit = FMath::Clamp(static_cast<int32>(LimitValue), 1, 1000);
	const TArray<FString> ClassFilters = SplitClassList(ClassFilter);

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Total = 0;
	for (const FAssetData& Asset : QueryAssets(Directory, Limit))
	{
		const FString AssetName = Asset.AssetName.ToString();
		if (!NameFilter.IsEmpty() && !AssetName.Contains(NameFilter))
		{
			continue;
		}
		if (!AssetMatchesClassList(Asset, ClassFilters))
		{
			continue;
		}
		Total++;
		if (Results.Num() >= Limit)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), AssetName);
		Json->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
		Json->SetStringField(TEXT("object_path"), Asset.GetObjectPathString());
		Json->SetStringField(TEXT("package_name"), Asset.PackageName.ToString());
		Results.Add(MakeShared<FJsonValueObject>(Json));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("label"), Label);
	Data->SetNumberField(TEXT("total"), Total);
	Data->SetArrayField(TEXT("assets"), Results);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("%s found %d asset(s)"), *Label, Total), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::DescribeAsset(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage(TEXT("/Game/BP_Player")));
	}

	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Object && !AssetPath.Contains(TEXT(".")))
	{
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + AssetName));
	}
	if (!Object)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load asset: %s.%s"), *AssetPath, *ScriptLoadHint(AssetPath)));
	}

	TArray<TSharedPtr<FJsonValue>> Properties;
	for (TFieldIterator<FProperty> It(Object->GetClass()); It && Properties.Num() < 200; ++It)
	{
		TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
		Prop->SetStringField(TEXT("name"), It->GetName());
		Prop->SetStringField(TEXT("type"), It->GetCPPType());
		Properties.Add(MakeShared<FJsonValueObject>(Prop));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Object->GetPathName());
	Data->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
	Data->SetArrayField(TEXT("properties"), Properties);
	return FUnreelMCPResult::Success(TEXT("Asset reflected"), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::DescribeClass(const TSharedRef<FJsonObject>& Params) const
{
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: class_path"));
	}
	UClass* Class = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
	if (!Class)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load class: %s.%s"), *ClassPath, *ScriptLoadHint(ClassPath)));
	}

	TArray<TSharedPtr<FJsonValue>> Properties;
	for (TFieldIterator<FProperty> It(Class); It && Properties.Num() < 300; ++It)
	{
		TSharedPtr<FJsonObject> Prop = MakeShared<FJsonObject>();
		Prop->SetStringField(TEXT("name"), It->GetName());
		Prop->SetStringField(TEXT("type"), It->GetCPPType());
		Properties.Add(MakeShared<FJsonValueObject>(Prop));
	}

	TArray<TSharedPtr<FJsonValue>> Functions;
	for (TFieldIterator<UFunction> It(Class); It && Functions.Num() < 300; ++It)
	{
		Functions.Add(MakeShared<FJsonValueString>(It->GetName()));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("class"), Class->GetPathName());
	Data->SetStringField(TEXT("parent"), Class->GetSuperClass() ? Class->GetSuperClass()->GetPathName() : TEXT(""));
	Data->SetArrayField(TEXT("properties"), Properties);
	Data->SetArrayField(TEXT("functions"), Functions);
	return FUnreelMCPResult::Success(TEXT("Class reflected"), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::ProjectSettings() const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("project"), FApp::GetProjectName());
	Data->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	Data->SetStringField(TEXT("project_dir"), FPaths::ProjectDir());
	Data->SetStringField(TEXT("content_dir"), FPaths::ProjectContentDir());
	Data->SetStringField(TEXT("config_dir"), FPaths::ProjectConfigDir());
	Data->SetStringField(TEXT("saved_dir"), FPaths::ProjectSavedDir());
	return FUnreelMCPResult::Success(TEXT("Project settings loaded"), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::ScanConfig(const TSharedRef<FJsonObject>& Params) const
{
	FString Pattern = TEXT("GameplayTag");
	double LimitValue = 100;
	Params->TryGetStringField(TEXT("pattern"), Pattern);
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	const int32 Limit = FMath::Clamp(static_cast<int32>(LimitValue), 1, 500);

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::ProjectConfigDir(), TEXT("*.ini"), true, false);

	TArray<TSharedPtr<FJsonValue>> Matches;
	for (const FString& File : Files)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *File))
		{
			continue;
		}
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			if (Lines[Index].Contains(Pattern, ESearchCase::IgnoreCase))
			{
				TSharedPtr<FJsonObject> Match = MakeShared<FJsonObject>();
				Match->SetStringField(TEXT("file"), File);
				Match->SetNumberField(TEXT("line"), Index + 1);
				Match->SetStringField(TEXT("text"), Lines[Index]);
				Matches.Add(MakeShared<FJsonValueObject>(Match));
				if (Matches.Num() >= Limit)
				{
					break;
				}
			}
		}
		if (Matches.Num() >= Limit)
		{
			break;
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("pattern"), Pattern);
	Data->SetArrayField(TEXT("matches"), Matches);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Found %d config match(es)"), Matches.Num()), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::PythonStatus() const
{
	TSharedPtr<FJsonObject> Data = UnreelMCPServiceUtils::BuildPythonStatus();
	const bool bEnabled = UnreelMCPServiceUtils::IsPythonExecutionEnabled();
	return FUnreelMCPResult::Success(bEnabled ? TEXT("Python execution enabled") : TEXT("Python execution disabled"), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::ExecutePython(const TSharedRef<FJsonObject>& Params) const
{
	if (!UnreelMCPServiceUtils::IsPythonExecutionEnabled())
	{
		UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("Python execution disabled"));
		return FUnreelMCPResult::Error(TEXT("Python execution is disabled. Set UNREEL_MCP_ENABLE_PYTHON=1 before launching Unreal Editor to enable it."));
	}
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
	{
		UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("Missing required parameter: code"));
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: code"));
	}
	if (Code.Contains(TEXT("\n")) || Code.Contains(TEXT("\r")))
	{
		UnreelMCPScriptHistory::Record(Code, false, TEXT("Rejected multiline command"));
		return FUnreelMCPResult::Error(TEXT("python.execute accepts a single-line command only"));
	}
	if (!GEditor)
	{
		UnreelMCPScriptHistory::Record(Code, false, TEXT("GEditor unavailable"));
		return FUnreelMCPResult::Error(TEXT("GEditor is unavailable"));
	}
	const FString Command = FString::Printf(TEXT("py %s"), *Code);
	const bool bExecuted = GEditor->Exec(nullptr, *Command);
	UnreelMCPScriptHistory::Record(Code, bExecuted, bExecuted ? TEXT("Python command submitted") : TEXT("Unreal did not accept the Python console command"));
	return bExecuted ? FUnreelMCPResult::Success(TEXT("Python command submitted")) : FUnreelMCPResult::Error(TEXT("Unreal did not accept the Python console command"));
}

FUnreelMCPResult FUnreelMCPBreadthService::ExecutePythonScript(const TSharedRef<FJsonObject>& Params) const
{
	if (!UnreelMCPServiceUtils::IsPythonExecutionEnabled())
	{
		UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("Python execution disabled"));
		return FUnreelMCPResult::Error(TEXT("Python execution is disabled. Set UNREEL_MCP_ENABLE_PYTHON=1 before launching Unreal Editor to enable it."));
	}
	if (!GEditor)
	{
		UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("GEditor unavailable"));
		return FUnreelMCPResult::Error(TEXT("GEditor is unavailable"));
	}

	FString Code;
	FString CodeBase64;
	FString ScriptPath;
	const bool bHasCode = Params->TryGetStringField(TEXT("code"), Code) && !Code.IsEmpty();
	const bool bHasCodeBase64 = Params->TryGetStringField(TEXT("code_base64"), CodeBase64) && !CodeBase64.IsEmpty();
	const bool bHasScriptPath = Params->TryGetStringField(TEXT("script_path"), ScriptPath) && !ScriptPath.IsEmpty();
	const int32 InputCount = (bHasCode ? 1 : 0) + (bHasCodeBase64 ? 1 : 0) + (bHasScriptPath ? 1 : 0);
	if (InputCount != 1)
	{
		UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("Invalid python.execute_script input"));
		return FUnreelMCPResult::Error(TEXT("Provide exactly one of code, code_base64, or script_path"));
	}

	FString WorkDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnreelMCP"), TEXT("PythonOutput"));
	IFileManager::Get().MakeDirectory(*WorkDir, true);
	const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FString SourcePath;
	if (bHasScriptPath)
	{
		SourcePath = FPaths::ConvertRelativePathToFull(ScriptPath);
		FPaths::NormalizeFilename(SourcePath);
		if (!SourcePath.EndsWith(TEXT(".py"), ESearchCase::IgnoreCase))
		{
			UnreelMCPScriptHistory::Record(ScriptPath, false, TEXT("Rejected non-.py script_path"));
			return FUnreelMCPResult::Error(TEXT("script_path must point to a .py file"));
		}
		if (!IsAllowedPythonScriptPath(SourcePath))
		{
			UnreelMCPScriptHistory::Record(ScriptPath, false, TEXT("Rejected script_path outside project or Saved directory"));
			return FUnreelMCPResult::Error(TEXT("script_path must be under the project directory or project Saved directory"));
		}
		if (!FPaths::FileExists(SourcePath))
		{
			UnreelMCPScriptHistory::Record(ScriptPath, false, TEXT("script_path not found"));
			return FUnreelMCPResult::Error(FString::Printf(TEXT("script_path not found: %s"), *ScriptPath));
		}
	}
	else
	{
		if (bHasCodeBase64)
		{
			TArray<uint8> Decoded;
			if (!FBase64::Decode(CodeBase64, Decoded))
			{
				UnreelMCPScriptHistory::Record(TEXT(""), false, TEXT("Invalid code_base64"));
				return FUnreelMCPResult::Error(TEXT("code_base64 is not valid Base64"));
			}
			FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Decoded.GetData()), Decoded.Num());
			Code = FString(Converter.Length(), Converter.Get());
		}
		SourcePath = FPaths::Combine(WorkDir, FString::Printf(TEXT("source_%s.py"), *Id));
		if (!FFileHelper::SaveStringToFile(Code, *SourcePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UnreelMCPScriptHistory::Record(Code.Left(2000), false, TEXT("Failed to write source file"));
			return FUnreelMCPResult::Error(TEXT("Failed to write temporary Python source file"));
		}
	}

	bool bCaptureOutput = false;
	Params->TryGetBoolField(TEXT("capture_output"), bCaptureOutput);
	const FString StdoutPath = FPaths::Combine(WorkDir, FString::Printf(TEXT("stdout_%s.txt"), *Id));
	const FString StderrPath = FPaths::Combine(WorkDir, FString::Printf(TEXT("stderr_%s.txt"), *Id));
	const FString WrapperPath = FPaths::Combine(WorkDir, FString::Printf(TEXT("wrapper_%s.py"), *Id));
	const FString Wrapper = FString::Printf(
		TEXT("import contextlib, runpy, sys\n")
		TEXT("source = %s\n")
		TEXT("stdout_path = %s\n")
		TEXT("stderr_path = %s\n")
		TEXT("if %s:\n")
		TEXT("    with open(stdout_path, 'w', encoding='utf-8') as out, open(stderr_path, 'w', encoding='utf-8') as err:\n")
		TEXT("        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):\n")
		TEXT("            runpy.run_path(source, run_name='__main__')\n")
		TEXT("else:\n")
		TEXT("    runpy.run_path(source, run_name='__main__')\n"),
		*PythonPathLiteral(SourcePath),
		*PythonPathLiteral(StdoutPath),
		*PythonPathLiteral(StderrPath),
		bCaptureOutput ? TEXT("True") : TEXT("False"));
	if (!FFileHelper::SaveStringToFile(Wrapper, *WrapperPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UnreelMCPScriptHistory::Record(SourcePath, false, TEXT("Failed to write wrapper file"));
		return FUnreelMCPResult::Error(TEXT("Failed to write temporary Python wrapper file"));
	}

	const FString Command = FString::Printf(TEXT("py exec(open(%s, encoding='utf-8').read())"), *PythonPathLiteral(WrapperPath));
	const bool bExecuted = GEditor->Exec(nullptr, *Command);
	UnreelMCPScriptHistory::Record(FString::Printf(TEXT("script:%s"), *SourcePath), bExecuted, bExecuted ? TEXT("Python script submitted") : TEXT("Unreal did not accept the Python script command"));
	if (!bExecuted)
	{
		return FUnreelMCPResult::Error(TEXT("Unreal did not accept the Python script command"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("source_path"), SourcePath);
	Data->SetStringField(TEXT("wrapper_path"), WrapperPath);
	Data->SetBoolField(TEXT("capture_output"), bCaptureOutput);
	if (bCaptureOutput)
	{
		FString Stdout;
		FString Stderr;
		FFileHelper::LoadFileToString(Stdout, *StdoutPath);
		FFileHelper::LoadFileToString(Stderr, *StderrPath);
		Data->SetStringField(TEXT("stdout"), Stdout);
		Data->SetStringField(TEXT("stderr"), Stderr);
		Data->SetStringField(TEXT("stdout_path"), StdoutPath);
		Data->SetStringField(TEXT("stderr_path"), StderrPath);
	}
	return FUnreelMCPResult::Success(TEXT("Python script submitted"), Data);
}

FUnreelMCPResult FUnreelMCPBreadthService::ListMaps(const TSharedRef<FJsonObject>& Params) const
{
	return SearchByClasses(Params, TEXT("World"), TEXT("map.search"));
}
