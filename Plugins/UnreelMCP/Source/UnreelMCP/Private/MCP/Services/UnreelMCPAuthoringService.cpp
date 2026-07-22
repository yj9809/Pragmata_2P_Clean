#include "MCP/Services/UnreelMCPAuthoringService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "MCP/Services/UnreelMCPTransactionService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Dom/JsonValue.h"
#include "Factories/Factory.h"
#include "Factories/DataTableFactory.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "K2Node_ComponentBoundEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewModelContext.h"
#include "MVVMEditorSubsystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "InstancedFoliage.h"
#include "InstancedFoliageActor.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontendDocument.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScratchPadContainer.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "StaticMeshEditorSubsystem.h"
#include "SkeletalMeshEditorSubsystem.h"
#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialInterface.h"
#include "GameplayTagContainer.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "WidgetBlueprint.h"

namespace
{
	FUnreelMCPParameter AuthorParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FUnreelMCPOperation AuthorOp(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FString>& Keywords, const TArray<FUnreelMCPParameter>& Params, EUnreelMCPRisk Risk, const FString& Example)
	{
		FUnreelMCPOperation Op;
		Op.Id = Id;
		Op.Domain = Domain;
		Op.Operation = Operation;
		Op.Description = Description;
		Op.Keywords = Keywords;
		Op.Parameters = Params;
		Op.bReadOnly = false;
		Op.bRequiresTransaction = true;
		Op.Risk = Risk;
		Op.ExampleJson = Example;
		return Op;
	}

	FString ObjectPathFromPackagePath(const FString& PackagePath)
	{
		return PackagePath.Contains(TEXT(".")) ? PackagePath : PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	}

	void AddChangedAsset(FUnreelMCPResult& Result, UObject* Object)
	{
		if (Object)
		{
			Result.ChangedAssets.Add(Object->GetPathName());
		}
	}

	bool ReadVectorObject(const TSharedPtr<FJsonObject>& Object, FVector& OutVector, const FVector& DefaultValue = FVector::ZeroVector)
	{
		if (!Object.IsValid())
		{
			OutVector = DefaultValue;
			return false;
		}

		double X = DefaultValue.X;
		double Y = DefaultValue.Y;
		double Z = DefaultValue.Z;
		Object->TryGetNumberField(TEXT("x"), X);
		Object->TryGetNumberField(TEXT("y"), Y);
		Object->TryGetNumberField(TEXT("z"), Z);
		OutVector = FVector(X, Y, Z);
		return true;
	}

	bool ReadVectorField(const TSharedRef<FJsonObject>& Params, const FString& FieldName, FVector& OutVector, const FVector& DefaultValue = FVector::ZeroVector)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Params->TryGetObjectField(FieldName, Object) || !Object || !Object->IsValid())
		{
			OutVector = DefaultValue;
			return false;
		}
		return ReadVectorObject(*Object, OutVector, DefaultValue);
	}

	bool ReadRotatorField(const TSharedPtr<FJsonObject>& Object, FRotator& OutRotator, const FRotator& DefaultValue = FRotator::ZeroRotator)
	{
		if (!Object.IsValid())
		{
			OutRotator = DefaultValue;
			return false;
		}

		double Pitch = DefaultValue.Pitch;
		double Yaw = DefaultValue.Yaw;
		double Roll = DefaultValue.Roll;
		Object->TryGetNumberField(TEXT("pitch"), Pitch);
		Object->TryGetNumberField(TEXT("yaw"), Yaw);
		Object->TryGetNumberField(TEXT("roll"), Roll);
		OutRotator = FRotator(Pitch, Yaw, Roll);
		return true;
	}

	FTransform ReadFoliageTransform(const TSharedPtr<FJsonObject>& Object, FString& OutError)
	{
		if (!Object.IsValid())
		{
			OutError = TEXT("Transform entry must be an object");
			return FTransform::Identity;
		}

		const TSharedPtr<FJsonObject>* LocationObject = nullptr;
		if (!Object->TryGetObjectField(TEXT("location"), LocationObject) || !LocationObject || !LocationObject->IsValid())
		{
			OutError = TEXT("Each foliage transform requires location {x,y,z}");
			return FTransform::Identity;
		}

		FVector Location;
		ReadVectorObject(*LocationObject, Location);

		FRotator Rotation = FRotator::ZeroRotator;
		const TSharedPtr<FJsonObject>* RotationObject = nullptr;
		if (Object->TryGetObjectField(TEXT("rotation"), RotationObject) && RotationObject && RotationObject->IsValid())
		{
			ReadRotatorField(*RotationObject, Rotation);
		}

		FVector Scale = FVector::OneVector;
		double UniformScale = 1.0;
		if (Object->TryGetNumberField(TEXT("scale"), UniformScale))
		{
			Scale = FVector(UniformScale);
		}
		else
		{
			const TSharedPtr<FJsonObject>* ScaleObject = nullptr;
			if (Object->TryGetObjectField(TEXT("scale"), ScaleObject) && ScaleObject && ScaleObject->IsValid())
			{
				ReadVectorObject(*ScaleObject, Scale, FVector::OneVector);
			}
		}

		OutError.Reset();
		return FTransform(Rotation, Location, Scale);
	}

	TSharedPtr<FJsonObject> VectorSummary(const FVector& Vector)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("x"), Vector.X);
		Data->SetNumberField(TEXT("y"), Vector.Y);
		Data->SetNumberField(TEXT("z"), Vector.Z);
		return Data;
	}

	TSharedPtr<FJsonObject> RotatorSummary(const FRotator& Rotator)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetNumberField(TEXT("pitch"), Rotator.Pitch);
		Data->SetNumberField(TEXT("yaw"), Rotator.Yaw);
		Data->SetNumberField(TEXT("roll"), Rotator.Roll);
		return Data;
	}

	FString EnumValueName(const UEnum* Enum, int64 Value)
	{
		return Enum ? Enum->GetNameStringByValue(Value) : FString::FromInt(static_cast<int32>(Value));
	}

	template <typename EnumType>
	EnumType ParseEnumValue(const FString& Value, EnumType DefaultValue)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		if (!Enum || Value.IsEmpty())
		{
			return DefaultValue;
		}
		const int64 ByName = Enum->GetValueByNameString(Value);
		if (ByName != INDEX_NONE)
		{
			return static_cast<EnumType>(ByName);
		}
		const FString QualifiedValue = FString::Printf(TEXT("%s::%s"), *Enum->GetName(), *Value);
		const int64 ByQualifiedName = Enum->GetValueByNameString(QualifiedValue);
		return ByQualifiedName != INDEX_NONE ? static_cast<EnumType>(ByQualifiedName) : DefaultValue;
	}

	bool StateMatchesIdentifier(const UStateTreeState* State, const FString& Identifier)
	{
		if (!State || Identifier.IsEmpty())
		{
			return false;
		}
		const FString StateId = State->ID.ToString(EGuidFormats::DigitsWithHyphens);
		return Identifier == StateId
			|| Identifier == State->Name.ToString()
			|| Identifier == State->GetName()
			|| Identifier == State->GetPath()
			|| State->GetPath().EndsWith(TEXT("/") + Identifier);
	}

	UStateTreeState* FindStateTreeStateRecursive(UStateTreeState* State, const FString& Identifier)
	{
		if (StateMatchesIdentifier(State, Identifier))
		{
			return State;
		}
		if (!State)
		{
			return nullptr;
		}
		for (TObjectPtr<UStateTreeState>& Child : State->Children)
		{
			if (UStateTreeState* Found = FindStateTreeStateRecursive(Child.Get(), Identifier))
			{
				return Found;
			}
		}
		return nullptr;
	}

	FStateTreeEditorNode* FindStateTreeNodeInArray(TArray<FStateTreeEditorNode>& Nodes, const FGuid& NodeId)
	{
		for (FStateTreeEditorNode& Node : Nodes)
		{
			if (Node.ID == NodeId || Node.GetNodeID() == NodeId || Node.GetInstanceDataID() == NodeId)
			{
				return &Node;
			}
		}
		return nullptr;
	}

	FStateTreeEditorNode* FindStateTreeNodeRecursive(UStateTreeState* State, const FGuid& NodeId, UObject*& OutOuter)
	{
		if (!State)
		{
			return nullptr;
		}
		if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(State->EnterConditions, NodeId))
		{
			OutOuter = State;
			return Found;
		}
		if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(State->Tasks, NodeId))
		{
			OutOuter = State;
			return Found;
		}
		if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(State->Considerations, NodeId))
		{
			OutOuter = State;
			return Found;
		}
		if (State->SingleTask.ID == NodeId || State->SingleTask.GetNodeID() == NodeId || State->SingleTask.GetInstanceDataID() == NodeId)
		{
			OutOuter = State;
			return &State->SingleTask;
		}
		for (FStateTreeTransition& Transition : State->Transitions)
		{
			if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(Transition.Conditions, NodeId))
			{
				OutOuter = State;
				return Found;
			}
		}
		for (TObjectPtr<UStateTreeState>& Child : State->Children)
		{
			if (FStateTreeEditorNode* Found = FindStateTreeNodeRecursive(Child.Get(), NodeId, OutOuter))
			{
				return Found;
			}
		}
		return nullptr;
	}

	TSharedPtr<FJsonObject> StateTreeNodeSummary(const FStateTreeEditorNode& Node)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("id"), Node.ID.ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("node_id"), Node.GetNodeID().ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("instance_id"), Node.GetInstanceDataID().ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("name"), Node.GetName().ToString());
		Data->SetStringField(TEXT("node_struct"), Node.Node.IsValid() ? Node.Node.GetScriptStruct()->GetPathName() : FString());
		Data->SetStringField(TEXT("instance_struct"), Node.Instance.IsValid() ? Node.Instance.GetScriptStruct()->GetPathName() : FString());
		Data->SetStringField(TEXT("runtime_struct"), Node.ExecutionRuntimeData.IsValid() ? Node.ExecutionRuntimeData.GetScriptStruct()->GetPathName() : FString());
		if (Node.InstanceObject)
		{
			Data->SetStringField(TEXT("instance_object"), Node.InstanceObject->GetPathName());
			Data->SetStringField(TEXT("instance_object_class"), Node.InstanceObject->GetClass()->GetPathName());
		}
		if (Node.ExecutionRuntimeDataObject)
		{
			Data->SetStringField(TEXT("runtime_object"), Node.ExecutionRuntimeDataObject->GetPathName());
			Data->SetStringField(TEXT("runtime_object_class"), Node.ExecutionRuntimeDataObject->GetClass()->GetPathName());
		}
		return Data;
	}

	TArray<TSharedPtr<FJsonValue>> StateTreeNodeArraySummary(const TArray<FStateTreeEditorNode>& Nodes)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FStateTreeEditorNode& Node : Nodes)
		{
			Values.Add(MakeShared<FJsonValueObject>(StateTreeNodeSummary(Node)));
		}
		return Values;
	}

	TSharedPtr<FJsonObject> StateTreeTransitionSummary(const FStateTreeTransition& Transition)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("id"), Transition.ID.ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("trigger"), EnumValueName(StaticEnum<EStateTreeTransitionTrigger>(), static_cast<int64>(Transition.Trigger)));
		Data->SetStringField(TEXT("type"), EnumValueName(StaticEnum<EStateTreeTransitionType>(), static_cast<int64>(Transition.State.LinkType)));
		Data->SetStringField(TEXT("target_state"), Transition.State.Name.ToString());
		Data->SetStringField(TEXT("target_state_id"), Transition.State.ID.ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("required_event"), Transition.RequiredEvent.Tag.ToString());
		Data->SetNumberField(TEXT("condition_count"), Transition.Conditions.Num());
		Data->SetArrayField(TEXT("conditions"), StateTreeNodeArraySummary(Transition.Conditions));
		return Data;
	}

	TSharedPtr<FJsonObject> StateTreeStateSummary(UStateTreeState* State)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		if (!State)
		{
			return Data;
		}
		Data->SetStringField(TEXT("id"), State->ID.ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetStringField(TEXT("name"), State->Name.ToString());
		Data->SetStringField(TEXT("path"), State->GetPath());
		Data->SetStringField(TEXT("type"), EnumValueName(StaticEnum<EStateTreeStateType>(), static_cast<int64>(State->Type)));
		Data->SetStringField(TEXT("tag"), State->Tag.ToString());
		Data->SetNumberField(TEXT("task_count"), State->Tasks.Num());
		Data->SetNumberField(TEXT("transition_count"), State->Transitions.Num());
		Data->SetArrayField(TEXT("tasks"), StateTreeNodeArraySummary(State->Tasks));
		Data->SetArrayField(TEXT("enter_conditions"), StateTreeNodeArraySummary(State->EnterConditions));
		Data->SetArrayField(TEXT("considerations"), StateTreeNodeArraySummary(State->Considerations));
		TArray<TSharedPtr<FJsonValue>> Transitions;
		for (const FStateTreeTransition& Transition : State->Transitions)
		{
			Transitions.Add(MakeShared<FJsonValueObject>(StateTreeTransitionSummary(Transition)));
		}
		Data->SetArrayField(TEXT("transitions"), Transitions);
		TArray<TSharedPtr<FJsonValue>> Children;
		for (TObjectPtr<UStateTreeState>& Child : State->Children)
		{
			Children.Add(MakeShared<FJsonValueObject>(StateTreeStateSummary(Child.Get())));
		}
		Data->SetArrayField(TEXT("children"), Children);
		return Data;
	}

	bool ParseNiagaraScriptUsage(const FString& InUsage, ENiagaraScriptUsage& OutUsage)
	{
		const FString Usage = InUsage.ToLower();
		if (Usage == TEXT("system_spawn") || Usage == TEXT("systemspawn"))
		{
			OutUsage = ENiagaraScriptUsage::SystemSpawnScript;
			return true;
		}
		if (Usage == TEXT("system_update") || Usage == TEXT("systemupdate"))
		{
			OutUsage = ENiagaraScriptUsage::SystemUpdateScript;
			return true;
		}
		if (Usage == TEXT("emitter_spawn") || Usage == TEXT("emitterspawn"))
		{
			OutUsage = ENiagaraScriptUsage::EmitterSpawnScript;
			return true;
		}
		if (Usage == TEXT("emitter_update") || Usage == TEXT("emitterupdate"))
		{
			OutUsage = ENiagaraScriptUsage::EmitterUpdateScript;
			return true;
		}
		if (Usage == TEXT("particle_spawn") || Usage == TEXT("particlespawn"))
		{
			OutUsage = ENiagaraScriptUsage::ParticleSpawnScript;
			return true;
		}
		if (Usage == TEXT("particle_update") || Usage == TEXT("particleupdate"))
		{
			OutUsage = ENiagaraScriptUsage::ParticleUpdateScript;
			return true;
		}
		return false;
	}

	FString NiagaraUsageName(ENiagaraScriptUsage Usage)
	{
		switch (Usage)
		{
		case ENiagaraScriptUsage::SystemSpawnScript:
			return TEXT("system_spawn");
		case ENiagaraScriptUsage::SystemUpdateScript:
			return TEXT("system_update");
		case ENiagaraScriptUsage::EmitterSpawnScript:
			return TEXT("emitter_spawn");
		case ENiagaraScriptUsage::EmitterUpdateScript:
			return TEXT("emitter_update");
		case ENiagaraScriptUsage::ParticleSpawnScript:
			return TEXT("particle_spawn");
		case ENiagaraScriptUsage::ParticleUpdateScript:
			return TEXT("particle_update");
		default:
			return EnumValueName(StaticEnum<ENiagaraScriptUsage>(), static_cast<int64>(Usage));
		}
	}

	bool IsSystemNiagaraUsage(ENiagaraScriptUsage Usage)
	{
		return Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::SystemUpdateScript;
	}

	UEdGraphPin* NiagaraParameterMapPin(TArrayView<UEdGraphPin* const> Pins)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			const UEdGraphSchema_Niagara* Schema = Pin ? Cast<UEdGraphSchema_Niagara>(Pin->GetSchema()) : nullptr;
			if (Schema && Schema->PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UEdGraphPin* NiagaraParameterMapInputPin(UNiagaraNode& Node)
	{
		TArray<UEdGraphPin*> Pins;
		Node.GetInputPins(Pins);
		return NiagaraParameterMapPin(Pins);
	}

	UEdGraphPin* NiagaraParameterMapOutputPin(UNiagaraNode& Node)
	{
		TArray<UEdGraphPin*> Pins;
		Node.GetOutputPins(Pins);
		return NiagaraParameterMapPin(Pins);
	}

	void NiagaraBreakAllLinks(UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return;
		}
		Pin->Modify();
		TArray<UEdGraphPin*> LinkedPins = Pin->LinkedTo;
		for (UEdGraphPin* LinkedPin : LinkedPins)
		{
			if (LinkedPin)
			{
				LinkedPin->Modify();
				Pin->BreakLinkTo(LinkedPin);
			}
		}
	}

	void NiagaraOrderedModules(UNiagaraNodeOutput& OutputNode, TArray<UNiagaraNodeFunctionCall*>& OutModules)
	{
		OutModules.Reset();
		UNiagaraNode* PreviousNode = &OutputNode;
		while (PreviousNode)
		{
			UEdGraphPin* InputPin = NiagaraParameterMapInputPin(*PreviousNode);
			if (!InputPin || InputPin->LinkedTo.Num() != 1)
			{
				break;
			}
			UNiagaraNode* CurrentNode = Cast<UNiagaraNode>(InputPin->LinkedTo[0]->GetOwningNode());
			if (UNiagaraNodeFunctionCall* ModuleNode = Cast<UNiagaraNodeFunctionCall>(CurrentNode))
			{
				OutModules.Insert(ModuleNode, 0);
			}
			PreviousNode = CurrentNode;
		}
	}

	UNiagaraNodeOutput* FindNiagaraOutputNode(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage, FGuid UsageId)
	{
		if (!Graph)
		{
			return nullptr;
		}
		TArray<UNiagaraNodeOutput*> OutputNodes;
		Graph->GetNodesOfClass(OutputNodes);
		for (UNiagaraNodeOutput* OutputNode : OutputNodes)
		{
			if (OutputNode && OutputNode->GetUsage() == Usage && (!UsageId.IsValid() || OutputNode->GetUsageId() == UsageId))
			{
				return OutputNode;
			}
		}
		return nullptr;
	}

	FNiagaraTypeDefinition ParseNiagaraTypeDefinition(const FString& TypeName)
	{
		const FString Type = TypeName.ToLower();
		if (Type == TEXT("float") || Type == TEXT("real"))
		{
			return FNiagaraTypeDefinition::GetFloatDef();
		}
		if (Type == TEXT("double"))
		{
			return FNiagaraTypeHelper::GetDoubleDef();
		}
		if (Type == TEXT("int") || Type == TEXT("integer") || Type == TEXT("int32"))
		{
			return FNiagaraTypeDefinition::GetIntDef();
		}
		if (Type == TEXT("bool") || Type == TEXT("boolean"))
		{
			return FNiagaraTypeDefinition::GetBoolDef();
		}
		if (Type == TEXT("vec2") || Type == TEXT("vector2") || Type == TEXT("vector2d"))
		{
			return FNiagaraTypeDefinition::GetVec2Def();
		}
		if (Type == TEXT("vec3") || Type == TEXT("vector") || Type == TEXT("vector3"))
		{
			return FNiagaraTypeDefinition::GetVec3Def();
		}
		if (Type == TEXT("vec4") || Type == TEXT("vector4"))
		{
			return FNiagaraTypeDefinition::GetVec4Def();
		}
		if (Type == TEXT("color") || Type == TEXT("linearcolor") || Type == TEXT("linear_color"))
		{
			return FNiagaraTypeDefinition::GetColorDef();
		}
		if (Type == TEXT("position"))
		{
			return FNiagaraTypeDefinition::GetPositionDef();
		}
		return FNiagaraTypeDefinition::GetFloatDef();
	}

	struct FResolvedNiagaraStack
	{
		UNiagaraSystem* System = nullptr;
		UNiagaraScript* Script = nullptr;
		UNiagaraNodeOutput* OutputNode = nullptr;
		UNiagaraScriptSource* Source = nullptr;
		FNiagaraEmitterHandle* EmitterHandle = nullptr;
		ENiagaraScriptUsage Usage = ENiagaraScriptUsage::SystemSpawnScript;
		FGuid EmitterId;
		FString EmitterName;
	};

	bool ResolveNiagaraStack(const TSharedRef<FJsonObject>& Params, FResolvedNiagaraStack& OutStack, FString& OutError)
	{
		FString AssetPath;
		if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
		{
			OutError = UnreelMCPServiceUtils::MissingAssetPathMessage();
			return false;
		}
		UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
		if (!Object && !AssetPath.Contains(TEXT(".")))
		{
			Object = StaticLoadObject(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + FPackageName::GetShortName(AssetPath)));
		}
		OutStack.System = Cast<UNiagaraSystem>(Object);
		if (!OutStack.System)
		{
			OutError = TEXT("Failed to load Niagara System asset");
			return false;
		}

		FString UsageText = TEXT("system_spawn");
		Params->TryGetStringField(TEXT("usage"), UsageText);
		if (!ParseNiagaraScriptUsage(UsageText, OutStack.Usage))
		{
			OutError = TEXT("usage must be system_spawn, system_update, emitter_spawn, emitter_update, particle_spawn, or particle_update");
			return false;
		}

		if (IsSystemNiagaraUsage(OutStack.Usage))
		{
			OutStack.Script = OutStack.Usage == ENiagaraScriptUsage::SystemSpawnScript ? OutStack.System->GetSystemSpawnScript() : OutStack.System->GetSystemUpdateScript();
		}
		else
		{
			FString EmitterIdentifier;
			Params->TryGetStringField(TEXT("emitter"), EmitterIdentifier);
			TArray<FNiagaraEmitterHandle>& Handles = OutStack.System->GetEmitterHandles();
			for (FNiagaraEmitterHandle& Handle : Handles)
			{
				const FString HandleId = Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens);
				if (EmitterIdentifier.IsEmpty() && Handles.Num() == 1)
				{
					OutStack.EmitterHandle = &Handle;
					break;
				}
				if (EmitterIdentifier == Handle.GetName().ToString() || EmitterIdentifier == HandleId || EmitterIdentifier == Handle.GetUniqueInstanceName())
				{
					OutStack.EmitterHandle = &Handle;
					break;
				}
			}
			if (!OutStack.EmitterHandle)
			{
				OutError = TEXT("Emitter usage requires emitter when the system does not have exactly one matching emitter");
				return false;
			}
			FVersionedNiagaraEmitterData* EmitterData = OutStack.EmitterHandle->GetEmitterData();
			OutStack.Script = EmitterData ? EmitterData->GetScript(OutStack.Usage, FGuid()) : nullptr;
			OutStack.EmitterId = OutStack.EmitterHandle->GetId();
			OutStack.EmitterName = OutStack.EmitterHandle->GetName().ToString();
		}

		OutStack.Source = OutStack.Script ? Cast<UNiagaraScriptSource>(OutStack.Script->GetLatestSource()) : nullptr;
		UNiagaraGraph* Graph = OutStack.Source ? OutStack.Source->NodeGraph : nullptr;
		OutStack.OutputNode = FindNiagaraOutputNode(Graph, OutStack.Usage, OutStack.Script ? OutStack.Script->GetUsageId() : FGuid());
		if (!OutStack.Script || !OutStack.Source || !Graph || !OutStack.OutputNode)
		{
			OutError = TEXT("Failed to resolve Niagara script graph/output node for the requested stack");
			return false;
		}
		return true;
	}

	UNiagaraNodeFunctionCall* FindNiagaraModule(UNiagaraNodeOutput* OutputNode, const TSharedRef<FJsonObject>& Params, int32& OutIndex)
	{
		OutIndex = INDEX_NONE;
		if (!OutputNode)
		{
			return nullptr;
		}
		TArray<UNiagaraNodeFunctionCall*> Modules;
		NiagaraOrderedModules(*OutputNode, Modules);
		int32 Index = INDEX_NONE;
		if (Params->TryGetNumberField(TEXT("index"), Index) && Modules.IsValidIndex(Index))
		{
			OutIndex = Index;
			return Modules[Index];
		}
		FString ModuleIdentifier;
		Params->TryGetStringField(TEXT("module"), ModuleIdentifier);
		if (ModuleIdentifier.IsEmpty())
		{
			Params->TryGetStringField(TEXT("module_id"), ModuleIdentifier);
		}
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			UNiagaraNodeFunctionCall* Module = Modules[ModuleIndex];
			if (!Module)
			{
				continue;
			}
			const FString Guid = Module->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens);
			if (ModuleIdentifier == Guid
				|| ModuleIdentifier == Module->GetName()
				|| ModuleIdentifier == Module->GetFunctionName()
				|| (Module->FunctionScript && ModuleIdentifier == Module->FunctionScript->GetPathName()))
			{
				OutIndex = ModuleIndex;
				return Module;
			}
		}
		return nullptr;
	}

	void FinalizeNiagaraEdit(const FResolvedNiagaraStack& Stack)
	{
		if (Stack.Source)
		{
			Stack.Source->Modify();
			if (Stack.Source->NodeGraph)
			{
				Stack.Source->NodeGraph->NotifyGraphChanged();
			}
		}
		if (Stack.Script)
		{
			Stack.Script->Modify();
			Stack.Script->MarkPackageDirty();
		}
		if (Stack.System)
		{
			Stack.System->Modify();
			Stack.System->ForceGraphToRecompileOnNextCheck();
			Stack.System->RequestCompile(true);
			Stack.System->MarkPackageDirty();
		}
	}

	struct FNiagaraModuleChainGroup
	{
		UNiagaraNodeFunctionCall* Module = nullptr;
		UNiagaraNode* StartNode = nullptr;
		TArray<UNiagaraNode*> Nodes;
		UEdGraphPin* PreviousOutput = nullptr;
		TArray<UEdGraphPin*> NextInputs;
	};

	bool ResolveNiagaraModuleChainGroup(UNiagaraNodeFunctionCall* Module, FNiagaraModuleChainGroup& OutGroup)
	{
		if (!Module)
		{
			return false;
		}
		OutGroup = FNiagaraModuleChainGroup();
		OutGroup.Module = Module;

		UNiagaraNode* CurrentNode = Module;
		while (CurrentNode)
		{
			OutGroup.Nodes.Insert(CurrentNode, 0);
			UEdGraphPin* CurrentInput = NiagaraParameterMapInputPin(*CurrentNode);
			if (!CurrentInput || CurrentInput->LinkedTo.Num() != 1)
			{
				return false;
			}
			UNiagaraNode* LinkedNode = Cast<UNiagaraNode>(CurrentInput->LinkedTo[0]->GetOwningNode());
			if (!LinkedNode)
			{
				return false;
			}
			if (LinkedNode->IsA<UNiagaraNodeInput>() || LinkedNode->IsA<UNiagaraNodeFunctionCall>())
			{
				OutGroup.PreviousOutput = NiagaraParameterMapOutputPin(*LinkedNode);
				OutGroup.StartNode = CurrentNode;
				break;
			}
			CurrentNode = LinkedNode;
		}
		UEdGraphPin* ModuleOutput = NiagaraParameterMapOutputPin(*Module);
		if (!OutGroup.StartNode || !OutGroup.PreviousOutput || !ModuleOutput)
		{
			return false;
		}
		OutGroup.NextInputs = ModuleOutput->LinkedTo;
		return true;
	}

	bool DisconnectNiagaraModuleGroup(FNiagaraModuleChainGroup& Group)
	{
		if (!Group.Module || !Group.PreviousOutput)
		{
			return false;
		}
		UEdGraphPin* ModuleOutput = NiagaraParameterMapOutputPin(*Group.Module);
		if (!ModuleOutput)
		{
			return false;
		}
		NiagaraBreakAllLinks(Group.PreviousOutput);
		NiagaraBreakAllLinks(ModuleOutput);
		for (UEdGraphPin* NextInput : Group.NextInputs)
		{
			if (NextInput)
			{
				Group.PreviousOutput->MakeLinkTo(NextInput);
			}
		}
		return true;
	}

	bool RemoveNiagaraModuleFromChain(UNiagaraNodeFunctionCall* Module, TArray<UNiagaraNode*>& OutRemovedNodes)
	{
		FNiagaraModuleChainGroup Group;
		if (!ResolveNiagaraModuleChainGroup(Module, Group) || !DisconnectNiagaraModuleGroup(Group))
		{
			return false;
		}
		OutRemovedNodes = Group.Nodes;
		return true;
	}

	bool InsertNiagaraModuleIntoChain(UNiagaraNodeOutput* OutputNode, FNiagaraModuleChainGroup& Group, int32 TargetIndex)
	{
		if (!OutputNode || !Group.Module || !Group.StartNode)
		{
			return false;
		}
		TArray<UNiagaraNodeFunctionCall*> Modules;
		NiagaraOrderedModules(*OutputNode, Modules);
		Modules.Remove(Group.Module);
		const int32 InsertIndex = FMath::Clamp(TargetIndex, 0, Modules.Num());
		UEdGraphPin* PreviousOutput = nullptr;
		UEdGraphPin* NextInput = nullptr;
		if (InsertIndex == 0)
		{
			if (Modules.Num() > 0)
			{
				UEdGraphPin* FirstInput = NiagaraParameterMapInputPin(*Modules[0]);
				PreviousOutput = FirstInput && FirstInput->LinkedTo.Num() == 1 ? FirstInput->LinkedTo[0] : nullptr;
				NextInput = FirstInput;
			}
			else
			{
				NextInput = NiagaraParameterMapInputPin(*OutputNode);
				PreviousOutput = NextInput && NextInput->LinkedTo.Num() == 1 ? NextInput->LinkedTo[0] : nullptr;
			}
		}
		else
		{
			PreviousOutput = NiagaraParameterMapOutputPin(*Modules[InsertIndex - 1]);
			NextInput = InsertIndex < Modules.Num() ? NiagaraParameterMapInputPin(*Modules[InsertIndex]) : NiagaraParameterMapInputPin(*OutputNode);
		}
		UEdGraphPin* ModuleInput = NiagaraParameterMapInputPin(*Group.StartNode);
		UEdGraphPin* ModuleOutput = NiagaraParameterMapOutputPin(*Group.Module);
		if (!PreviousOutput || !NextInput || !ModuleInput || !ModuleOutput)
		{
			return false;
		}
		NiagaraBreakAllLinks(PreviousOutput);
		NiagaraBreakAllLinks(ModuleOutput);
		PreviousOutput->MakeLinkTo(ModuleInput);
		ModuleOutput->MakeLinkTo(NextInput);
		return true;
	}
}

FString FUnreelMCPAuthoringService::GetServiceName() const
{
	return TEXT("authoring");
}

TArray<FUnreelMCPOperation> FUnreelMCPAuthoringService::GetOperations() const
{
	const TArray<FUnreelMCPParameter> AssetCreateParams = {
		AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Package path such as /Game/Folder/MyAsset. Alternative to package_path + asset_name.")),
		AuthorParam(TEXT("package_path"), TEXT("string"), TEXT("Destination package folder, such as /Game/Folder.")),
		AuthorParam(TEXT("asset_name"), TEXT("string"), TEXT("Asset name when package_path is used.")),
		AuthorParam(TEXT("class_path"), TEXT("string"), TEXT("Asset class path, such as /Script/Engine.Material.")),
		AuthorParam(TEXT("factory_class_path"), TEXT("string"), TEXT("Optional factory class path.")),
		AuthorParam(TEXT("factory_properties"), TEXT("object"), TEXT("Optional reflected properties to set on the factory before creation.")),
		AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the created asset."))
	};

	TArray<FUnreelMCPOperation> Ops;
	Ops.Add(AuthorOp(TEXT("asset.create"), TEXT("asset"), TEXT("create"), TEXT("Create an asset using a class and optional UFactory."), { TEXT("asset"), TEXT("create"), TEXT("factory") }, AssetCreateParams, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"asset.create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/M_GenericAsset\",\"class_path\":\"/Script/Engine.Material\"}}")));
	Ops.Add(AuthorOp(TEXT("asset.set_property"), TEXT("asset"), TEXT("set_property"), TEXT("Set one reflected property on an asset using Unreal import text or object path values."), { TEXT("asset"), TEXT("property"), TEXT("reflection") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Asset path."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("JSON value or import-text string."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"asset.set_property\",\"params\":{\"asset_path\":\"/Game/DA_Item\",\"property\":\"DisplayName\",\"value\":\"Sword\"}}")));
	Ops.Add(AuthorOp(TEXT("data.datatable_create"), TEXT("data"), TEXT("datatable_create"), TEXT("Create a DataTable with a row UScriptStruct."), { TEXT("datatable"), TEXT("data"), TEXT("table") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("row_struct_path"), TEXT("string"), TEXT("UScriptStruct path used as the row type."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"data.datatable_create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/DT_Items\",\"row_struct_path\":\"/Script/Engine.DataTableRowHandle\"}}")));
	Ops.Add(AuthorOp(TEXT("dataasset.create"), TEXT("data"), TEXT("dataasset_create"), TEXT("Create a concrete DataAsset-derived asset and optionally set reflected properties. class_path is required because UDataAsset and UPrimaryDataAsset are abstract."), { TEXT("dataasset"), TEXT("data"), TEXT("asset") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("class_path"), TEXT("string"), TEXT("Concrete DataAsset subclass path."), true), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the created asset.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"dataasset.create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/DA_Config\",\"class_path\":\"/Script/YourModule.YourConcreteDataAsset\"}}")));

	Ops.Add(AuthorOp(TEXT("blueprint.create"), TEXT("blueprint"), TEXT("create"), TEXT("Create a Blueprint asset from a parent class."), { TEXT("blueprint"), TEXT("create"), TEXT("class") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("parent_class_path"), TEXT("string"), TEXT("Parent class path."), false, TEXT("/Script/Engine.Actor")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/BP_Test\",\"parent_class_path\":\"/Script/Engine.Actor\"}}")));
	Ops.Add(AuthorOp(TEXT("blueprint.add_variable"), TEXT("blueprint"), TEXT("add_variable"), TEXT("Add a member variable to a Blueprint."), { TEXT("blueprint"), TEXT("variable") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint path."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Variable name."), true), AuthorParam(TEXT("category"), TEXT("string"), TEXT("Pin category such as bool, int, real, string, object."), false, TEXT("bool")), AuthorParam(TEXT("subcategory_object"), TEXT("string"), TEXT("Optional class/struct path for object pins.")), AuthorParam(TEXT("default_value"), TEXT("string"), TEXT("Default value import text.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.add_variable\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"name\":\"bEnabled\",\"category\":\"bool\",\"default_value\":\"true\"}}")));
	Ops.Add(AuthorOp(TEXT("blueprint.compile"), TEXT("blueprint"), TEXT("compile"), TEXT("Compile a Blueprint asset."), { TEXT("blueprint"), TEXT("compile") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.compile\",\"params\":{\"asset_path\":\"/Game/BP_Test\"}}")));

	Ops.Add(AuthorOp(TEXT("material.create"), TEXT("material"), TEXT("create"), TEXT("Create a Material asset."), { TEXT("material"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/M_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("material.instance_create"), TEXT("material"), TEXT("instance_create"), TEXT("Create a Material Instance Constant asset and optionally assign a parent material."), { TEXT("material"), TEXT("instance") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("parent"), TEXT("string"), TEXT("Optional parent material path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.instance_create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/MI_Test\",\"parent\":\"/Game/M_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("material.instance_set_parameter"), TEXT("material"), TEXT("instance_set_parameter"), TEXT("Set scalar, vector, texture, or static-switch parameters on a Material Instance Constant."), { TEXT("material"), TEXT("parameter"), TEXT("instance") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Material instance path."), true), AuthorParam(TEXT("parameter"), TEXT("string"), TEXT("Parameter name."), true), AuthorParam(TEXT("type"), TEXT("string"), TEXT("scalar, vector, texture, or static_switch."), false, TEXT("scalar")), AuthorParam(TEXT("value"), TEXT("any"), TEXT("Parameter value. Vector accepts {r,g,b,a}; texture accepts asset path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.instance_set_parameter\",\"params\":{\"asset_path\":\"/Game/MI_Test\",\"parameter\":\"Roughness\",\"type\":\"scalar\",\"value\":0.5}}")));

	Ops.Add(AuthorOp(TEXT("widget.create"), TEXT("widget"), TEXT("create"), TEXT("Create a Widget Blueprint via UWidgetBlueprintFactory."), { TEXT("widget"), TEXT("umg"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("parent_class_path"), TEXT("string"), TEXT("Parent widget class."), false, TEXT("/Script/UMG.UserWidget")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.create\",\"params\":{\"asset_path\":\"/Game/UnreelMCP_Test/WBP_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.tree.list"), TEXT("widget"), TEXT("tree_list"), TEXT("Inspect a Widget Blueprint's editable UWidgetTree hierarchy, including parent, slot, and child metadata."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("inspect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"widget.tree.list\",\"params\":{\"asset_path\":\"/Game/WBP_Test\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("widget.tree.add_widget"), TEXT("widget"), TEXT("tree_add_widget"), TEXT("Construct a UWidget in a Widget Blueprint tree, optionally set it as root, attach it to a panel/content parent, and apply reflected properties."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("class_path"), TEXT("string"), TEXT("UWidget class path such as /Script/UMG.CanvasPanel or /Script/UMG.TextBlock."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Unique widget name."), true), AuthorParam(TEXT("parent"), TEXT("string"), TEXT("Optional parent widget name. Parent must be a panel/content widget.")), AuthorParam(TEXT("set_root"), TEXT("boolean"), TEXT("Set the new widget as the root widget."), false, TEXT("false")), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the widget.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.tree.add_widget\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"class_path\":\"/Script/UMG.TextBlock\",\"name\":\"TitleText\",\"parent\":\"RootCanvas\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.tree.reparent"), TEXT("widget"), TEXT("tree_reparent"), TEXT("Move an existing widget under another panel/content widget in the same Widget Blueprint tree."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("move"), TEXT("reparent") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name to move."), true), AuthorParam(TEXT("parent"), TEXT("string"), TEXT("New panel/content parent widget name."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.tree.reparent\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\",\"parent\":\"ChildCanvas\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.tree.remove_widget"), TEXT("widget"), TEXT("tree_remove_widget"), TEXT("Remove a widget and its descendants from a Widget Blueprint tree."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name to remove."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.tree.remove_widget\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.tree.set_property"), TEXT("widget"), TEXT("tree_set_property"), TEXT("Set one reflected property on a widget template inside a Widget Blueprint tree."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("property") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("JSON value or import-text string."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.tree.set_property\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\",\"property\":\"Text\",\"value\":\"Hello\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.tree.set_slot_property"), TEXT("widget"), TEXT("tree_set_slot_property"), TEXT("Set one reflected property on a widget's slot object, such as CanvasPanelSlot bAutoSize or LayoutData."), { TEXT("widget"), TEXT("umg"), TEXT("tree"), TEXT("slot"), TEXT("property") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Reflected slot property name."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("JSON value or import-text string."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.tree.set_slot_property\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\",\"property\":\"bAutoSize\",\"value\":true}}")));
	Ops.Add(AuthorOp(TEXT("widget.event.list"), TEXT("widget"), TEXT("event_list"), TEXT("List available multicast delegate events and existing component-bound event handler nodes for widgets in a Widget Blueprint."), { TEXT("widget"), TEXT("umg"), TEXT("event"), TEXT("delegate"), TEXT("list") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Optional widget name filter.")) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"widget.event.list\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"StartButton\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.event.bind"), TEXT("widget"), TEXT("event_bind"), TEXT("Create a Blueprint component-bound event handler node for a widget multicast delegate such as Button.OnClicked."), { TEXT("widget"), TEXT("umg"), TEXT("event"), TEXT("delegate"), TEXT("bind") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("event"), TEXT("string"), TEXT("Multicast delegate property name such as OnClicked."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.event.bind\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"StartButton\",\"event\":\"OnClicked\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.event.remove"), TEXT("widget"), TEXT("event_remove"), TEXT("Remove an existing Blueprint component-bound event handler node for a widget event."), { TEXT("widget"), TEXT("umg"), TEXT("event"), TEXT("delegate"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("event"), TEXT("string"), TEXT("Multicast delegate property name."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.event.remove\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"StartButton\",\"event\":\"OnClicked\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.binding.list"), TEXT("widget"), TEXT("binding_list"), TEXT("List classic UMG property/function bindings stored on a Widget Blueprint."), { TEXT("widget"), TEXT("umg"), TEXT("binding"), TEXT("property"), TEXT("list") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"widget.binding.list\",\"params\":{\"asset_path\":\"/Game/WBP_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.binding.add"), TEXT("widget"), TEXT("binding_add"), TEXT("Add or replace a classic UMG binding from a widget property to a Blueprint source property or function."), { TEXT("widget"), TEXT("umg"), TEXT("binding"), TEXT("property"), TEXT("function") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Bindable widget property, e.g. Text for TextBlock.Text."), true), AuthorParam(TEXT("source_property"), TEXT("string"), TEXT("Blueprint member property to bind from.")), AuthorParam(TEXT("function"), TEXT("string"), TEXT("Blueprint function to bind from. Use either source_property or function.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.binding.add\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\",\"property\":\"Text\",\"source_property\":\"Title\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.binding.remove"), TEXT("widget"), TEXT("binding_remove"), TEXT("Remove a classic UMG binding for a widget property."), { TEXT("widget"), TEXT("umg"), TEXT("binding"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("widget"), TEXT("string"), TEXT("Widget name."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Bound widget property."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.binding.remove\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"widget\":\"TitleText\",\"property\":\"Text\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.mvvm.list"), TEXT("widget"), TEXT("mvvm_list"), TEXT("List MVVM view extension state, viewmodels, and binding count for a Widget Blueprint."), { TEXT("widget"), TEXT("umg"), TEXT("mvvm"), TEXT("viewmodel"), TEXT("list") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("request_view"), TEXT("boolean"), TEXT("Create the MVVM view extension if missing."), false, TEXT("false")) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"widget.mvvm.list\",\"params\":{\"asset_path\":\"/Game/WBP_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.mvvm.add_viewmodel"), TEXT("widget"), TEXT("mvvm_add_viewmodel"), TEXT("Register a ViewModel class on a Widget Blueprint MVVM view."), { TEXT("widget"), TEXT("umg"), TEXT("mvvm"), TEXT("viewmodel"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("class_path"), TEXT("string"), TEXT("ViewModel class implementing NotifyFieldValueChanged."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Optional final ViewModel name.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.mvvm.add_viewmodel\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"class_path\":\"/Game/BP_ScoreViewModel.BP_ScoreViewModel_C\",\"name\":\"ScoreVM\"}}")));
	Ops.Add(AuthorOp(TEXT("widget.mvvm.remove_viewmodel"), TEXT("widget"), TEXT("mvvm_remove_viewmodel"), TEXT("Remove a ViewModel by name from a Widget Blueprint MVVM view."), { TEXT("widget"), TEXT("umg"), TEXT("mvvm"), TEXT("viewmodel"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Widget Blueprint path."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("ViewModel name."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"widget.mvvm.remove_viewmodel\",\"params\":{\"asset_path\":\"/Game/WBP_Test\",\"name\":\"ScoreVM\"}}")));

	Ops.Add(AuthorOp(TEXT("animation.anim_blueprint_create"), TEXT("animation"), TEXT("anim_blueprint_create"), TEXT("Create an Animation Blueprint via UAnimBlueprintFactory. Set target_skeleton when available."), { TEXT("animation"), TEXT("animblueprint"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("target_skeleton"), TEXT("string"), TEXT("Skeleton asset path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.anim_blueprint_create\",\"params\":{\"asset_path\":\"/Game/ABP_Test\",\"target_skeleton\":\"/Game/SK_Mannequin_Skeleton\"}}")));
	Ops.Add(AuthorOp(TEXT("animation.sequence_create"), TEXT("animation"), TEXT("sequence_create"), TEXT("Create an AnimSequence via UAnimSequenceFactory. Raw key authoring is best-effort and not exposed here."), { TEXT("animation"), TEXT("sequence"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("target_skeleton"), TEXT("string"), TEXT("Skeleton asset path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.sequence_create\",\"params\":{\"asset_path\":\"/Game/A_Test\",\"target_skeleton\":\"/Game/SK_Mannequin_Skeleton\"}}")));
	Ops.Add(AuthorOp(TEXT("animation.montage_create"), TEXT("animation"), TEXT("montage_create"), TEXT("Create an AnimMontage via UAnimMontageFactory."), { TEXT("animation"), TEXT("montage"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("target_skeleton"), TEXT("string"), TEXT("Skeleton asset path.")), AuthorParam(TEXT("source_animation"), TEXT("string"), TEXT("Optional AnimSequence path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage_create\",\"params\":{\"asset_path\":\"/Game/AM_Test\",\"target_skeleton\":\"/Game/SK_Mannequin_Skeleton\"}}")));

	Ops.Add(AuthorOp(TEXT("input.action_create"), TEXT("input"), TEXT("action_create"), TEXT("Create an Enhanced Input Action asset when the EnhancedInput plugin is enabled."), { TEXT("input"), TEXT("action"), TEXT("enhanced") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.action_create\",\"params\":{\"asset_path\":\"/Game/IA_Jump\"}}")));
	Ops.Add(AuthorOp(TEXT("input.mapping_context_create"), TEXT("input"), TEXT("mapping_context_create"), TEXT("Create an Enhanced Input Mapping Context asset when the EnhancedInput plugin is enabled."), { TEXT("input"), TEXT("mapping"), TEXT("enhanced") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping_context_create\",\"params\":{\"asset_path\":\"/Game/IMC_Player\"}}")));

	Ops.Add(AuthorOp(TEXT("mesh.static_generate_uv"), TEXT("mesh"), TEXT("static_generate_uv"), TEXT("Generate planar, cylindrical, or box UVs for a StaticMesh using UStaticMeshEditorSubsystem."), { TEXT("mesh"), TEXT("uv"), TEXT("static") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StaticMesh path."), true), AuthorParam(TEXT("mode"), TEXT("string"), TEXT("planar, cylindrical, or box."), false, TEXT("planar")), AuthorParam(TEXT("lod_index"), TEXT("integer"), TEXT("LOD index."), false, TEXT("0")), AuthorParam(TEXT("uv_channel"), TEXT("integer"), TEXT("UV channel index."), false, TEXT("1")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"mesh.static_generate_uv\",\"params\":{\"asset_path\":\"/Game/SM_Test\",\"mode\":\"planar\",\"uv_channel\":1}}")));
	Ops.Add(AuthorOp(TEXT("mesh.skeletal_create_physics_asset"), TEXT("mesh"), TEXT("skeletal_create_physics_asset"), TEXT("Create and assign a PhysicsAsset for a SkeletalMesh using USkeletalMeshEditorSubsystem."), { TEXT("mesh"), TEXT("skeletal"), TEXT("physics") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SkeletalMesh path."), true), AuthorParam(TEXT("set_to_mesh"), TEXT("boolean"), TEXT("Assign the physics asset to the mesh."), false, TEXT("true")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"mesh.skeletal_create_physics_asset\",\"params\":{\"asset_path\":\"/Game/SK_Test\",\"set_to_mesh\":true}}")));

	Ops.Add(AuthorOp(TEXT("foliage.type_create"), TEXT("foliage"), TEXT("type_create"), TEXT("Create a FoliageType_InstancedStaticMesh asset and optionally assign a static mesh."), { TEXT("foliage"), TEXT("landscape"), TEXT("mesh") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("mesh"), TEXT("string"), TEXT("Optional StaticMesh path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"foliage.type_create\",\"params\":{\"asset_path\":\"/Game/FT_Grass\",\"mesh\":\"/Game/SM_Grass\"}}")));
	Ops.Add(AuthorOp(TEXT("foliage.inspect"), TEXT("foliage"), TEXT("inspect"), TEXT("Inspect foliage types and placed instances in the current editor world."), { TEXT("foliage"), TEXT("inspect"), TEXT("instances"), TEXT("world") }, { AuthorParam(TEXT("foliage_type"), TEXT("string"), TEXT("Optional FoliageType asset path filter.")), AuthorParam(TEXT("include_instances"), TEXT("boolean"), TEXT("Include per-instance transforms."), false, TEXT("false")), AuthorParam(TEXT("max_instances"), TEXT("integer"), TEXT("Maximum per-type instances to return when include_instances is true."), false, TEXT("100")) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"foliage.inspect\",\"params\":{\"foliage_type\":\"/Game/FT_Grass\",\"include_instances\":true}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("foliage.add_instance"), TEXT("foliage"), TEXT("add_instance"), TEXT("Place one foliage instance in the current editor world."), { TEXT("foliage"), TEXT("place"), TEXT("instance"), TEXT("world") }, { AuthorParam(TEXT("foliage_type"), TEXT("string"), TEXT("FoliageType asset path."), true), AuthorParam(TEXT("location"), TEXT("object"), TEXT("World location {x,y,z}."), true), AuthorParam(TEXT("rotation"), TEXT("object"), TEXT("Optional rotation {pitch,yaw,roll}.")), AuthorParam(TEXT("scale"), TEXT("number|object"), TEXT("Optional uniform scale number or {x,y,z}.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"foliage.add_instance\",\"params\":{\"foliage_type\":\"/Game/FT_Grass\",\"location\":{\"x\":0,\"y\":0,\"z\":100},\"scale\":1.0}}")));
	Ops.Add(AuthorOp(TEXT("foliage.add_instances"), TEXT("foliage"), TEXT("add_instances"), TEXT("Place multiple foliage instances in the current editor world."), { TEXT("foliage"), TEXT("place"), TEXT("instances"), TEXT("batch") }, { AuthorParam(TEXT("foliage_type"), TEXT("string"), TEXT("FoliageType asset path."), true), AuthorParam(TEXT("transforms"), TEXT("array"), TEXT("Array of {location, rotation, scale} objects."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"foliage.add_instances\",\"params\":{\"foliage_type\":\"/Game/FT_Grass\",\"transforms\":[{\"location\":{\"x\":0,\"y\":0,\"z\":100}}]}}")));
	Ops.Add(AuthorOp(TEXT("foliage.remove_instances"), TEXT("foliage"), TEXT("remove_instances"), TEXT("Remove foliage instances by explicit indices, sphere, box, or all for one FoliageType."), { TEXT("foliage"), TEXT("remove"), TEXT("instances"), TEXT("clear") }, { AuthorParam(TEXT("foliage_type"), TEXT("string"), TEXT("FoliageType asset path."), true), AuthorParam(TEXT("indices"), TEXT("array"), TEXT("Optional explicit zero-based instance indices.")), AuthorParam(TEXT("sphere"), TEXT("object"), TEXT("Optional {center:{x,y,z}, radius:number} selector.")), AuthorParam(TEXT("box"), TEXT("object"), TEXT("Optional {min:{x,y,z}, max:{x,y,z}} selector.")), AuthorParam(TEXT("all"), TEXT("boolean"), TEXT("Remove all instances for this type."), false, TEXT("false")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"foliage.remove_instances\",\"params\":{\"foliage_type\":\"/Game/FT_Grass\",\"indices\":[0]}}")));
	Ops.Add(AuthorOp(TEXT("landscape.layer_info_create"), TEXT("landscape"), TEXT("layer_info_create"), TEXT("Create a LandscapeLayerInfoObject asset."), { TEXT("landscape"), TEXT("layer"), TEXT("terrain") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("layer_name"), TEXT("string"), TEXT("Landscape layer name."), false, TEXT("Layer")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"landscape.layer_info_create\",\"params\":{\"asset_path\":\"/Game/LI_Grass\",\"layer_name\":\"Grass\"}}")));
	Ops.Add(AuthorOp(TEXT("landscape.inspect"), TEXT("landscape"), TEXT("inspect"), TEXT("Inspect Landscape actors, extents, component counts, scale, material, and target layers."), { TEXT("landscape"), TEXT("terrain"), TEXT("inspect") }, { AuthorParam(TEXT("actor"), TEXT("string"), TEXT("Optional actor path, name, or label filter.")) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"landscape.inspect\",\"params\":{}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("landscape.create"), TEXT("landscape"), TEXT("create"), TEXT("Create a flat Landscape actor with configurable component layout, scale, material, and initial height."), { TEXT("landscape"), TEXT("terrain"), TEXT("create") }, { AuthorParam(TEXT("actor_label"), TEXT("string"), TEXT("Optional actor label.")), AuthorParam(TEXT("components_x"), TEXT("integer"), TEXT("Component count on X."), false, TEXT("1")), AuthorParam(TEXT("components_y"), TEXT("integer"), TEXT("Component count on Y."), false, TEXT("1")), AuthorParam(TEXT("sections_per_component"), TEXT("integer"), TEXT("Landscape sections per component."), false, TEXT("1")), AuthorParam(TEXT("quads_per_section"), TEXT("integer"), TEXT("Quads per section, commonly 7, 15, 31, 63, 127, or 255."), false, TEXT("7")), AuthorParam(TEXT("height"), TEXT("number"), TEXT("Initial local height in Unreal units."), false, TEXT("0")), AuthorParam(TEXT("location"), TEXT("object"), TEXT("World location {x,y,z}.")), AuthorParam(TEXT("rotation"), TEXT("object"), TEXT("World rotation {pitch,yaw,roll}.")), AuthorParam(TEXT("scale"), TEXT("object|number"), TEXT("Actor scale; object {x,y,z} or uniform number.")), AuthorParam(TEXT("material"), TEXT("string"), TEXT("Optional Landscape material asset path.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"landscape.create\",\"params\":{\"actor_label\":\"MCP_Landscape\",\"components_x\":1,\"components_y\":1,\"quads_per_section\":7}}")));
	Ops.Add(AuthorOp(TEXT("landscape.height.set_region"), TEXT("landscape"), TEXT("height_set_region"), TEXT("Set a rectangular Landscape height region to a constant height or explicit height array."), { TEXT("landscape"), TEXT("terrain"), TEXT("height"), TEXT("sculpt") }, { AuthorParam(TEXT("actor"), TEXT("string"), TEXT("Landscape actor path, name, or label. Omit when only one Landscape exists.")), AuthorParam(TEXT("min_x"), TEXT("integer"), TEXT("Landscape-space min X."), true), AuthorParam(TEXT("min_y"), TEXT("integer"), TEXT("Landscape-space min Y."), true), AuthorParam(TEXT("max_x"), TEXT("integer"), TEXT("Landscape-space max X."), true), AuthorParam(TEXT("max_y"), TEXT("integer"), TEXT("Landscape-space max Y."), true), AuthorParam(TEXT("height"), TEXT("number"), TEXT("Constant local height in Unreal units.")), AuthorParam(TEXT("heights"), TEXT("array"), TEXT("Optional row-major local height values.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"landscape.height.set_region\",\"params\":{\"actor\":\"MCP_Landscape\",\"min_x\":0,\"min_y\":0,\"max_x\":7,\"max_y\":7,\"height\":32}}")));
	Ops.Add(AuthorOp(TEXT("landscape.layer.paint_region"), TEXT("landscape"), TEXT("layer_paint_region"), TEXT("Paint a rectangular Landscape weight layer with a constant alpha value."), { TEXT("landscape"), TEXT("terrain"), TEXT("paint"), TEXT("layer") }, { AuthorParam(TEXT("actor"), TEXT("string"), TEXT("Landscape actor path, name, or label. Omit when only one Landscape exists.")), AuthorParam(TEXT("layer_info"), TEXT("string"), TEXT("LandscapeLayerInfoObject path."), true), AuthorParam(TEXT("min_x"), TEXT("integer"), TEXT("Landscape-space min X."), true), AuthorParam(TEXT("min_y"), TEXT("integer"), TEXT("Landscape-space min Y."), true), AuthorParam(TEXT("max_x"), TEXT("integer"), TEXT("Landscape-space max X."), true), AuthorParam(TEXT("max_y"), TEXT("integer"), TEXT("Landscape-space max Y."), true), AuthorParam(TEXT("alpha"), TEXT("integer"), TEXT("Layer alpha 0-255."), false, TEXT("255")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"landscape.layer.paint_region\",\"params\":{\"actor\":\"MCP_Landscape\",\"layer_info\":\"/Game/LI_Grass\",\"min_x\":0,\"min_y\":0,\"max_x\":7,\"max_y\":7,\"alpha\":255}}")));

	Ops.Add(AuthorOp(TEXT("audio.soundcue_create"), TEXT("audio"), TEXT("soundcue_create"), TEXT("Create a SoundCue using USoundCueFactoryNew."), { TEXT("audio"), TEXT("soundcue"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue_create\",\"params\":{\"asset_path\":\"/Game/SC_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.inspect"), TEXT("audio"), TEXT("soundcue.inspect"), TEXT("Inspect a SoundCue runtime node graph including FirstNode and child links."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("inspect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"audio.soundcue.inspect\",\"params\":{\"asset_path\":\"/Game/SC_Test\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("audio.soundcue.add_node"), TEXT("audio"), TEXT("soundcue.add_node"), TEXT("Add a concrete USoundNode subclass to a SoundCue."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("node"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("class_path"), TEXT("string"), TEXT("Concrete USoundNode class path such as /Script/Engine.SoundNodeMixer."), true), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the new node.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.add_node\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"class_path\":\"/Script/Engine.SoundNodeMixer\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.connect"), TEXT("audio"), TEXT("soundcue.connect"), TEXT("Connect a child SoundNode into a parent node child pin."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("connect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("parent_index"), TEXT("integer"), TEXT("Parent node index from inspect."), true), AuthorParam(TEXT("child_index"), TEXT("integer"), TEXT("Child node index from inspect."), true), AuthorParam(TEXT("input_index"), TEXT("integer"), TEXT("Parent child input index."), false, TEXT("0")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.connect\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"parent_index\":0,\"child_index\":1,\"input_index\":0}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.disconnect"), TEXT("audio"), TEXT("soundcue.disconnect"), TEXT("Clear one child pin from a SoundCue node."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("disconnect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("parent_index"), TEXT("integer"), TEXT("Parent node index from inspect."), true), AuthorParam(TEXT("input_index"), TEXT("integer"), TEXT("Parent child input index."), false, TEXT("0")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.disconnect\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"parent_index\":0,\"input_index\":0}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.set_output"), TEXT("audio"), TEXT("soundcue.set_output"), TEXT("Set the SoundCue FirstNode by node index."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("output") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("node_index"), TEXT("integer"), TEXT("Node index from inspect."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.set_output\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"node_index\":0}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.set_node_property"), TEXT("audio"), TEXT("soundcue.set_node_property"), TEXT("Set one reflected property on a SoundCue node."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("property") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("node_index"), TEXT("integer"), TEXT("Node index from inspect."), true), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("JSON value or asset path for object properties."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.set_node_property\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"node_index\":0,\"property\":\"PitchMin\",\"value\":0.9}}")));
	Ops.Add(AuthorOp(TEXT("audio.soundcue.remove_node"), TEXT("audio"), TEXT("soundcue.remove_node"), TEXT("Remove a SoundCue node by index and clear references to it."), { TEXT("audio"), TEXT("soundcue"), TEXT("graph"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("SoundCue asset path."), true), AuthorParam(TEXT("node_index"), TEXT("integer"), TEXT("Node index from inspect."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.soundcue.remove_node\",\"params\":{\"asset_path\":\"/Game/SC_Test\",\"node_index\":1}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound_source_create"), TEXT("audio"), TEXT("metasound_source_create"), TEXT("Create a MetaSound Source when the MetaSound editor plugin is available."), { TEXT("audio"), TEXT("metasound"), TEXT("create") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound_source_create\",\"params\":{\"asset_path\":\"/Game/MSS_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.inspect"), TEXT("audio"), TEXT("metasound.inspect"), TEXT("Inspect a MetaSound frontend document including nodes, graph inputs, graph outputs, and edges."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("inspect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"audio.metasound.inspect\",\"params\":{\"asset_path\":\"/Game/MSS_Test\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("audio.metasound.add_node"), TEXT("audio"), TEXT("metasound.add_node"), TEXT("Add a MetaSound native node by frontend class name."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("node"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("class_name"), TEXT("string"), TEXT("MetaSound class string, or provide namespace/name/variant fields.")), AuthorParam(TEXT("namespace"), TEXT("string"), TEXT("MetaSound class namespace, for example UE.")), AuthorParam(TEXT("name"), TEXT("string"), TEXT("MetaSound class name, for example Sine.")), AuthorParam(TEXT("variant"), TEXT("string"), TEXT("MetaSound class variant, for example Audio.")), AuthorParam(TEXT("major_version"), TEXT("integer"), TEXT("Class major version."), false, TEXT("1")), AuthorParam(TEXT("x"), TEXT("number"), TEXT("Editor X position.")), AuthorParam(TEXT("y"), TEXT("number"), TEXT("Editor Y position.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.add_node\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"namespace\":\"UE\",\"name\":\"Sine\",\"variant\":\"Audio\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.add_graph_input"), TEXT("audio"), TEXT("metasound.add_graph_input"), TEXT("Add a MetaSound graph input node with a data type and default literal."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("input"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Graph input name."), true), AuthorParam(TEXT("data_type"), TEXT("string"), TEXT("MetaSound data type such as Float, Bool, Int32, String, Trigger, or Audio."), true), AuthorParam(TEXT("default_value"), TEXT("any"), TEXT("Optional default literal.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.add_graph_input\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"name\":\"Pitch\",\"data_type\":\"Float\",\"default_value\":440.0}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.add_graph_output"), TEXT("audio"), TEXT("metasound.add_graph_output"), TEXT("Add a MetaSound graph output node with a data type and default literal."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("output"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Graph output name."), true), AuthorParam(TEXT("data_type"), TEXT("string"), TEXT("MetaSound data type such as Float, Bool, Int32, String, Trigger, or Audio."), true), AuthorParam(TEXT("default_value"), TEXT("any"), TEXT("Optional default literal.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.add_graph_output\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"name\":\"Signal\",\"data_type\":\"Float\",\"default_value\":0.0}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.connect"), TEXT("audio"), TEXT("metasound.connect"), TEXT("Connect two MetaSound nodes by node GUID and named output/input pins."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("connect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("from_node"), TEXT("string"), TEXT("Source node GUID."), true), AuthorParam(TEXT("from_output"), TEXT("string"), TEXT("Source output pin name."), true), AuthorParam(TEXT("to_node"), TEXT("string"), TEXT("Destination node GUID."), true), AuthorParam(TEXT("to_input"), TEXT("string"), TEXT("Destination input pin name."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.connect\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"from_node\":\"...\",\"from_output\":\"Audio\",\"to_node\":\"...\",\"to_input\":\"In\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.disconnect_input"), TEXT("audio"), TEXT("metasound.disconnect_input"), TEXT("Remove any connection to a MetaSound node input pin."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("disconnect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("node"), TEXT("string"), TEXT("Node GUID."), true), AuthorParam(TEXT("input"), TEXT("string"), TEXT("Input pin name."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.disconnect_input\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"node\":\"...\",\"input\":\"In\"}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.set_node_input_default"), TEXT("audio"), TEXT("metasound.set_node_input_default"), TEXT("Set a MetaSound node input default literal."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("default"), TEXT("property") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("node"), TEXT("string"), TEXT("Node GUID."), true), AuthorParam(TEXT("input"), TEXT("string"), TEXT("Input pin name."), true), AuthorParam(TEXT("data_type"), TEXT("string"), TEXT("MetaSound data type."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("Default literal value."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.set_node_input_default\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"node\":\"...\",\"input\":\"Frequency\",\"data_type\":\"Float\",\"value\":220.0}}")));
	Ops.Add(AuthorOp(TEXT("audio.metasound.remove_node"), TEXT("audio"), TEXT("metasound.remove_node"), TEXT("Remove a MetaSound node and its connected edges by node GUID."), { TEXT("audio"), TEXT("metasound"), TEXT("graph"), TEXT("node"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("MetaSound asset path."), true), AuthorParam(TEXT("node"), TEXT("string"), TEXT("Node GUID."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"audio.metasound.remove_node\",\"params\":{\"asset_path\":\"/Game/MSS_Test\",\"node\":\"...\"}}")));

	Ops.Add(AuthorOp(TEXT("niagara.system_create"), TEXT("niagara"), TEXT("system_create"), TEXT("Create a Niagara System asset when NiagaraEditor is available."), { TEXT("niagara"), TEXT("system"), TEXT("fx") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"niagara.system_create\",\"params\":{\"asset_path\":\"/Game/NS_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.emitter_create"), TEXT("niagara"), TEXT("emitter_create"), TEXT("Create a Niagara Emitter asset when NiagaraEditor is available."), { TEXT("niagara"), TEXT("emitter"), TEXT("fx") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"niagara.emitter_create\",\"params\":{\"asset_path\":\"/Game/NE_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.inspect"), TEXT("niagara"), TEXT("stack.inspect"), TEXT("Inspect a Niagara system, emitter, or particle stack and its module order."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("inspect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("system_spawn, system_update, emitter_spawn, emitter_update, particle_spawn, or particle_update."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter name, unique name, or GUID for emitter/particle usages.")) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"niagara.stack.inspect\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"usage\":\"system_spawn\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("niagara.stack.add_module"), TEXT("niagara"), TEXT("stack.add_module"), TEXT("Add a Niagara module script to a stack by script asset path."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("module_script"), TEXT("string"), TEXT("Niagara Script module asset path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Insertion index; omit to append.")), AuthorParam(TEXT("name"), TEXT("string"), TEXT("Suggested module name.")), AuthorParam(TEXT("version_guid"), TEXT("string"), TEXT("Optional module script version GUID.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.add_module\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"module_script\":\"/Niagara/Modules/...\",\"usage\":\"system_spawn\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.add_parameter_module"), TEXT("niagara"), TEXT("stack.add_parameter_module"), TEXT("Add a Set Parameters module that writes one Niagara variable with a default value."), { TEXT("niagara"), TEXT("module"), TEXT("parameter"), TEXT("stack"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("parameter"), TEXT("string"), TEXT("Parameter name such as System.MCPValue, Emitter.MCPValue, or Particles.MCPValue."), true), AuthorParam(TEXT("type"), TEXT("string"), TEXT("float, double, int, bool, vec2, vec3, vec4, color, or position."), false, TEXT("float")), AuthorParam(TEXT("default_value"), TEXT("string"), TEXT("Niagara default-value import string."), false, TEXT("0.0")), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Insertion index; omit to append.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.add_parameter_module\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"parameter\":\"System.MCPValue\",\"type\":\"float\",\"default_value\":\"1.0\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.remove_module"), TEXT("niagara"), TEXT("stack.remove_module"), TEXT("Remove a Niagara stack module by index, node GUID, module name, or script path."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Module index from inspect.")), AuthorParam(TEXT("module"), TEXT("string"), TEXT("Module node GUID, name, or script path.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.remove_module\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"index\":0}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.move_module"), TEXT("niagara"), TEXT("stack.move_module"), TEXT("Move a Niagara stack module to a new index."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("move"), TEXT("reorder") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Source module index from inspect.")), AuthorParam(TEXT("module"), TEXT("string"), TEXT("Module node GUID, name, or script path.")), AuthorParam(TEXT("target_index"), TEXT("integer"), TEXT("Destination module index."), true) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.move_module\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"index\":1,\"target_index\":0}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.set_module_enabled"), TEXT("niagara"), TEXT("stack.set_module_enabled"), TEXT("Enable or disable a Niagara stack module."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("enable"), TEXT("disable") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Module index from inspect.")), AuthorParam(TEXT("module"), TEXT("string"), TEXT("Module node GUID, name, or script path.")), AuthorParam(TEXT("enabled"), TEXT("boolean"), TEXT("Desired enabled state."), true) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.set_module_enabled\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"index\":0,\"enabled\":false}}")));
	Ops.Add(AuthorOp(TEXT("niagara.stack.set_input_default"), TEXT("niagara"), TEXT("stack.set_input_default"), TEXT("Set a module input override pin default value."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("input"), TEXT("default") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("usage"), TEXT("string"), TEXT("Target usage."), false, TEXT("system_spawn")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Emitter identifier for emitter/particle usages.")), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Module index from inspect.")), AuthorParam(TEXT("module"), TEXT("string"), TEXT("Module node GUID, name, or script path.")), AuthorParam(TEXT("input"), TEXT("string"), TEXT("Module input name without the Module namespace."), true), AuthorParam(TEXT("type"), TEXT("string"), TEXT("Niagara input type."), false, TEXT("float")), AuthorParam(TEXT("default_value"), TEXT("string"), TEXT("Niagara default-value import string."), true) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.stack.set_input_default\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"index\":0,\"input\":\"MCPValue\",\"type\":\"float\",\"default_value\":\"2.0\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.scratchpad.inspect"), TEXT("niagara"), TEXT("scratchpad.inspect"), TEXT("Inspect Niagara System and emitter scratch pad scripts."), { TEXT("niagara"), TEXT("scratchpad"), TEXT("inspect") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"niagara.scratchpad.inspect\",\"params\":{\"asset_path\":\"/Game/NS_Test\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("niagara.scratchpad.add_script"), TEXT("niagara"), TEXT("scratchpad.add_script"), TEXT("Duplicate an existing Niagara Script into a system or emitter scratch pad script list."), { TEXT("niagara"), TEXT("scratchpad"), TEXT("script"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("source_script"), TEXT("string"), TEXT("Existing Niagara Script asset or scratch script path to duplicate."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("New scratch script object name.")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Optional emitter identifier; omit for system scratch pad.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.scratchpad.add_script\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"source_script\":\"/Niagara/Modules/...\",\"name\":\"MCP_Scratch\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.scratchpad.remove_script"), TEXT("niagara"), TEXT("scratchpad.remove_script"), TEXT("Remove a Niagara scratch pad script by index, name, or object path."), { TEXT("niagara"), TEXT("scratchpad"), TEXT("script"), TEXT("remove") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Niagara System path."), true), AuthorParam(TEXT("index"), TEXT("integer"), TEXT("Scratch script index.")), AuthorParam(TEXT("script"), TEXT("string"), TEXT("Scratch script object path or name.")), AuthorParam(TEXT("emitter"), TEXT("string"), TEXT("Optional emitter identifier; omit for system scratch pad.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.scratchpad.remove_script\",\"params\":{\"asset_path\":\"/Game/NS_Test\",\"script\":\"MCP_Scratch\"}}")));
	Ops.Add(AuthorOp(TEXT("niagara.module_stack_edit"), TEXT("niagara"), TEXT("module_stack_edit"), TEXT("Compatibility dispatcher for Niagara stack operations. Prefer niagara.stack.* and niagara.scratchpad.*."), { TEXT("niagara"), TEXT("module"), TEXT("stack"), TEXT("compatibility") }, { AuthorParam(TEXT("action"), TEXT("string"), TEXT("inspect, add_parameter_module, add_module, remove_module, move_module, set_module_enabled, set_input_default, scratchpad_inspect, scratchpad_add_script, or scratchpad_remove_script."), true) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"niagara.module_stack_edit\",\"params\":{\"action\":\"inspect\",\"asset_path\":\"/Game/NS_Test\"}}")));

	Ops.Add(AuthorOp(TEXT("statetree.create"), TEXT("statetree"), TEXT("create"), TEXT("Create a StateTree asset via UStateTreeFactory when the StateTree editor plugin is available."), { TEXT("statetree"), TEXT("create"), TEXT("ai") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination package path."), true), AuthorParam(TEXT("schema_class_path"), TEXT("string"), TEXT("Optional schema class path.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"statetree.create\",\"params\":{\"asset_path\":\"/Game/ST_Test\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.inspect"), TEXT("statetree"), TEXT("inspect"), TEXT("Inspect StateTree editor data, states, global tasks, evaluators, tasks, and transitions."), { TEXT("statetree"), TEXT("inspect"), TEXT("state machine") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true) }, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"statetree.inspect\",\"params\":{\"asset_path\":\"/Game/ST_Test\"}}")));
	Ops.Last().bReadOnly = true;
	Ops.Last().bRequiresTransaction = false;
	Ops.Add(AuthorOp(TEXT("statetree.add_state"), TEXT("statetree"), TEXT("add_state"), TEXT("Add a top-level StateTree state or a child state under an existing state."), { TEXT("statetree"), TEXT("state"), TEXT("add") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true), AuthorParam(TEXT("name"), TEXT("string"), TEXT("State name."), true), AuthorParam(TEXT("parent"), TEXT("string"), TEXT("Optional parent state name, path, or GUID.")), AuthorParam(TEXT("type"), TEXT("string"), TEXT("State, Group, Linked, LinkedAsset, or Subtree."), false, TEXT("State")), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected state properties.")) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"statetree.add_state\",\"params\":{\"asset_path\":\"/Game/ST_Test\",\"name\":\"Idle\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.add_task"), TEXT("statetree"), TEXT("add_task"), TEXT("Add a StateTree task node by public UScriptStruct path, then optionally set node and instance properties."), { TEXT("statetree"), TEXT("task"), TEXT("node") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true), AuthorParam(TEXT("state"), TEXT("string"), TEXT("Target state name, path, or GUID. Required unless global_task is true.")), AuthorParam(TEXT("struct_path"), TEXT("string"), TEXT("Task UScriptStruct path, such as /Script/StateTreeModule.StateTreeRunParallelStateTreeTask."), true), AuthorParam(TEXT("global_task"), TEXT("boolean"), TEXT("Add as a global task instead of a state task."), false, TEXT("false")), AuthorParam(TEXT("node_properties"), TEXT("object"), TEXT("Optional reflected properties for the task node struct.")), AuthorParam(TEXT("instance_properties"), TEXT("object"), TEXT("Optional reflected properties for task instance data.")), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Shortcut applied to instance data when present, otherwise the node struct.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"statetree.add_task\",\"params\":{\"asset_path\":\"/Game/ST_Test\",\"state\":\"Idle\",\"struct_path\":\"/Script/StateTreeModule.StateTreeRunParallelStateTreeTask\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.add_evaluator"), TEXT("statetree"), TEXT("add_evaluator"), TEXT("Add a global StateTree evaluator node by public UScriptStruct path, then optionally set node and instance properties."), { TEXT("statetree"), TEXT("evaluator"), TEXT("node") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true), AuthorParam(TEXT("struct_path"), TEXT("string"), TEXT("Evaluator UScriptStruct path."), true), AuthorParam(TEXT("node_properties"), TEXT("object"), TEXT("Optional reflected properties for the evaluator node struct.")), AuthorParam(TEXT("instance_properties"), TEXT("object"), TEXT("Optional reflected properties for evaluator instance data.")), AuthorParam(TEXT("properties"), TEXT("object"), TEXT("Shortcut applied to instance data when present, otherwise the node struct.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"statetree.add_evaluator\",\"params\":{\"asset_path\":\"/Game/ST_Test\",\"struct_path\":\"/Script/YourModule.YourEvaluator\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.add_transition"), TEXT("statetree"), TEXT("add_transition"), TEXT("Add a transition from one StateTree state to another state or completion outcome."), { TEXT("statetree"), TEXT("transition"), TEXT("state") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true), AuthorParam(TEXT("from_state"), TEXT("string"), TEXT("Source state name, path, or GUID."), true), AuthorParam(TEXT("to_state"), TEXT("string"), TEXT("Target state name, path, or GUID when type is GotoState.")), AuthorParam(TEXT("trigger"), TEXT("string"), TEXT("OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, or OnEvent."), false, TEXT("OnStateCompleted")), AuthorParam(TEXT("type"), TEXT("string"), TEXT("GotoState, Succeeded, Failed, Parent, NextState, NextSelectableState, NextParent, or None."), false, TEXT("GotoState")), AuthorParam(TEXT("event_tag"), TEXT("string"), TEXT("Gameplay tag required when trigger is OnEvent.")) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"statetree.add_transition\",\"params\":{\"asset_path\":\"/Game/ST_Test\",\"from_state\":\"Idle\",\"to_state\":\"Run\",\"trigger\":\"OnStateCompleted\",\"type\":\"GotoState\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.set_node_property"), TEXT("statetree"), TEXT("set_node_property"), TEXT("Set one reflected property on a StateTree node template, instance data, runtime data, or UObject instance."), { TEXT("statetree"), TEXT("node"), TEXT("property") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true), AuthorParam(TEXT("node_id"), TEXT("string"), TEXT("Node GUID from statetree.inspect."), true), AuthorParam(TEXT("target"), TEXT("string"), TEXT("node, instance, or runtime."), false, TEXT("instance")), AuthorParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), AuthorParam(TEXT("value"), TEXT("any"), TEXT("JSON value or Unreal import text."), true) }, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"statetree.set_node_property\",\"params\":{\"asset_path\":\"/Game/ST_Test\",\"node_id\":\"...\",\"target\":\"node\",\"property\":\"Name\",\"value\":\"TaskName\"}}")));
	Ops.Add(AuthorOp(TEXT("statetree.compile"), TEXT("statetree"), TEXT("compile"), TEXT("Compile a StateTree after editor-data edits and report readiness."), { TEXT("statetree"), TEXT("compile"), TEXT("validate") }, { AuthorParam(TEXT("asset_path"), TEXT("string"), TEXT("StateTree asset path."), true) }, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"statetree.compile\",\"params\":{\"asset_path\":\"/Game/ST_Test\"}}")));
	return Ops;
}

FUnreelMCPResult FUnreelMCPAuthoringService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("asset.create")) return CreateAssetOperation(Params);
	if (OperationId == TEXT("asset.set_property")) return SetAssetProperty(Params);
	if (OperationId == TEXT("data.datatable_create")) return CreateDataTable(Params);
	if (OperationId == TEXT("dataasset.create")) return CreateAssetOperation(Params);
	if (OperationId == TEXT("blueprint.create")) return CreateBlueprint(Params);
	if (OperationId == TEXT("blueprint.add_variable")) return AddBlueprintVariable(Params);
	if (OperationId == TEXT("blueprint.compile")) return CompileBlueprint(Params);
	if (OperationId == TEXT("material.create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.Material"));
	if (OperationId == TEXT("material.instance_create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.MaterialInstanceConstant"), TEXT("/Script/UnrealEd.MaterialInstanceConstantFactoryNew"));
	if (OperationId == TEXT("material.instance_set_parameter")) return SetMaterialInstanceParameter(Params);
	if (OperationId == TEXT("widget.create")) return CreateAssetOperation(Params, TEXT("/Script/UMGEditor.WidgetBlueprint"), TEXT("/Script/UMGEditor.WidgetBlueprintFactory"));
	if (OperationId == TEXT("widget.tree.list")) return ListWidgetTree(Params);
	if (OperationId == TEXT("widget.tree.add_widget")) return AddWidgetTreeWidget(Params);
	if (OperationId == TEXT("widget.tree.reparent")) return ReparentWidgetTreeWidget(Params);
	if (OperationId == TEXT("widget.tree.remove_widget")) return RemoveWidgetTreeWidget(Params);
	if (OperationId == TEXT("widget.tree.set_property")) return SetWidgetTreeWidgetProperty(Params, false);
	if (OperationId == TEXT("widget.tree.set_slot_property")) return SetWidgetTreeWidgetProperty(Params, true);
	if (OperationId == TEXT("widget.event.list")) return ListWidgetEvents(Params);
	if (OperationId == TEXT("widget.event.bind")) return BindWidgetEvent(Params);
	if (OperationId == TEXT("widget.event.remove")) return RemoveWidgetEventBinding(Params);
	if (OperationId == TEXT("widget.binding.list")) return ListWidgetBindings(Params);
	if (OperationId == TEXT("widget.binding.add")) return AddWidgetBinding(Params);
	if (OperationId == TEXT("widget.binding.remove")) return RemoveWidgetBinding(Params);
	if (OperationId == TEXT("widget.mvvm.list")) return ListWidgetMVVM(Params);
	if (OperationId == TEXT("widget.mvvm.add_viewmodel")) return AddWidgetMVVMViewModel(Params);
	if (OperationId == TEXT("widget.mvvm.remove_viewmodel")) return RemoveWidgetMVVMViewModel(Params);
	if (OperationId == TEXT("animation.anim_blueprint_create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.AnimBlueprint"), TEXT("/Script/UnrealEd.AnimBlueprintFactory"));
	if (OperationId == TEXT("animation.sequence_create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.AnimSequence"), TEXT("/Script/UnrealEd.AnimSequenceFactory"));
	if (OperationId == TEXT("animation.montage_create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.AnimMontage"), TEXT("/Script/UnrealEd.AnimMontageFactory"));
	if (OperationId == TEXT("input.action_create")) return CreateAssetOperation(Params, TEXT("/Script/EnhancedInput.InputAction"));
	if (OperationId == TEXT("input.mapping_context_create")) return CreateAssetOperation(Params, TEXT("/Script/EnhancedInput.InputMappingContext"));
	if (OperationId == TEXT("mesh.static_generate_uv")) return GenerateStaticMeshUV(Params);
	if (OperationId == TEXT("mesh.skeletal_create_physics_asset")) return CreateSkeletalPhysicsAsset(Params);
	if (OperationId == TEXT("foliage.type_create")) return CreateAssetOperation(Params, TEXT("/Script/Foliage.FoliageType_InstancedStaticMesh"));
	if (OperationId == TEXT("foliage.inspect")) return InspectFoliage(Params);
	if (OperationId == TEXT("foliage.add_instance")) return AddFoliageInstance(Params);
	if (OperationId == TEXT("foliage.add_instances")) return AddFoliageInstances(Params);
	if (OperationId == TEXT("foliage.remove_instances")) return RemoveFoliageInstances(Params);
	if (OperationId == TEXT("landscape.layer_info_create")) return CreateLandscapeLayerInfo(Params);
	if (OperationId == TEXT("landscape.inspect")) return InspectLandscape(Params);
	if (OperationId == TEXT("landscape.create")) return CreateLandscape(Params);
	if (OperationId == TEXT("landscape.height.set_region")) return SetLandscapeHeightRegion(Params);
	if (OperationId == TEXT("landscape.layer.paint_region")) return PaintLandscapeLayerRegion(Params);
	if (OperationId == TEXT("audio.soundcue_create")) return CreateAssetOperation(Params, TEXT("/Script/Engine.SoundCue"), TEXT("/Script/AudioEditor.SoundCueFactoryNew"));
	if (OperationId == TEXT("audio.soundcue.inspect")) return InspectSoundCue(Params);
	if (OperationId == TEXT("audio.soundcue.add_node")) return AddSoundCueNode(Params);
	if (OperationId == TEXT("audio.soundcue.connect")) return ConnectSoundCueNode(Params);
	if (OperationId == TEXT("audio.soundcue.disconnect")) return DisconnectSoundCueNode(Params);
	if (OperationId == TEXT("audio.soundcue.set_output")) return SetSoundCueOutput(Params);
	if (OperationId == TEXT("audio.soundcue.set_node_property")) return SetSoundCueNodeProperty(Params);
	if (OperationId == TEXT("audio.soundcue.remove_node")) return RemoveSoundCueNode(Params);
	if (OperationId == TEXT("audio.metasound_source_create")) return CreateMetaSoundSource(Params);
	if (OperationId == TEXT("audio.metasound.inspect")) return InspectMetaSound(Params);
	if (OperationId == TEXT("audio.metasound.add_node")) return AddMetaSoundNode(Params);
	if (OperationId == TEXT("audio.metasound.add_graph_input")) return AddMetaSoundGraphInput(Params);
	if (OperationId == TEXT("audio.metasound.add_graph_output")) return AddMetaSoundGraphOutput(Params);
	if (OperationId == TEXT("audio.metasound.connect")) return ConnectMetaSoundNodes(Params);
	if (OperationId == TEXT("audio.metasound.disconnect_input")) return DisconnectMetaSoundInput(Params);
	if (OperationId == TEXT("audio.metasound.set_node_input_default")) return SetMetaSoundNodeInputDefault(Params);
	if (OperationId == TEXT("audio.metasound.remove_node")) return RemoveMetaSoundNode(Params);
	if (OperationId == TEXT("niagara.system_create")) return CreateAssetOperation(Params, TEXT("/Script/Niagara.NiagaraSystem"), TEXT("/Script/NiagaraEditor.NiagaraSystemFactoryNew"));
	if (OperationId == TEXT("niagara.emitter_create")) return CreateAssetOperation(Params, TEXT("/Script/Niagara.NiagaraEmitter"), TEXT("/Script/NiagaraEditor.NiagaraEmitterFactoryNew"));
	if (OperationId == TEXT("niagara.stack.inspect")) return InspectNiagaraStack(Params);
	if (OperationId == TEXT("niagara.stack.add_module")) return AddNiagaraScriptModule(Params);
	if (OperationId == TEXT("niagara.stack.add_parameter_module")) return AddNiagaraParameterModule(Params);
	if (OperationId == TEXT("niagara.stack.remove_module")) return RemoveNiagaraModule(Params);
	if (OperationId == TEXT("niagara.stack.move_module")) return MoveNiagaraModule(Params);
	if (OperationId == TEXT("niagara.stack.set_module_enabled")) return SetNiagaraModuleEnabled(Params);
	if (OperationId == TEXT("niagara.stack.set_input_default")) return SetNiagaraModuleInputDefault(Params);
	if (OperationId == TEXT("niagara.scratchpad.inspect")) return InspectNiagaraScratchPad(Params);
	if (OperationId == TEXT("niagara.scratchpad.add_script")) return AddNiagaraScratchPadScript(Params);
	if (OperationId == TEXT("niagara.scratchpad.remove_script")) return RemoveNiagaraScratchPadScript(Params);
	if (OperationId == TEXT("niagara.module_stack_edit"))
	{
		FString Action;
		Params->TryGetStringField(TEXT("action"), Action);
		if (Action == TEXT("inspect")) return InspectNiagaraStack(Params);
		if (Action == TEXT("add_parameter_module")) return AddNiagaraParameterModule(Params);
		if (Action == TEXT("add_module")) return AddNiagaraScriptModule(Params);
		if (Action == TEXT("remove_module")) return RemoveNiagaraModule(Params);
		if (Action == TEXT("move_module")) return MoveNiagaraModule(Params);
		if (Action == TEXT("set_module_enabled")) return SetNiagaraModuleEnabled(Params);
		if (Action == TEXT("set_input_default")) return SetNiagaraModuleInputDefault(Params);
		if (Action == TEXT("scratchpad_inspect")) return InspectNiagaraScratchPad(Params);
		if (Action == TEXT("scratchpad_add_script")) return AddNiagaraScratchPadScript(Params);
		if (Action == TEXT("scratchpad_remove_script")) return RemoveNiagaraScratchPadScript(Params);
		return FUnreelMCPResult::Error(TEXT("Unknown niagara.module_stack_edit action"));
	}
	if (OperationId == TEXT("statetree.create")) return CreateAssetOperation(Params, TEXT("/Script/StateTreeModule.StateTree"), TEXT("/Script/StateTreeEditorModule.StateTreeFactory"));
	if (OperationId == TEXT("statetree.inspect")) return InspectStateTree(Params);
	if (OperationId == TEXT("statetree.add_state")) return AddStateTreeState(Params);
	if (OperationId == TEXT("statetree.add_task")) return AddStateTreeTask(Params);
	if (OperationId == TEXT("statetree.add_evaluator")) return AddStateTreeEvaluator(Params);
	if (OperationId == TEXT("statetree.add_transition")) return AddStateTreeTransition(Params);
	if (OperationId == TEXT("statetree.set_node_property")) return SetStateTreeNodeProperty(Params);
	if (OperationId == TEXT("statetree.compile")) return CompileStateTree(Params);
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown authoring operation: %s"), *OperationId));
}

bool FUnreelMCPAuthoringService::ResolveAssetPath(const TSharedRef<FJsonObject>& Params, FString& OutPackagePath, FString& OutAssetName, FString& OutError) const
{
	FString AssetPath;
	Params->TryGetStringField(TEXT("asset_path"), AssetPath);
	Params->TryGetStringField(TEXT("package_path"), OutPackagePath);
	Params->TryGetStringField(TEXT("asset_name"), OutAssetName);

	if (!AssetPath.IsEmpty())
	{
		if (AssetPath.Contains(TEXT(".")))
		{
			AssetPath.Split(TEXT("."), &AssetPath, nullptr);
		}
		OutPackagePath = FPackageName::GetLongPackagePath(AssetPath);
		OutAssetName = FPackageName::GetShortName(AssetPath);
	}

	if (OutPackagePath.IsEmpty() || OutAssetName.IsEmpty())
	{
		OutError = TEXT("Provide asset_path or both package_path and asset_name");
		return false;
	}
	if (!OutPackagePath.StartsWith(TEXT("/Game")))
	{
		OutError = TEXT("Only /Game package paths are allowed for authoring operations");
		return false;
	}
	return true;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateAssetOperation(const TSharedRef<FJsonObject>& Params, const FString& DefaultClassPath, const FString& DefaultFactoryClassPath) const
{
	FString PackagePath;
	FString AssetName;
	FString Error;
	if (!ResolveAssetPath(Params, PackagePath, AssetName, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}

	FString ClassPath = DefaultClassPath;
	FString FactoryClassPath = DefaultFactoryClassPath;
	Params->TryGetStringField(TEXT("class_path"), ClassPath);
	Params->TryGetStringField(TEXT("factory_class_path"), FactoryClassPath);
	if (ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required class_path"));
	}

	UClass* AssetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
	if (!AssetClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("asset class"), ClassPath));
	}
	if (AssetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Cannot create abstract asset class: %s"), *ClassPath));
	}

	UFactory* Factory = nullptr;
	if (!FactoryClassPath.IsEmpty())
	{
		UClass* FactoryClass = StaticLoadClass(UFactory::StaticClass(), nullptr, *FactoryClassPath);
		if (!FactoryClass)
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("factory class"), FactoryClassPath));
		}
		Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		TSharedPtr<FJsonObject> AliasFactoryProperties = MakeShared<FJsonObject>();
		FString AliasValue;
		if (Params->TryGetStringField(TEXT("parent"), AliasValue) && !AliasValue.IsEmpty())
		{
			AliasFactoryProperties->SetStringField(TEXT("InitialParent"), AliasValue);
		}
		if (Params->TryGetStringField(TEXT("target_skeleton"), AliasValue) && !AliasValue.IsEmpty())
		{
			AliasFactoryProperties->SetStringField(TEXT("TargetSkeleton"), AliasValue);
		}
		if (Params->TryGetStringField(TEXT("source_animation"), AliasValue) && !AliasValue.IsEmpty())
		{
			AliasFactoryProperties->SetStringField(TEXT("SourceAnimation"), AliasValue);
		}
		if (Params->TryGetStringField(TEXT("parent_class_path"), AliasValue) && !AliasValue.IsEmpty())
		{
			AliasFactoryProperties->SetStringField(TEXT("ParentClass"), AliasValue);
		}
		if (Params->TryGetStringField(TEXT("schema_class_path"), AliasValue) && !AliasValue.IsEmpty())
		{
			AliasFactoryProperties->SetStringField(TEXT("StateTreeSchemaClass"), AliasValue);
		}
		else if (FactoryClassPath == TEXT("/Script/StateTreeEditorModule.StateTreeFactory"))
		{
			AliasFactoryProperties->SetStringField(TEXT("StateTreeSchemaClass"), TEXT("/Script/GameplayStateTreeModule.StateTreeComponentSchema"));
		}
		if (AliasFactoryProperties->Values.Num() > 0 && !ApplyJsonProperties(Factory, AliasFactoryProperties, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to configure factory aliases: %s"), *Error));
		}
		const TSharedPtr<FJsonObject>* FactoryProperties = nullptr;
		if (Params->TryGetObjectField(TEXT("factory_properties"), FactoryProperties) && FactoryProperties && FactoryProperties->IsValid())
		{
			if (!ApplyJsonProperties(Factory, *FactoryProperties, Error))
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to configure factory: %s"), *Error));
			}
		}
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Create %s"), *AssetName)));
	UObject* Asset = nullptr;
	if (Factory)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Asset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, Factory);
	}
	else
	{
		UPackage* Package = CreatePackage(*(PackagePath / AssetName));
		if (!Package)
		{
			return FUnreelMCPResult::Error(TEXT("Failed to create package"));
		}
		Asset = NewObject<UObject>(Package, AssetClass, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
		if (Asset)
		{
			FAssetRegistryModule::AssetCreated(Asset);
			Package->MarkPackageDirty();
		}
	}

	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Asset creation returned null"));
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		if (!ApplyJsonProperties(Asset, *Properties, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Asset created but property assignment failed: %s"), *Error));
		}
	}
	FString MeshPath;
	if (Params->TryGetStringField(TEXT("mesh"), MeshPath) && !MeshPath.IsEmpty())
	{
		TSharedPtr<FJsonValue> MeshValue = MakeShared<FJsonValueString>(MeshPath);
		if (!ApplyJsonProperty(Asset, TEXT("Mesh"), MeshValue, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Asset created but mesh assignment failed: %s"), *Error));
		}
	}

	Asset->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Asset created"), ObjectSummary(Asset));
	AddChangedAsset(Result, Asset);
	return Result;
}

UObject* FUnreelMCPAuthoringService::LoadAssetObject(const FString& AssetPath) const
{
	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Object && !AssetPath.Contains(TEXT(".")))
	{
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPathFromPackagePath(AssetPath));
	}
	return Object;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetAssetProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: property"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: value"));
	}
	UObject* Object = LoadAssetObject(AssetPath);
	if (!Object)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("asset"), AssetPath));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Set Property %s"), *PropertyName)));
	Object->Modify();
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Object, PropertyName);
	FString Error;
	if (!ApplyJsonProperty(Object, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Object, PropertyName);
	Object->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Property set"), ObjectSummary(Object));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Object->GetPathName(), PropertyName, BeforeValue, AfterValue));
	AddChangedAsset(Result, Object);
	return Result;
}

bool FUnreelMCPAuthoringService::ApplyJsonProperties(UObject* Target, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const
{
	if (!Target || !Properties.IsValid())
	{
		return true;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
	{
		if (!ApplyJsonProperty(Target, Pair.Key, Pair.Value, OutError))
		{
			return false;
		}
	}
	return true;
}

bool FUnreelMCPAuthoringService::ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	if (!Target)
	{
		OutError = TEXT("Target is null");
		return false;
	}
	FProperty* Property = Target->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property not found on %s: %s"), *Target->GetClass()->GetName(), *PropertyName);
		return false;
	}

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		FString ObjectPath;
		if (!Value->TryGetString(ObjectPath))
		{
			OutError = FString::Printf(TEXT("Object property requires an asset/class path string: %s"), *PropertyName);
			return false;
		}
		UObject* Loaded = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *ObjectPath);
		if (!Loaded && ObjectProperty->PropertyClass->IsChildOf(UClass::StaticClass()))
		{
			Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *ObjectPath);
		}
		if (!Loaded)
		{
			OutError = FString::Printf(TEXT("Failed to load object for %s: %s.%s"), *PropertyName, *ObjectPath, *UnreelMCPServiceUtils::ScriptLoadHint(ObjectPath));
			return false;
		}
		ObjectProperty->SetObjectPropertyValue(ValuePtr, Loaded);
		return true;
	}

	const FString ImportText = UnreelMCPServiceUtils::CoerceImportText(Property, Value, JsonValueToImportText(Value));
	if (Property->ImportText_Direct(*ImportText, ValuePtr, Target, PPF_None) == nullptr)
	{
		OutError = FString::Printf(TEXT("Failed to import value for %s from '%s'"), *PropertyName, *ImportText);
		return false;
	}
	return true;
}

bool FUnreelMCPAuthoringService::ApplyJsonStructProperties(const UStruct* Struct, void* StructMemory, UObject* Owner, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const
{
	if (!Struct || !StructMemory || !Properties.IsValid())
	{
		return true;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
	{
		if (!ApplyJsonStructProperty(Struct, StructMemory, Owner, Pair.Key, Pair.Value, OutError))
		{
			return false;
		}
	}
	return true;
}

bool FUnreelMCPAuthoringService::ApplyJsonStructProperty(const UStruct* Struct, void* StructMemory, UObject* Owner, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	if (!Struct || !StructMemory)
	{
		OutError = TEXT("Target struct is null");
		return false;
	}
	FProperty* Property = Struct->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property not found on %s: %s"), *Struct->GetName(), *PropertyName);
		return false;
	}
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructMemory);
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		FString ObjectPath;
		if (!Value->TryGetString(ObjectPath))
		{
			OutError = FString::Printf(TEXT("Object property requires an asset/class path string: %s"), *PropertyName);
			return false;
		}
		UObject* Loaded = StaticLoadObject(ObjectProperty->PropertyClass, nullptr, *ObjectPath);
		if (!Loaded && ObjectProperty->PropertyClass->IsChildOf(UClass::StaticClass()))
		{
			Loaded = StaticLoadClass(UObject::StaticClass(), nullptr, *ObjectPath);
		}
		if (!Loaded)
		{
			OutError = FString::Printf(TEXT("Failed to load object for %s: %s.%s"), *PropertyName, *ObjectPath, *UnreelMCPServiceUtils::ScriptLoadHint(ObjectPath));
			return false;
		}
		ObjectProperty->SetObjectPropertyValue(ValuePtr, Loaded);
		return true;
	}
	const FString ImportText = UnreelMCPServiceUtils::CoerceImportText(Property, Value, JsonValueToImportText(Value));
	if (Property->ImportText_Direct(*ImportText, ValuePtr, Owner, PPF_None) == nullptr)
	{
		OutError = FString::Printf(TEXT("Failed to import value for %s from '%s'"), *PropertyName, *ImportText);
		return false;
	}
	return true;
}

FString FUnreelMCPAuthoringService::JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const
{
	if (!Value.IsValid())
	{
		return FString();
	}
	FString StringValue;
	if (Value->TryGetString(StringValue))
	{
		return StringValue;
	}
	bool BoolValue = false;
	if (Value->TryGetBool(BoolValue))
	{
		return BoolValue ? TEXT("true") : TEXT("false");
	}
	double NumberValue = 0.0;
	if (Value->TryGetNumber(NumberValue))
	{
		return FString::SanitizeFloat(NumberValue);
	}
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Value, TEXT(""), Writer);
	return JsonString;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::ObjectSummary(UObject* Object) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (Object)
	{
		Data->SetStringField(TEXT("path"), Object->GetPathName());
		Data->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
		Data->SetStringField(TEXT("package"), Object->GetPackage()->GetName());
	}
	return Data;
}

UStateTree* FUnreelMCPAuthoringService::LoadStateTree(const FString& AssetPath, FString& OutError) const
{
	UStateTree* StateTree = Cast<UStateTree>(LoadAssetObject(AssetPath));
	if (!StateTree)
	{
		OutError = FString::Printf(TEXT("Failed to load StateTree: %s"), *AssetPath);
	}
	return StateTree;
}

UStateTreeEditorData* FUnreelMCPAuthoringService::LoadStateTreeEditorData(UStateTree* StateTree, FString& OutError) const
{
	UStateTreeEditorData* EditorData = StateTree ? Cast<UStateTreeEditorData>(StateTree->EditorData) : nullptr;
	if (!EditorData)
	{
		OutError = TEXT("StateTree editor data is unavailable");
	}
	return EditorData;
}

UStateTreeState* FUnreelMCPAuthoringService::FindStateTreeState(UStateTreeEditorData* EditorData, const FString& Identifier) const
{
	if (!EditorData)
	{
		return nullptr;
	}
	for (TObjectPtr<UStateTreeState>& SubTree : EditorData->SubTrees)
	{
		if (UStateTreeState* Found = FindStateTreeStateRecursive(SubTree.Get(), Identifier))
		{
			return Found;
		}
	}
	return nullptr;
}

FStateTreeEditorNode* FUnreelMCPAuthoringService::FindStateTreeNode(UStateTreeEditorData* EditorData, const FString& NodeId, UObject*& OutOuter) const
{
	OutOuter = EditorData;
	FGuid ParsedId;
	if (!EditorData || !FGuid::Parse(NodeId, ParsedId))
	{
		return nullptr;
	}
	if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(EditorData->Evaluators, ParsedId))
	{
		return Found;
	}
	if (FStateTreeEditorNode* Found = FindStateTreeNodeInArray(EditorData->GlobalTasks, ParsedId))
	{
		return Found;
	}
	for (TObjectPtr<UStateTreeState>& SubTree : EditorData->SubTrees)
	{
		if (FStateTreeEditorNode* Found = FindStateTreeNodeRecursive(SubTree.Get(), ParsedId, OutOuter))
		{
			return Found;
		}
	}
	return nullptr;
}

bool FUnreelMCPAuthoringService::ApplyStateTreeNodeProperties(FStateTreeEditorNode& Node, UObject* Owner, const TSharedRef<FJsonObject>& Params, FString& OutError) const
{
	const TSharedPtr<FJsonObject>* NodeProperties = nullptr;
	if (Params->TryGetObjectField(TEXT("node_properties"), NodeProperties) && NodeProperties && NodeProperties->IsValid())
	{
		if (!ApplyJsonStructProperties(Node.Node.GetScriptStruct(), Node.Node.GetMutableMemory(), Owner, *NodeProperties, OutError))
		{
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* InstanceProperties = nullptr;
	if (Params->TryGetObjectField(TEXT("instance_properties"), InstanceProperties) && InstanceProperties && InstanceProperties->IsValid())
	{
		if (Node.InstanceObject)
		{
			if (!ApplyJsonProperties(Node.InstanceObject, *InstanceProperties, OutError))
			{
				return false;
			}
		}
		else if (!ApplyJsonStructProperties(Node.Instance.GetScriptStruct(), Node.Instance.GetMutableMemory(), Owner, *InstanceProperties, OutError))
		{
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* RuntimeProperties = nullptr;
	if (Params->TryGetObjectField(TEXT("runtime_properties"), RuntimeProperties) && RuntimeProperties && RuntimeProperties->IsValid())
	{
		if (Node.ExecutionRuntimeDataObject)
		{
			if (!ApplyJsonProperties(Node.ExecutionRuntimeDataObject, *RuntimeProperties, OutError))
			{
				return false;
			}
		}
		else if (!ApplyJsonStructProperties(Node.ExecutionRuntimeData.GetScriptStruct(), Node.ExecutionRuntimeData.GetMutableMemory(), Owner, *RuntimeProperties, OutError))
		{
			return false;
		}
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		if (Node.InstanceObject)
		{
			return ApplyJsonProperties(Node.InstanceObject, *Properties, OutError);
		}
		if (Node.Instance.IsValid())
		{
			return ApplyJsonStructProperties(Node.Instance.GetScriptStruct(), Node.Instance.GetMutableMemory(), Owner, *Properties, OutError);
		}
		return ApplyJsonStructProperties(Node.Node.GetScriptStruct(), Node.Node.GetMutableMemory(), Owner, *Properties, OutError);
	}
	return true;
}

void FUnreelMCPAuthoringService::FinalizeStateTreeEdit(UStateTree* StateTree, UStateTreeEditorData* EditorData) const
{
	if (!StateTree || !EditorData)
	{
		return;
	}
	EditorData->ReparentStates();
	EditorData->FixDuplicateIDs();
	EditorData->UpdateBindings();
	EditorData->RemoveInvalidBindings();
	StateTree->MarkAsModified(true);
	StateTree->CompileIfChanged();
	StateTree->MarkPackageDirty();
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::StateTreeSummary(UStateTree* StateTree, UStateTreeEditorData* EditorData) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!StateTree || !EditorData)
	{
		return Data;
	}
	Data->SetStringField(TEXT("asset"), StateTree->GetPathName());
	Data->SetBoolField(TEXT("ready_to_run"), StateTree->IsReadyToRun());
	Data->SetBoolField(TEXT("editor_data_dirty"), StateTree->IsEditorDataDirty());
	Data->SetStringField(TEXT("schema"), EditorData->Schema ? EditorData->Schema->GetClass()->GetPathName() : FString());
	Data->SetNumberField(TEXT("subtree_count"), EditorData->SubTrees.Num());
	Data->SetNumberField(TEXT("global_task_count"), EditorData->GlobalTasks.Num());
	Data->SetNumberField(TEXT("evaluator_count"), EditorData->Evaluators.Num());
	Data->SetArrayField(TEXT("global_tasks"), StateTreeNodeArraySummary(EditorData->GlobalTasks));
	Data->SetArrayField(TEXT("evaluators"), StateTreeNodeArraySummary(EditorData->Evaluators));
	TArray<TSharedPtr<FJsonValue>> SubTrees;
	for (TObjectPtr<UStateTreeState>& SubTree : EditorData->SubTrees)
	{
		SubTrees.Add(MakeShared<FJsonValueObject>(StateTreeStateSummary(SubTree.Get())));
	}
	Data->SetArrayField(TEXT("states"), SubTrees);
	return Data;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectStateTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	return FUnreelMCPResult::Success(TEXT("StateTree inspected"), StateTreeSummary(StateTree, EditorData));
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddStateTreeState(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or name"));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add StateTree State")));
	StateTree->Modify();
	EditorData->Modify();
	FString TypeString;
	Params->TryGetStringField(TEXT("type"), TypeString);
	const EStateTreeStateType StateType = ParseEnumValue(TypeString, EStateTreeStateType::State);
	FString ParentIdentifier;
	UStateTreeState* NewState = nullptr;
	if (Params->TryGetStringField(TEXT("parent"), ParentIdentifier) && !ParentIdentifier.IsEmpty())
	{
		UStateTreeState* ParentState = FindStateTreeState(EditorData, ParentIdentifier);
		if (!ParentState)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("StateTree parent state not found: %s"), *ParentIdentifier));
		}
		ParentState->Modify();
		NewState = &ParentState->AddChildState(FName(*Name), StateType);
	}
	else
	{
		NewState = &EditorData->AddSubTree(FName(*Name), StateType);
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		NewState->Modify();
		if (!ApplyJsonProperties(NewState, *Properties, Error))
		{
			return FUnreelMCPResult::Error(Error);
		}
	}
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree state added"), StateTreeSummary(StateTree, EditorData));
	Result.Data->SetObjectField(TEXT("state"), StateTreeStateSummary(NewState));
	AddChangedAsset(Result, StateTree);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddStateTreeTask(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString StructPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("struct_path"), StructPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or struct_path"));
	}
	UScriptStruct* TaskStruct = Cast<UScriptStruct>(StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *StructPath));
	if (!TaskStruct || !TaskStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Struct is not a StateTree task: %s.%s"), *StructPath, *UnreelMCPServiceUtils::ScriptLoadHint(StructPath)));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add StateTree Task")));
	StateTree->Modify();
	EditorData->Modify();
	bool bGlobalTask = false;
	Params->TryGetBoolField(TEXT("global_task"), bGlobalTask);
	UObject* Owner = EditorData;
	FStateTreeEditorNode* NewNode = nullptr;
	if (bGlobalTask)
	{
		NewNode = &EditorData->GlobalTasks.AddDefaulted_GetRef();
		NewNode->InitializeAs(EditorData, TaskStruct);
	}
	else
	{
		FString StateIdentifier;
		if (!Params->TryGetStringField(TEXT("state"), StateIdentifier) || StateIdentifier.IsEmpty())
		{
			return FUnreelMCPResult::Error(TEXT("Missing state for StateTree task"));
		}
		UStateTreeState* State = FindStateTreeState(EditorData, StateIdentifier);
		if (!State)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("StateTree state not found: %s"), *StateIdentifier));
		}
		State->Modify();
		Owner = State;
		NewNode = &State->Tasks.AddDefaulted_GetRef();
		NewNode->InitializeAs(State, TaskStruct);
	}
	if (!ApplyStateTreeNodeProperties(*NewNode, Owner, Params, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree task added"), StateTreeSummary(StateTree, EditorData));
	Result.Data->SetObjectField(TEXT("node"), StateTreeNodeSummary(*NewNode));
	AddChangedAsset(Result, StateTree);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddStateTreeEvaluator(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString StructPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("struct_path"), StructPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or struct_path"));
	}
	UScriptStruct* EvaluatorStruct = Cast<UScriptStruct>(StaticLoadObject(UScriptStruct::StaticClass(), nullptr, *StructPath));
	if (!EvaluatorStruct || !EvaluatorStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Struct is not a StateTree evaluator: %s.%s"), *StructPath, *UnreelMCPServiceUtils::ScriptLoadHint(StructPath)));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add StateTree Evaluator")));
	StateTree->Modify();
	EditorData->Modify();
	FStateTreeEditorNode& NewNode = EditorData->Evaluators.AddDefaulted_GetRef();
	NewNode.InitializeAs(EditorData, EvaluatorStruct);
	if (!ApplyStateTreeNodeProperties(NewNode, EditorData, Params, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree evaluator added"), StateTreeSummary(StateTree, EditorData));
	Result.Data->SetObjectField(TEXT("node"), StateTreeNodeSummary(NewNode));
	AddChangedAsset(Result, StateTree);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddStateTreeTransition(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString FromStateIdentifier;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("from_state"), FromStateIdentifier))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or from_state"));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UStateTreeState* FromState = FindStateTreeState(EditorData, FromStateIdentifier);
	if (!FromState)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Source state not found: %s"), *FromStateIdentifier));
	}
	FString TriggerString;
	Params->TryGetStringField(TEXT("trigger"), TriggerString);
	const EStateTreeTransitionTrigger Trigger = ParseEnumValue(TriggerString, EStateTreeTransitionTrigger::OnStateCompleted);
	FString TypeString;
	Params->TryGetStringField(TEXT("type"), TypeString);
	const EStateTreeTransitionType Type = ParseEnumValue(TypeString, EStateTreeTransitionType::GotoState);
	FString ToStateIdentifier;
	UStateTreeState* ToState = nullptr;
	if (Params->TryGetStringField(TEXT("to_state"), ToStateIdentifier) && !ToStateIdentifier.IsEmpty())
	{
		ToState = FindStateTreeState(EditorData, ToStateIdentifier);
		if (!ToState)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Target state not found: %s"), *ToStateIdentifier));
		}
	}
	if (Type == EStateTreeTransitionType::GotoState && !ToState)
	{
		return FUnreelMCPResult::Error(TEXT("GotoState transition requires to_state"));
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add StateTree Transition")));
	StateTree->Modify();
	EditorData->Modify();
	FromState->Modify();
	FStateTreeTransition* Transition = nullptr;
	FString EventTagString;
	if (Trigger == EStateTreeTransitionTrigger::OnEvent && Params->TryGetStringField(TEXT("event_tag"), EventTagString) && !EventTagString.IsEmpty())
	{
		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*EventTagString), false);
		if (!EventTag.IsValid())
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Gameplay tag not found: %s"), *EventTagString));
		}
		Transition = &FromState->AddTransition(Trigger, EventTag, Type, ToState);
	}
	else
	{
		Transition = &FromState->AddTransition(Trigger, Type, ToState);
	}
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree transition added"), StateTreeSummary(StateTree, EditorData));
	Result.Data->SetObjectField(TEXT("transition"), StateTreeTransitionSummary(*Transition));
	AddChangedAsset(Result, StateTree);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetStateTreeNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString NodeId;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("node_id"), NodeId) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node_id, property, or value"));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UObject* Owner = nullptr;
	FStateTreeEditorNode* Node = FindStateTreeNode(EditorData, NodeId, Owner);
	if (!Node)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("StateTree node not found: %s"), *NodeId));
	}
	FString Target;
	Params->TryGetStringField(TEXT("target"), Target);
	Target = Target.IsEmpty() ? TEXT("instance") : Target.ToLower();
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Set StateTree Node Property")));
	StateTree->Modify();
	EditorData->Modify();
	if (Owner)
	{
		Owner->Modify();
	}
	bool bApplied = false;
	FString ChangeTarget;
	FString BeforeValue;
	if (Target == TEXT("node"))
	{
		ChangeTarget = FString::Printf(TEXT("StateTree node %s"), *NodeId);
		BeforeValue = UnreelMCPServiceUtils::ExportStructPropertyText(Node->Node.GetScriptStruct(), Node->Node.GetMemory(), Owner, PropertyName);
		bApplied = ApplyJsonStructProperty(Node->Node.GetScriptStruct(), Node->Node.GetMutableMemory(), Owner, PropertyName, Value, Error);
	}
	else if (Target == TEXT("runtime"))
	{
		ChangeTarget = Node->ExecutionRuntimeDataObject ? Node->ExecutionRuntimeDataObject->GetPathName() : FString::Printf(TEXT("StateTree runtime %s"), *NodeId);
		BeforeValue = Node->ExecutionRuntimeDataObject
			? UnreelMCPServiceUtils::ExportObjectPropertyText(Node->ExecutionRuntimeDataObject, PropertyName)
			: UnreelMCPServiceUtils::ExportStructPropertyText(Node->ExecutionRuntimeData.GetScriptStruct(), Node->ExecutionRuntimeData.GetMemory(), Owner, PropertyName);
		bApplied = Node->ExecutionRuntimeDataObject
			? ApplyJsonProperty(Node->ExecutionRuntimeDataObject, PropertyName, Value, Error)
			: ApplyJsonStructProperty(Node->ExecutionRuntimeData.GetScriptStruct(), Node->ExecutionRuntimeData.GetMutableMemory(), Owner, PropertyName, Value, Error);
	}
	else
	{
		ChangeTarget = Node->InstanceObject ? Node->InstanceObject->GetPathName() : FString::Printf(TEXT("StateTree instance %s"), *NodeId);
		BeforeValue = Node->InstanceObject
			? UnreelMCPServiceUtils::ExportObjectPropertyText(Node->InstanceObject, PropertyName)
			: UnreelMCPServiceUtils::ExportStructPropertyText(Node->Instance.GetScriptStruct(), Node->Instance.GetMemory(), Owner, PropertyName);
		bApplied = Node->InstanceObject
			? ApplyJsonProperty(Node->InstanceObject, PropertyName, Value, Error)
			: ApplyJsonStructProperty(Node->Instance.GetScriptStruct(), Node->Instance.GetMutableMemory(), Owner, PropertyName, Value, Error);
	}
	if (!bApplied)
	{
		return FUnreelMCPResult::Error(Error);
	}
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree node property set"), StateTreeSummary(StateTree, EditorData));
	UObject* AfterOwner = nullptr;
	FStateTreeEditorNode* AfterNode = FindStateTreeNode(EditorData, NodeId, AfterOwner);
	if (!AfterNode)
	{
		Result.Warnings.Add(FString::Printf(TEXT("Property was applied, but StateTree node could not be re-resolved after finalization: %s"), *NodeId));
		AddChangedAsset(Result, StateTree);
		return Result;
	}
	Result.Data->SetObjectField(TEXT("node"), StateTreeNodeSummary(*AfterNode));
	FString AfterValue;
	if (Target == TEXT("node"))
	{
		AfterValue = UnreelMCPServiceUtils::ExportStructPropertyText(AfterNode->Node.GetScriptStruct(), AfterNode->Node.GetMemory(), AfterOwner, PropertyName);
	}
	else if (Target == TEXT("runtime"))
	{
		AfterValue = AfterNode->ExecutionRuntimeDataObject
			? UnreelMCPServiceUtils::ExportObjectPropertyText(AfterNode->ExecutionRuntimeDataObject, PropertyName)
			: UnreelMCPServiceUtils::ExportStructPropertyText(AfterNode->ExecutionRuntimeData.GetScriptStruct(), AfterNode->ExecutionRuntimeData.GetMemory(), AfterOwner, PropertyName);
	}
	else
	{
		AfterValue = AfterNode->InstanceObject
			? UnreelMCPServiceUtils::ExportObjectPropertyText(AfterNode->InstanceObject, PropertyName)
			: UnreelMCPServiceUtils::ExportStructPropertyText(AfterNode->Instance.GetScriptStruct(), AfterNode->Instance.GetMemory(), AfterOwner, PropertyName);
	}
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(ChangeTarget, PropertyName, BeforeValue, AfterValue));
	AddChangedAsset(Result, StateTree);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CompileStateTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	FString Error;
	UStateTree* StateTree = LoadStateTree(AssetPath, Error);
	UStateTreeEditorData* EditorData = LoadStateTreeEditorData(StateTree, Error);
	if (!StateTree || !EditorData)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Compile StateTree")));
	StateTree->Modify();
	EditorData->Modify();
	FinalizeStateTreeEdit(StateTree, EditorData);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("StateTree compiled"), StateTreeSummary(StateTree, EditorData));
	AddChangedAsset(Result, StateTree);
	return Result;
}

UWidgetBlueprint* FUnreelMCPAuthoringService::LoadWidgetBlueprint(const FString& AssetPath, FString& OutError) const
{
	UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(LoadAssetObject(AssetPath));
	if (!WidgetBlueprint)
	{
		OutError = FString::Printf(TEXT("Failed to load Widget Blueprint: %s"), *AssetPath);
		return nullptr;
	}
	if (!WidgetBlueprint->WidgetTree)
	{
		OutError = FString::Printf(TEXT("Widget Blueprint has no editable WidgetTree: %s"), *AssetPath);
		return nullptr;
	}
	return WidgetBlueprint;
}

UWidget* FUnreelMCPAuthoringService::FindWidgetByName(UWidgetTree* WidgetTree, const FString& WidgetName) const
{
	return WidgetTree && !WidgetName.IsEmpty() ? WidgetTree->FindWidget(FName(*WidgetName)) : nullptr;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::WidgetSummary(UWidget* Widget) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!Widget)
	{
		return Data;
	}

	Data->SetStringField(TEXT("name"), Widget->GetName());
	Data->SetStringField(TEXT("class"), Widget->GetClass()->GetPathName());
	Data->SetStringField(TEXT("path"), Widget->GetPathName());

	int32 ChildIndex = INDEX_NONE;
	UPanelWidget* Parent = UWidgetTree::FindWidgetParent(Widget, ChildIndex);
	if (Parent)
	{
		Data->SetStringField(TEXT("parent"), Parent->GetName());
		Data->SetNumberField(TEXT("child_index"), ChildIndex);
	}
	else
	{
		Data->SetStringField(TEXT("parent"), TEXT(""));
		Data->SetNumberField(TEXT("child_index"), -1);
	}

	if (Widget->Slot)
	{
		Data->SetStringField(TEXT("slot_class"), Widget->Slot->GetClass()->GetPathName());
	}
	else
	{
		Data->SetStringField(TEXT("slot_class"), TEXT(""));
	}

	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		Data->SetNumberField(TEXT("child_count"), Panel->GetChildrenCount());
		Data->SetBoolField(TEXT("can_have_children"), true);
	}
	else
	{
		Data->SetNumberField(TEXT("child_count"), 0);
		Data->SetBoolField(TEXT("can_have_children"), false);
	}
	return Data;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::WidgetHierarchySummary(UWidget* Widget) const
{
	TSharedPtr<FJsonObject> Data = WidgetSummary(Widget);
	TArray<TSharedPtr<FJsonValue>> Children;
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
		{
			if (UWidget* Child = Panel->GetChildAt(Index))
			{
				Children.Add(MakeShared<FJsonValueObject>(WidgetHierarchySummary(Child)));
			}
		}
	}
	Data->SetArrayField(TEXT("children"), Children);
	return Data;
}

void FUnreelMCPAuthoringService::MarkWidgetBlueprintChanged(UWidgetBlueprint* WidgetBlueprint, bool bStructurallyModified) const
{
	if (!WidgetBlueprint)
	{
		return;
	}
	if (bStructurallyModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBlueprint);
	}
	WidgetBlueprint->MarkPackageDirty();
}

FUnreelMCPResult FUnreelMCPAuthoringService::ListWidgetTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}

	TSharedPtr<FJsonObject> Data = ObjectSummary(WidgetBlueprint);
	TArray<TSharedPtr<FJsonValue>> Widgets;
	WidgetBlueprint->WidgetTree->ForEachWidget([&Widgets, this](UWidget* Widget)
	{
		Widgets.Add(MakeShared<FJsonValueObject>(WidgetSummary(Widget)));
	});
	Data->SetArrayField(TEXT("widgets"), Widgets);
	Data->SetObjectField(TEXT("root"), WidgetBlueprint->WidgetTree->RootWidget ? WidgetHierarchySummary(WidgetBlueprint->WidgetTree->RootWidget) : MakeShared<FJsonObject>());
	return FUnreelMCPResult::Success(TEXT("Widget tree loaded"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ClassPath;
	FString Name;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: class_path"));
	}
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: name"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	if (FindWidgetByName(WidgetTree, Name))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget already exists: %s"), *Name));
	}

	UClass* WidgetClass = StaticLoadClass(UWidget::StaticClass(), nullptr, *ClassPath);
	if (!WidgetClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("widget class"), ClassPath));
	}
	if (!WidgetClass->IsChildOf(UWidget::StaticClass()) || WidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Class is not a concrete UWidget: %s"), *ClassPath));
	}

	bool bSetRoot = false;
	Params->TryGetBoolField(TEXT("set_root"), bSetRoot);
	FString ParentName;
	Params->TryGetStringField(TEXT("parent"), ParentName);
	if (bSetRoot && !ParentName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("set_root=true cannot be combined with parent"));
	}
	if (bSetRoot && WidgetTree->RootWidget)
	{
		return FUnreelMCPResult::Error(TEXT("WidgetTree already has a root widget; remove it before setting a replacement root"));
	}
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		ParentPanel = Cast<UPanelWidget>(FindWidgetByName(WidgetTree, ParentName));
		if (!ParentPanel)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Parent widget is missing or cannot contain children: %s"), *ParentName));
		}
	}
	else if (WidgetTree->RootWidget && !bSetRoot)
	{
		return FUnreelMCPResult::Error(TEXT("Provide parent or set_root=true when the WidgetTree already has a root widget"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Add Widget %s"), *Name)));
	WidgetBlueprint->Modify();
	WidgetTree->Modify();
	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass, FName(*Name));
	if (!NewWidget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("ConstructWidget returned null for class: %s"), *ClassPath));
	}
	NewWidget->Modify();
#if WITH_EDITORONLY_DATA
	if (!WidgetBlueprint->WidgetVariableNameToGuidMap.Contains(NewWidget->GetFName()))
	{
		WidgetBlueprint->OnVariableAdded(NewWidget->GetFName());
	}
#endif

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		if (!ApplyJsonProperties(NewWidget, *Properties, Error))
		{
			WidgetTree->RemoveWidget(NewWidget);
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget constructed but property assignment failed: %s"), *Error));
		}
	}

	if (bSetRoot || !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = NewWidget;
	}
	if (ParentPanel)
	{
		ParentPanel->Modify();
		if (!ParentPanel->AddChild(NewWidget))
		{
			WidgetTree->RemoveWidget(NewWidget);
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to attach widget %s to parent %s"), *Name, *ParentName));
		}
	}

	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget added"), WidgetSummary(NewWidget));
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ReparentWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	FString ParentName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("parent"), ParentName) || ParentName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: parent"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
	UPanelWidget* NewParent = Cast<UPanelWidget>(FindWidgetByName(WidgetTree, ParentName));
	if (!Widget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}
	if (!NewParent)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Parent widget is missing or cannot contain children: %s"), *ParentName));
	}
	if (Widget == NewParent)
	{
		return FUnreelMCPResult::Error(TEXT("Cannot reparent a widget to itself"));
	}
	bool bParentIsDescendant = false;
	UWidgetTree::ForWidgetAndChildren(Widget, [&bParentIsDescendant, NewParent](UWidget* Candidate)
	{
		if (Candidate == NewParent)
		{
			bParentIsDescendant = true;
		}
	});
	if (bParentIsDescendant)
	{
		return FUnreelMCPResult::Error(TEXT("Cannot reparent a widget under one of its descendants"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Reparent Widget %s"), *WidgetName)));
	WidgetBlueprint->Modify();
	WidgetTree->Modify();
	Widget->Modify();
	NewParent->Modify();

	int32 OldChildIndex = INDEX_NONE;
	if (UPanelWidget* OldParent = UWidgetTree::FindWidgetParent(Widget, OldChildIndex))
	{
		OldParent->Modify();
		OldParent->RemoveChild(Widget);
	}
	else if (WidgetTree->RootWidget == Widget)
	{
		WidgetTree->RootWidget = nullptr;
	}

	if (!NewParent->AddChild(Widget))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to attach widget %s to parent %s"), *WidgetName, *ParentName));
	}

	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget reparented"), WidgetSummary(Widget));
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidgetTree* WidgetTree = WidgetBlueprint->WidgetTree;
	UWidget* Widget = FindWidgetByName(WidgetTree, WidgetName);
	if (!Widget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Remove Widget %s"), *WidgetName)));
	WidgetBlueprint->Modify();
	WidgetTree->Modify();
	Widget->Modify();
	const bool bWasRoot = WidgetTree->RootWidget == Widget;
	if (!WidgetTree->RemoveWidget(Widget))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to remove widget: %s"), *WidgetName));
	}
	if (bWasRoot)
	{
		WidgetTree->RootWidget = nullptr;
	}

	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("removed_widget"), WidgetName);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget removed"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetWidgetTreeWidgetProperty(const TSharedRef<FJsonObject>& Params, bool bSlotProperty) const
{
	FString AssetPath;
	FString WidgetName;
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: property"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: value"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidget* Widget = FindWidgetByName(WidgetBlueprint->WidgetTree, WidgetName);
	if (!Widget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}
	UObject* Target = Widget;
	if (bSlotProperty)
	{
		if (!Widget->Slot)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget has no slot object: %s"), *WidgetName));
		}
		Target = Widget->Slot;
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Set Widget Property %s"), *PropertyName)));
	WidgetBlueprint->Modify();
	Target->Modify();
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Target, PropertyName);
	if (!ApplyJsonProperty(Target, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Target, PropertyName);

	MarkWidgetBlueprintChanged(WidgetBlueprint, false);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(bSlotProperty ? TEXT("Widget slot property set") : TEXT("Widget property set"), WidgetSummary(Widget));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Target->GetPathName(), PropertyName, BeforeValue, AfterValue));
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ListWidgetEvents(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}

	FString WidgetFilter;
	Params->TryGetStringField(TEXT("widget"), WidgetFilter);
	TArray<TSharedPtr<FJsonValue>> Widgets;
	WidgetBlueprint->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget || (!WidgetFilter.IsEmpty() && Widget->GetName() != WidgetFilter))
		{
			return;
		}
		TSharedPtr<FJsonObject> WidgetJson = WidgetSummary(Widget);
		TArray<TSharedPtr<FJsonValue>> Events;
		for (TFieldIterator<FMulticastDelegateProperty> It(Widget->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			FMulticastDelegateProperty* DelegateProperty = *It;
			TSharedPtr<FJsonObject> EventJson = MakeShared<FJsonObject>();
			EventJson->SetStringField(TEXT("name"), DelegateProperty->GetName());
			EventJson->SetStringField(TEXT("display_name"), DelegateProperty->GetDisplayNameText().ToString());
			EventJson->SetStringField(TEXT("signature"), DelegateProperty->SignatureFunction ? DelegateProperty->SignatureFunction->GetPathName() : TEXT(""));
			EventJson->SetBoolField(TEXT("bound"), FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, DelegateProperty->GetFName(), Widget->GetFName()) != nullptr);
			Events.Add(MakeShared<FJsonValueObject>(EventJson));
		}
		WidgetJson->SetArrayField(TEXT("events"), Events);
		Widgets.Add(MakeShared<FJsonValueObject>(WidgetJson));
	});

	TArray<TSharedPtr<FJsonValue>> BoundEvents;
	TArray<UEdGraph*> Graphs;
	WidgetBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UK2Node_ComponentBoundEvent* BoundEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
			{
				TSharedPtr<FJsonObject> BoundJson = MakeShared<FJsonObject>();
				BoundJson->SetStringField(TEXT("graph"), Graph->GetName());
				BoundJson->SetStringField(TEXT("node_id"), BoundEvent->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
				BoundJson->SetStringField(TEXT("widget"), BoundEvent->ComponentPropertyName.ToString());
				BoundJson->SetStringField(TEXT("event"), BoundEvent->DelegatePropertyName.ToString());
				BoundEvents.Add(MakeShared<FJsonValueObject>(BoundJson));
			}
		}
	}

	TSharedPtr<FJsonObject> Data = ObjectSummary(WidgetBlueprint);
	Data->SetArrayField(TEXT("widgets"), Widgets);
	Data->SetArrayField(TEXT("bound_events"), BoundEvents);
	return FUnreelMCPResult::Success(TEXT("Widget events listed"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::BindWidgetEvent(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	FString EventName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("event"), EventName) || EventName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: event"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidget* Widget = FindWidgetByName(WidgetBlueprint->WidgetTree, WidgetName);
	if (!Widget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}
	FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), FName(*EventName));
	if (!DelegateProperty)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget %s has no multicast event: %s"), *WidgetName, *EventName));
	}

	if (const UK2Node_ComponentBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, DelegateProperty->GetFName(), Widget->GetFName()))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("widget"), WidgetName);
		Data->SetStringField(TEXT("event"), EventName);
		Data->SetStringField(TEXT("node_id"), ExistingNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
		Data->SetBoolField(TEXT("already_bound"), true);
		return FUnreelMCPResult::Success(TEXT("Widget event already bound"), Data);
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Bind Widget Event %s.%s"), *WidgetName, *EventName)));
	WidgetBlueprint->Modify();
	Widget->Modify();
	Widget->bIsVariable = true;
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	FObjectProperty* VariableProperty = FindFProperty<FObjectProperty>(WidgetBlueprint->SkeletonGeneratedClass, Widget->GetFName());
	if (!VariableProperty)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to generate widget variable for event binding: %s"), *WidgetName));
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
	FKismetEditorUtilities::CreateNewBoundEventForClass(Widget->GetClass(), DelegateProperty->GetFName(), WidgetBlueprint, VariableProperty, false);
#else
	FKismetEditorUtilities::CreateNewBoundEventForClass(Widget->GetClass(), DelegateProperty->GetFName(), WidgetBlueprint, VariableProperty);
#endif
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	const UK2Node_ComponentBoundEvent* CreatedNode = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, DelegateProperty->GetFName(), Widget->GetFName());
	if (!CreatedNode)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("CreateNewBoundEventForClass did not create a node for %s.%s"), *WidgetName, *EventName));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), WidgetName);
	Data->SetStringField(TEXT("event"), EventName);
	Data->SetStringField(TEXT("node_id"), CreatedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	Data->SetStringField(TEXT("graph"), CreatedNode->GetGraph() ? CreatedNode->GetGraph()->GetName() : TEXT(""));
	Data->SetBoolField(TEXT("already_bound"), false);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget event bound"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveWidgetEventBinding(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	FString EventName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("event"), EventName) || EventName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: event"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const UK2Node_ComponentBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForComponent(WidgetBlueprint, FName(*EventName), FName(*WidgetName));
	if (!ExistingNode)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget event binding not found: %s.%s"), *WidgetName, *EventName));
	}
	UEdGraph* Graph = ExistingNode->GetGraph();
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Existing event node has no graph"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Remove Widget Event %s.%s"), *WidgetName, *EventName)));
	WidgetBlueprint->Modify();
	Graph->Modify();
	Graph->RemoveNode(const_cast<UK2Node_ComponentBoundEvent*>(ExistingNode));
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), WidgetName);
	Data->SetStringField(TEXT("event"), EventName);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget event binding removed"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ListWidgetBindings(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}

	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (const FDelegateEditorBinding& Binding : WidgetBlueprint->Bindings)
	{
		TSharedPtr<FJsonObject> BindingJson = MakeShared<FJsonObject>();
		BindingJson->SetStringField(TEXT("widget"), Binding.ObjectName);
		BindingJson->SetStringField(TEXT("property"), Binding.PropertyName.ToString());
		BindingJson->SetStringField(TEXT("function"), Binding.FunctionName.ToString());
		BindingJson->SetStringField(TEXT("source_property"), Binding.SourceProperty.ToString());
		BindingJson->SetStringField(TEXT("member_guid"), Binding.MemberGuid.ToString(EGuidFormats::DigitsWithHyphens));
		BindingJson->SetStringField(TEXT("kind"), Binding.Kind == EBindingKind::Function ? TEXT("function") : TEXT("property"));
		BindingJson->SetBoolField(TEXT("target_exists"), Binding.DoesBindingTargetExist(WidgetBlueprint));
		Bindings.Add(MakeShared<FJsonValueObject>(BindingJson));
	}

	TSharedPtr<FJsonObject> Data = ObjectSummary(WidgetBlueprint);
	Data->SetArrayField(TEXT("bindings"), Bindings);
	return FUnreelMCPResult::Success(TEXT("Widget bindings listed"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddWidgetBinding(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: property"));
	}

	FString SourcePropertyName;
	FString FunctionName;
	Params->TryGetStringField(TEXT("source_property"), SourcePropertyName);
	Params->TryGetStringField(TEXT("function"), FunctionName);
	if (SourcePropertyName.IsEmpty() == FunctionName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Provide exactly one of source_property or function"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UWidget* Widget = FindWidgetByName(WidgetBlueprint->WidgetTree, WidgetName);
	if (!Widget)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
	}
	if (!FindFProperty<FDelegateProperty>(Widget->GetClass(), FName(*(PropertyName + TEXT("Delegate")))) && !FindFProperty<FDelegateProperty>(Widget->GetClass(), FName(*PropertyName)))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget property is not bindable: %s.%s"), *WidgetName, *PropertyName));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Add Widget Binding %s.%s"), *WidgetName, *PropertyName)));
	WidgetBlueprint->Modify();
	Widget->Modify();
	Widget->bIsVariable = true;
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	FDelegateEditorBinding Binding;
	Binding.ObjectName = WidgetName;
	Binding.PropertyName = FName(*PropertyName);
	if (!SourcePropertyName.IsEmpty())
	{
		FProperty* SourceProperty = FindFProperty<FProperty>(WidgetBlueprint->SkeletonGeneratedClass, FName(*SourcePropertyName));
		if (!SourceProperty)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Source property not found on Widget Blueprint skeleton: %s"), *SourcePropertyName));
		}
		Binding.SourceProperty = SourceProperty->GetFName();
		TArray<FFieldVariant> FieldChain;
		FieldChain.Add(SourceProperty);
		Binding.SourcePath = FEditorPropertyPath(FieldChain);
		UBlueprint::GetGuidFromClassByFieldName<FProperty>(WidgetBlueprint->SkeletonGeneratedClass, SourceProperty->GetFName(), Binding.MemberGuid);
		Binding.Kind = EBindingKind::Property;
	}
	else
	{
		UFunction* Function = WidgetBlueprint->SkeletonGeneratedClass ? WidgetBlueprint->SkeletonGeneratedClass->FindFunctionByName(FName(*FunctionName), EIncludeSuperFlag::IncludeSuper) : nullptr;
		if (!Function)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Function not found on Widget Blueprint skeleton: %s"), *FunctionName));
		}
		Binding.FunctionName = Function->GetFName();
		UBlueprint::GetGuidFromClassByFieldName<UFunction>(Function->GetOwnerClass(), Function->GetFName(), Binding.MemberGuid);
		Binding.Kind = EBindingKind::Function;
	}

	WidgetBlueprint->Bindings.Remove(Binding);
	WidgetBlueprint->Bindings.AddUnique(Binding);
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), Binding.ObjectName);
	Data->SetStringField(TEXT("property"), Binding.PropertyName.ToString());
	Data->SetStringField(TEXT("kind"), Binding.Kind == EBindingKind::Function ? TEXT("function") : TEXT("property"));
	Data->SetStringField(TEXT("function"), Binding.FunctionName.ToString());
	Data->SetStringField(TEXT("source_property"), Binding.SourceProperty.ToString());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget binding added"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveWidgetBinding(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString WidgetName;
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("widget"), WidgetName) || WidgetName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: widget"));
	}
	if (!Params->TryGetStringField(TEXT("property"), PropertyName) || PropertyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: property"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}

	FDelegateEditorBinding Binding;
	Binding.ObjectName = WidgetName;
	Binding.PropertyName = FName(*PropertyName);
	const int32 Removed = WidgetBlueprint->Bindings.Remove(Binding);
	if (Removed <= 0)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Widget binding not found: %s.%s"), *WidgetName, *PropertyName));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Remove Widget Binding %s.%s"), *WidgetName, *PropertyName)));
	WidgetBlueprint->Modify();
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("widget"), WidgetName);
	Data->SetStringField(TEXT("property"), PropertyName);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget binding removed"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ListWidgetMVVM(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	if (!GEditor)
	{
		return FUnreelMCPResult::Error(TEXT("GEditor is unavailable"));
	}
	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (!Subsystem)
	{
		return FUnreelMCPResult::Error(TEXT("MVVM editor subsystem is unavailable"));
	}
	bool bRequestView = false;
	Params->TryGetBoolField(TEXT("request_view"), bRequestView);
	UMVVMBlueprintView* View = bRequestView ? Subsystem->RequestView(WidgetBlueprint) : Subsystem->GetView(WidgetBlueprint);

	TArray<TSharedPtr<FJsonValue>> ViewModels;
	if (View)
	{
		for (const FMVVMBlueprintViewModelContext& Context : View->GetViewModels())
		{
			TSharedPtr<FJsonObject> ViewModelJson = MakeShared<FJsonObject>();
			ViewModelJson->SetStringField(TEXT("id"), Context.GetViewModelId().ToString(EGuidFormats::DigitsWithHyphens));
			ViewModelJson->SetStringField(TEXT("name"), Context.GetViewModelName().ToString());
			ViewModelJson->SetStringField(TEXT("display_name"), Context.GetDisplayName().ToString());
			ViewModelJson->SetStringField(TEXT("class"), Context.GetViewModelClass() ? Context.GetViewModelClass()->GetPathName() : TEXT(""));
			ViewModels.Add(MakeShared<FJsonValueObject>(ViewModelJson));
		}
	}
	TSharedPtr<FJsonObject> Data = ObjectSummary(WidgetBlueprint);
	Data->SetBoolField(TEXT("has_view"), View != nullptr);
	Data->SetNumberField(TEXT("binding_count"), View ? View->GetNumBindings() : 0);
	Data->SetArrayField(TEXT("viewmodels"), ViewModels);
	return FUnreelMCPResult::Success(TEXT("Widget MVVM listed"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddWidgetMVVMViewModel(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: class_path"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UClass* ViewModelClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
	if (!ViewModelClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("ViewModel class"), ClassPath));
	}
	if (!GEditor)
	{
		return FUnreelMCPResult::Error(TEXT("GEditor is unavailable"));
	}
	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (!Subsystem)
	{
		return FUnreelMCPResult::Error(TEXT("MVVM editor subsystem is unavailable"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add MVVM ViewModel")));
	WidgetBlueprint->Modify();
	Subsystem->RequestView(WidgetBlueprint);
	FGuid ViewModelId = Subsystem->AddViewModel(WidgetBlueprint, ViewModelClass);
	if (!ViewModelId.IsValid())
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to add ViewModel class. It must implement NotifyFieldValueChanged: %s"), *ClassPath));
	}

	FString DesiredName;
	Params->TryGetStringField(TEXT("name"), DesiredName);
	if (!DesiredName.IsEmpty())
	{
		if (UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprint))
		{
			if (const FMVVMBlueprintViewModelContext* Context = View->FindViewModel(ViewModelId))
			{
				FText RenameError;
				if (!Subsystem->RenameViewModel(WidgetBlueprint, Context->GetViewModelName(), FName(*DesiredName), RenameError))
				{
					return FUnreelMCPResult::Error(FString::Printf(TEXT("ViewModel was added but rename failed: %s"), *RenameError.ToString()));
				}
			}
		}
	}

	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("id"), ViewModelId.ToString(EGuidFormats::DigitsWithHyphens));
	Data->SetStringField(TEXT("class"), ViewModelClass->GetPathName());
	if (!DesiredName.IsEmpty())
	{
		Data->SetStringField(TEXT("name"), DesiredName);
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget MVVM ViewModel added"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveWidgetMVVMViewModel(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: name"));
	}

	FString Error;
	UWidgetBlueprint* WidgetBlueprint = LoadWidgetBlueprint(AssetPath, Error);
	if (!WidgetBlueprint)
	{
		return FUnreelMCPResult::Error(Error);
	}
	if (!GEditor)
	{
		return FUnreelMCPResult::Error(TEXT("GEditor is unavailable"));
	}
	UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
	if (!Subsystem)
	{
		return FUnreelMCPResult::Error(TEXT("MVVM editor subsystem is unavailable"));
	}
	if (UMVVMBlueprintView* View = Subsystem->GetView(WidgetBlueprint))
	{
		if (!View->FindViewModel(FName(*Name)))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("ViewModel not found: %s"), *Name));
		}
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Remove MVVM ViewModel")));
	WidgetBlueprint->Modify();
	Subsystem->RemoveViewModel(WidgetBlueprint, FName(*Name));
	MarkWidgetBlueprintChanged(WidgetBlueprint, true);
	FKismetEditorUtilities::CompileBlueprint(WidgetBlueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("name"), Name);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Widget MVVM ViewModel removed"), Data);
	AddChangedAsset(Result, WidgetBlueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateBlueprint(const TSharedRef<FJsonObject>& Params) const
{
	FString PackagePath;
	FString AssetName;
	FString Error;
	if (!ResolveAssetPath(Params, PackagePath, AssetName, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FString ParentClassPath = TEXT("/Script/Engine.Actor");
	Params->TryGetStringField(TEXT("parent_class_path"), ParentClassPath);
	UClass* ParentClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ParentClassPath);
	if (!ParentClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("parent class"), ParentClassPath));
	}
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Create Blueprint %s"), *AssetName)));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClass, Package, FName(*AssetName), BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(TEXT("CreateBlueprint returned null"));
	}
	FAssetRegistryModule::AssetCreated(Blueprint);
	Blueprint->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint created"), ObjectSummary(Blueprint));
	AddChangedAsset(Result, Blueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddBlueprintVariable(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: name"));
	}
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(AssetPath));
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load Blueprint: %s"), *AssetPath));
	}

	FString Category = TEXT("bool");
	FString SubcategoryObjectPath;
	FString DefaultValue;
	Params->TryGetStringField(TEXT("category"), Category);
	Params->TryGetStringField(TEXT("subcategory_object"), SubcategoryObjectPath);
	Params->TryGetStringField(TEXT("default_value"), DefaultValue);

	FEdGraphPinType PinType;
	PinType.PinCategory = FName(*Category);
	if (!SubcategoryObjectPath.IsEmpty())
	{
		PinType.PinSubCategoryObject = StaticLoadObject(UObject::StaticClass(), nullptr, *SubcategoryObjectPath);
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Add Blueprint Variable %s"), *Name)));
	Blueprint->Modify();
	if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), PinType, DefaultValue))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to add Blueprint variable: %s"), *Name));
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint variable added"), ObjectSummary(Blueprint));
	AddChangedAsset(Result, Blueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CompileBlueprint(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(AssetPath));
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load Blueprint: %s"), *AssetPath));
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint compiled"), ObjectSummary(Blueprint));
	AddChangedAsset(Result, Blueprint);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateDataTable(const TSharedRef<FJsonObject>& Params) const
{
	FString RowStructPath;
	if (!Params->TryGetStringField(TEXT("row_struct_path"), RowStructPath) || RowStructPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: row_struct_path"));
	}
	UScriptStruct* RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
	if (!RowStruct)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load row struct: %s"), *RowStructPath));
	}
	TSharedRef<FJsonObject> CreateParams = MakeShared<FJsonObject>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Params->Values)
	{
		CreateParams->SetField(Pair.Key, Pair.Value);
	}
	TSharedPtr<FJsonObject> FactoryProps = MakeShared<FJsonObject>();
	FactoryProps->SetStringField(TEXT("Struct"), RowStructPath);
	CreateParams->SetObjectField(TEXT("factory_properties"), FactoryProps);
	return CreateAssetOperation(CreateParams, TEXT("/Script/Engine.DataTable"), TEXT("/Script/UnrealEd.DataTableFactory"));
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetMaterialInstanceParameter(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParameterName;
	FString Type = TEXT("scalar");
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	if (!Params->TryGetStringField(TEXT("parameter"), ParameterName) || ParameterName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: parameter"));
	}
	Params->TryGetStringField(TEXT("type"), Type);
	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(LoadAssetObject(AssetPath));
	if (!Instance)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load MaterialInstanceConstant: %s"), *AssetPath));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Set Material Parameter %s"), *ParameterName)));
	Instance->Modify();
	bool bOk = false;
	if (Type.Equals(TEXT("scalar"), ESearchCase::IgnoreCase))
	{
		double Number = 0.0;
		Params->TryGetNumberField(TEXT("value"), Number);
		bOk = UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(Instance, FName(*ParameterName), static_cast<float>(Number));
	}
	else if (Type.Equals(TEXT("texture"), ESearchCase::IgnoreCase))
	{
		FString TexturePath;
		Params->TryGetStringField(TEXT("value"), TexturePath);
		UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
		if (!Texture)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load texture: %s"), *TexturePath));
		}
		bOk = UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(Instance, FName(*ParameterName), Texture);
	}
	else if (Type.Equals(TEXT("static_switch"), ESearchCase::IgnoreCase))
	{
		bool bValue = false;
		Params->TryGetBoolField(TEXT("value"), bValue);
		bOk = UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, FName(*ParameterName), bValue);
	}
	else
	{
		const TSharedPtr<FJsonObject>* ValueObject = nullptr;
		FLinearColor Color = FLinearColor::White;
		if (Params->TryGetObjectField(TEXT("value"), ValueObject) && ValueObject && ValueObject->IsValid())
		{
			double R = Color.R, G = Color.G, B = Color.B, A = Color.A;
			(*ValueObject)->TryGetNumberField(TEXT("r"), R);
			(*ValueObject)->TryGetNumberField(TEXT("g"), G);
			(*ValueObject)->TryGetNumberField(TEXT("b"), B);
			(*ValueObject)->TryGetNumberField(TEXT("a"), A);
			Color = FLinearColor(R, G, B, A);
		}
		bOk = UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(Instance, FName(*ParameterName), Color);
	}

	if (!bOk)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to set material parameter: %s"), *ParameterName));
	}
	Instance->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material parameter set"), ObjectSummary(Instance));
	AddChangedAsset(Result, Instance);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::GenerateStaticMeshUV(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UStaticMesh* Mesh = Cast<UStaticMesh>(LoadAssetObject(AssetPath));
	if (!Mesh)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load StaticMesh: %s"), *AssetPath));
	}
	double LodValue = 0;
	double ChannelValue = 1;
	Params->TryGetNumberField(TEXT("lod_index"), LodValue);
	Params->TryGetNumberField(TEXT("uv_channel"), ChannelValue);
	FString Mode = TEXT("planar");
	Params->TryGetStringField(TEXT("mode"), Mode);

	UStaticMeshEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return FUnreelMCPResult::Error(TEXT("UStaticMeshEditorSubsystem is unavailable"));
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Generate Static Mesh UV")));
	Mesh->Modify();
	const int32 LodIndex = static_cast<int32>(LodValue);
	const int32 Channel = static_cast<int32>(ChannelValue);
	bool bOk = false;
	if (Mode.Equals(TEXT("box"), ESearchCase::IgnoreCase))
	{
		bOk = Subsystem->GenerateBoxUVChannel(Mesh, LodIndex, Channel, FVector::ZeroVector, FRotator::ZeroRotator, FVector(100.0));
	}
	else if (Mode.Equals(TEXT("cylindrical"), ESearchCase::IgnoreCase))
	{
		bOk = Subsystem->GenerateCylindricalUVChannel(Mesh, LodIndex, Channel, FVector::ZeroVector, FRotator::ZeroRotator, FVector2D(1.0));
	}
	else
	{
		bOk = Subsystem->GeneratePlanarUVChannel(Mesh, LodIndex, Channel, FVector::ZeroVector, FRotator::ZeroRotator, FVector2D(1.0));
	}
	if (!bOk)
	{
		return FUnreelMCPResult::Error(TEXT("Static mesh UV generation failed"));
	}
	Mesh->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Static mesh UV generated"), ObjectSummary(Mesh));
	AddChangedAsset(Result, Mesh);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateSkeletalPhysicsAsset(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadAssetObject(AssetPath));
	if (!Mesh)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load SkeletalMesh: %s"), *AssetPath));
	}
	bool bSetToMesh = true;
	Params->TryGetBoolField(TEXT("set_to_mesh"), bSetToMesh);
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Create Physics Asset")));
	UPhysicsAsset* PhysicsAsset = USkeletalMeshEditorSubsystem::CreatePhysicsAsset(Mesh, bSetToMesh);
	if (!PhysicsAsset)
	{
		return FUnreelMCPResult::Error(TEXT("CreatePhysicsAsset returned null"));
	}
	PhysicsAsset->MarkPackageDirty();
	Mesh->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Physics asset created"), ObjectSummary(PhysicsAsset));
	AddChangedAsset(Result, PhysicsAsset);
	AddChangedAsset(Result, Mesh);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectFoliage(const TSharedRef<FJsonObject>& Params) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnreelMCPResult::Error(TEXT("Editor world is unavailable"));
	}

	FString FoliageTypePath;
	Params->TryGetStringField(TEXT("foliage_type"), FoliageTypePath);
	UFoliageType* FilterType = nullptr;
	if (!FoliageTypePath.IsEmpty())
	{
		FilterType = Cast<UFoliageType>(LoadAssetObject(FoliageTypePath));
		if (!FilterType)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load FoliageType: %s"), *FoliageTypePath));
		}
	}

	bool bIncludeInstances = false;
	Params->TryGetBoolField(TEXT("include_instances"), bIncludeInstances);
	double MaxInstancesValue = 100.0;
	Params->TryGetNumberField(TEXT("max_instances"), MaxInstancesValue);
	const int32 MaxInstances = FMath::Max(0, static_cast<int32>(MaxInstancesValue));

	TArray<TSharedPtr<FJsonValue>> TypeValues;
	int32 TotalInstances = 0;
	int32 FoliageActorCount = 0;
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA)
		{
			continue;
		}
		++FoliageActorCount;
		IFA->ForEachFoliageInfo([&](UFoliageType* Type, FFoliageInfo& Info)
		{
			if (FilterType && Type != FilterType)
			{
				return true;
			}

			TSharedPtr<FJsonObject> TypeJson = MakeShared<FJsonObject>();
			TypeJson->SetStringField(TEXT("foliage_type"), Type ? Type->GetPathName() : FString());
			TypeJson->SetStringField(TEXT("actor"), IFA->GetPathName());
			TypeJson->SetStringField(TEXT("level"), IFA->GetLevel() ? IFA->GetLevel()->GetPathName() : FString());
			TypeJson->SetNumberField(TEXT("instance_count"), Info.Instances.Num());
			TotalInstances += Info.Instances.Num();

			if (bIncludeInstances)
			{
				TArray<TSharedPtr<FJsonValue>> InstanceValues;
				const int32 Count = FMath::Min(MaxInstances, Info.Instances.Num());
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const FFoliageInstance& Instance = Info.Instances[Index];
					TSharedPtr<FJsonObject> InstanceJson = MakeShared<FJsonObject>();
					InstanceJson->SetNumberField(TEXT("index"), Index);
					InstanceJson->SetObjectField(TEXT("location"), VectorSummary(Instance.Location));
					InstanceJson->SetObjectField(TEXT("rotation"), RotatorSummary(Instance.Rotation));
					InstanceJson->SetObjectField(TEXT("scale"), VectorSummary(FVector(Instance.DrawScale3D)));
					InstanceValues.Add(MakeShared<FJsonValueObject>(InstanceJson));
				}
				TypeJson->SetArrayField(TEXT("instances"), InstanceValues);
				TypeJson->SetBoolField(TEXT("instances_truncated"), Info.Instances.Num() > Count);
			}

			TypeValues.Add(MakeShared<FJsonValueObject>(TypeJson));
			return true;
		});
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("foliage_actor_count"), FoliageActorCount);
	Data->SetNumberField(TEXT("total_instance_count"), TotalInstances);
	Data->SetArrayField(TEXT("foliage"), TypeValues);
	return FUnreelMCPResult::Success(TEXT("Foliage inspected"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddFoliageInstance(const TSharedRef<FJsonObject>& Params) const
{
	TArray<TSharedPtr<FJsonValue>> TransformValues;
	TransformValues.Add(MakeShared<FJsonValueObject>(Params));
	TSharedRef<FJsonObject> BatchParams = MakeShared<FJsonObject>();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Params->Values)
	{
		BatchParams->SetField(Pair.Key, Pair.Value);
	}
	BatchParams->SetArrayField(TEXT("transforms"), TransformValues);
	return AddFoliageInstances(BatchParams);
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddFoliageInstances(const TSharedRef<FJsonObject>& Params) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnreelMCPResult::Error(TEXT("Editor world is unavailable"));
	}

	FString FoliageTypePath;
	if (!Params->TryGetStringField(TEXT("foliage_type"), FoliageTypePath) || FoliageTypePath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: foliage_type"));
	}
	UFoliageType* FoliageType = Cast<UFoliageType>(LoadAssetObject(FoliageTypePath));
	if (!FoliageType)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load FoliageType: %s"), *FoliageTypePath));
	}

	const TArray<TSharedPtr<FJsonValue>>* TransformValues = nullptr;
	if (!Params->TryGetArrayField(TEXT("transforms"), TransformValues) || !TransformValues || TransformValues->Num() == 0)
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: transforms"));
	}

	TArray<FTransform> Transforms;
	Transforms.Reserve(TransformValues->Num());
	for (const TSharedPtr<FJsonValue>& Value : *TransformValues)
	{
		const TSharedPtr<FJsonObject>* TransformObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(TransformObject) || !TransformObject || !TransformObject->IsValid())
		{
			return FUnreelMCPResult::Error(TEXT("Each transforms entry must be an object"));
		}
		FString Error;
		const FTransform Transform = ReadFoliageTransform(*TransformObject, Error);
		if (!Error.IsEmpty())
		{
			return FUnreelMCPResult::Error(Error);
		}
		Transforms.Add(Transform);
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add Foliage Instances")));
	for (const FTransform& Transform : Transforms)
	{
		AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(World, true, World->PersistentLevel, Transform.GetLocation());
		if (!IFA)
		{
			return FUnreelMCPResult::Error(TEXT("Failed to get or create InstancedFoliageActor"));
		}

		FFoliageInfo* TypeInfo = nullptr;
		UFoliageType* AddedFoliageType = IFA->AddFoliageType(FoliageType, &TypeInfo);
		if (!AddedFoliageType || !TypeInfo)
		{
			return FUnreelMCPResult::Error(TEXT("Failed to add foliage type to InstancedFoliageActor"));
		}

		FFoliageInstance Instance;
		Instance.Location = Transform.GetLocation();
		Instance.Rotation = Transform.GetRotation().Rotator();
		Instance.DrawScale3D = FVector3f(Transform.GetScale3D());

		IFA->Modify();
		TypeInfo->AddInstance(AddedFoliageType, Instance);
		IFA->MarkPackageDirty();
	}
	World->MarkPackageDirty();
	if (World->PersistentLevel)
	{
		World->PersistentLevel->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("foliage_type"), FoliageType->GetPathName());
	Data->SetNumberField(TEXT("added_count"), Transforms.Num());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Foliage instances added"), Data);
	AddChangedAsset(Result, FoliageType);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveFoliageInstances(const TSharedRef<FJsonObject>& Params) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnreelMCPResult::Error(TEXT("Editor world is unavailable"));
	}

	FString FoliageTypePath;
	if (!Params->TryGetStringField(TEXT("foliage_type"), FoliageTypePath) || FoliageTypePath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: foliage_type"));
	}
	UFoliageType* FoliageType = Cast<UFoliageType>(LoadAssetObject(FoliageTypePath));
	if (!FoliageType)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load FoliageType: %s"), *FoliageTypePath));
	}

	bool bRemoveAll = false;
	Params->TryGetBoolField(TEXT("all"), bRemoveAll);
	const TArray<TSharedPtr<FJsonValue>>* IndexValues = nullptr;
	const bool bHasIndices = Params->TryGetArrayField(TEXT("indices"), IndexValues) && IndexValues;
	const TSharedPtr<FJsonObject>* SphereObject = nullptr;
	const bool bHasSphere = Params->TryGetObjectField(TEXT("sphere"), SphereObject) && SphereObject && SphereObject->IsValid();
	const TSharedPtr<FJsonObject>* BoxObject = nullptr;
	const bool bHasBox = Params->TryGetObjectField(TEXT("box"), BoxObject) && BoxObject && BoxObject->IsValid();

	if (!bRemoveAll && !bHasIndices && !bHasSphere && !bHasBox)
	{
		return FUnreelMCPResult::Error(TEXT("Provide one removal selector: all=true, indices, sphere, or box"));
	}

	int32 RemovedCount = 0;
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Remove Foliage Instances")));
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (!IFA)
		{
			continue;
		}
		FFoliageInfo* Info = IFA->FindInfo(FoliageType);
		if (!Info)
		{
			continue;
		}

		TArray<int32> IndicesToRemove;
		if (bRemoveAll)
		{
			IndicesToRemove.Reserve(Info->Instances.Num());
			for (int32 Index = 0; Index < Info->Instances.Num(); ++Index)
			{
				IndicesToRemove.Add(Index);
			}
		}
		else if (bHasIndices)
		{
			for (const TSharedPtr<FJsonValue>& Value : *IndexValues)
			{
				double IndexValue = -1.0;
				if (Value.IsValid() && Value->TryGetNumber(IndexValue))
				{
					const int32 Index = static_cast<int32>(IndexValue);
					if (Info->Instances.IsValidIndex(Index))
					{
						IndicesToRemove.AddUnique(Index);
					}
				}
			}
		}
		else if (bHasSphere)
		{
			const TSharedPtr<FJsonObject>* CenterObject = nullptr;
			if (!(*SphereObject)->TryGetObjectField(TEXT("center"), CenterObject) || !CenterObject || !CenterObject->IsValid())
			{
				return FUnreelMCPResult::Error(TEXT("sphere selector requires center {x,y,z}"));
			}
			double Radius = 0.0;
			if (!(*SphereObject)->TryGetNumberField(TEXT("radius"), Radius) || Radius <= 0.0)
			{
				return FUnreelMCPResult::Error(TEXT("sphere selector requires positive radius"));
			}
			FVector Center;
			ReadVectorObject(*CenterObject, Center);
			Info->GetInstancesInsideSphere(FSphere(Center, Radius), IndicesToRemove);
		}
		else if (bHasBox)
		{
			const TSharedPtr<FJsonObject>* MinObject = nullptr;
			const TSharedPtr<FJsonObject>* MaxObject = nullptr;
			if (!(*BoxObject)->TryGetObjectField(TEXT("min"), MinObject) || !MinObject || !MinObject->IsValid() ||
				!(*BoxObject)->TryGetObjectField(TEXT("max"), MaxObject) || !MaxObject || !MaxObject->IsValid())
			{
				return FUnreelMCPResult::Error(TEXT("box selector requires min {x,y,z} and max {x,y,z}"));
			}
			FVector Min;
			FVector Max;
			ReadVectorObject(*MinObject, Min);
			ReadVectorObject(*MaxObject, Max);
			Info->GetInstancesInsideBounds(FBox(Min, Max), IndicesToRemove);
		}

		if (IndicesToRemove.Num() > 0)
		{
			IFA->Modify();
			Info->RemoveInstances(IndicesToRemove, true);
			RemovedCount += IndicesToRemove.Num();
			IFA->MarkPackageDirty();
		}
	}

	World->MarkPackageDirty();
	if (World->PersistentLevel)
	{
		World->PersistentLevel->MarkPackageDirty();
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("foliage_type"), FoliageType->GetPathName());
	Data->SetNumberField(TEXT("removed_count"), RemovedCount);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Foliage instances removed"), Data);
	AddChangedAsset(Result, FoliageType);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateLandscapeLayerInfo(const TSharedRef<FJsonObject>& Params) const
{
	FString PackagePath;
	FString AssetName;
	FString Error;
	if (!ResolveAssetPath(Params, PackagePath, AssetName, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FString LayerName = TEXT("Layer");
	Params->TryGetStringField(TEXT("layer_name"), LayerName);
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Create Layer Info %s"), *AssetName)));
	UPackage* Package = CreatePackage(*(PackagePath / AssetName));
	ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (!LayerInfo)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create LandscapeLayerInfoObject"));
	}
	LayerInfo->SetLayerName(FName(*LayerName), false);
	FAssetRegistryModule::AssetCreated(LayerInfo);
	Package->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Landscape layer info created"), ObjectSummary(LayerInfo));
	AddChangedAsset(Result, LayerInfo);
	return Result;
}

ALandscape* FUnreelMCPAuthoringService::ResolveLandscape(const TSharedRef<FJsonObject>& Params, FString& OutError) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		OutError = TEXT("Editor world is unavailable");
		return nullptr;
	}

	FString ActorFilter;
	Params->TryGetStringField(TEXT("actor"), ActorFilter);
	ALandscape* FirstLandscape = nullptr;
	int32 MatchCount = 0;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		if (!Landscape)
		{
			continue;
		}
		if (!FirstLandscape)
		{
			FirstLandscape = Landscape;
		}
		if (ActorFilter.IsEmpty() ||
			Landscape->GetName() == ActorFilter ||
			Landscape->GetPathName() == ActorFilter ||
			Landscape->GetActorLabel() == ActorFilter)
		{
			++MatchCount;
			FirstLandscape = Landscape;
		}
	}

	if (!FirstLandscape)
	{
		OutError = TEXT("No Landscape actor found in the current editor world");
		return nullptr;
	}
	if (ActorFilter.IsEmpty() && MatchCount > 1)
	{
		OutError = TEXT("Multiple Landscape actors found; provide actor path, name, or label");
		return nullptr;
	}
	if (!ActorFilter.IsEmpty() && MatchCount == 0)
	{
		OutError = FString::Printf(TEXT("Landscape actor not found: %s"), *ActorFilter);
		return nullptr;
	}
	return FirstLandscape;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::LandscapeSummary(ALandscape* Landscape) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!Landscape)
	{
		return Data;
	}

	Data->SetStringField(TEXT("path"), Landscape->GetPathName());
	Data->SetStringField(TEXT("name"), Landscape->GetName());
	Data->SetStringField(TEXT("label"), Landscape->GetActorLabel());
	Data->SetObjectField(TEXT("location"), VectorSummary(Landscape->GetActorLocation()));
	Data->SetObjectField(TEXT("scale"), VectorSummary(Landscape->GetActorScale3D()));
	Data->SetStringField(TEXT("material"), Landscape->LandscapeMaterial ? Landscape->LandscapeMaterial->GetPathName() : FString());
	Data->SetNumberField(TEXT("component_count"), Landscape->LandscapeComponents.Num());
	Data->SetNumberField(TEXT("component_size_quads"), Landscape->ComponentSizeQuads);
	Data->SetNumberField(TEXT("num_subsections"), Landscape->NumSubsections);
	Data->SetNumberField(TEXT("subsection_size_quads"), Landscape->SubsectionSizeQuads);

	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		FIntRect Extent;
		if (LandscapeInfo->GetLandscapeExtent(Extent))
		{
			TSharedPtr<FJsonObject> ExtentJson = MakeShared<FJsonObject>();
			ExtentJson->SetNumberField(TEXT("min_x"), Extent.Min.X);
			ExtentJson->SetNumberField(TEXT("min_y"), Extent.Min.Y);
			ExtentJson->SetNumberField(TEXT("max_x"), Extent.Max.X);
			ExtentJson->SetNumberField(TEXT("max_y"), Extent.Max.Y);
			Data->SetObjectField(TEXT("extent"), ExtentJson);
		}
		TArray<TSharedPtr<FJsonValue>> LayerValues;
		for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
		{
			TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
			LayerJson->SetStringField(TEXT("name"), Layer.GetLayerName().ToString());
			LayerJson->SetStringField(TEXT("layer_info"), Layer.LayerInfoObj ? Layer.LayerInfoObj->GetPathName() : FString());
			LayerValues.Add(MakeShared<FJsonValueObject>(LayerJson));
		}
		Data->SetArrayField(TEXT("layers"), LayerValues);
	}

	return Data;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectLandscape(const TSharedRef<FJsonObject>& Params) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnreelMCPResult::Error(TEXT("Editor world is unavailable"));
	}

	FString ActorFilter;
	Params->TryGetStringField(TEXT("actor"), ActorFilter);
	TArray<TSharedPtr<FJsonValue>> LandscapeValues;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* Landscape = *It;
		if (!Landscape)
		{
			continue;
		}
		if (!ActorFilter.IsEmpty() &&
			Landscape->GetName() != ActorFilter &&
			Landscape->GetPathName() != ActorFilter &&
			Landscape->GetActorLabel() != ActorFilter)
		{
			continue;
		}
		LandscapeValues.Add(MakeShared<FJsonValueObject>(LandscapeSummary(Landscape)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("count"), LandscapeValues.Num());
	Data->SetArrayField(TEXT("landscapes"), LandscapeValues);
	return FUnreelMCPResult::Success(TEXT("Landscape inspected"), Data);
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateLandscape(const TSharedRef<FJsonObject>& Params) const
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUnreelMCPResult::Error(TEXT("Editor world is unavailable"));
	}

	double ComponentsXValue = 1.0;
	double ComponentsYValue = 1.0;
	double SectionsValue = 1.0;
	double QuadsValue = 7.0;
	double HeightValue = 0.0;
	Params->TryGetNumberField(TEXT("components_x"), ComponentsXValue);
	Params->TryGetNumberField(TEXT("components_y"), ComponentsYValue);
	Params->TryGetNumberField(TEXT("sections_per_component"), SectionsValue);
	Params->TryGetNumberField(TEXT("quads_per_section"), QuadsValue);
	Params->TryGetNumberField(TEXT("height"), HeightValue);

	const int32 ComponentsX = FMath::Clamp(static_cast<int32>(ComponentsXValue), 1, 64);
	const int32 ComponentsY = FMath::Clamp(static_cast<int32>(ComponentsYValue), 1, 64);
	const int32 SectionsPerComponent = FMath::Clamp(static_cast<int32>(SectionsValue), 1, 2);
	const int32 QuadsPerSection = FMath::Clamp(static_cast<int32>(QuadsValue), 1, 255);
	const int32 SizeX = ComponentsX * SectionsPerComponent * QuadsPerSection + 1;
	const int32 SizeY = ComponentsY * SectionsPerComponent * QuadsPerSection + 1;

	FVector Location = FVector::ZeroVector;
	ReadVectorField(Params, TEXT("location"), Location, FVector::ZeroVector);
	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonObject>* RotationObject = nullptr;
	if (Params->TryGetObjectField(TEXT("rotation"), RotationObject) && RotationObject && RotationObject->IsValid())
	{
		ReadRotatorField(*RotationObject, Rotation);
	}

	FVector Scale = FVector(100.0, 100.0, 100.0);
	double UniformScale = 0.0;
	if (Params->TryGetNumberField(TEXT("scale"), UniformScale))
	{
		Scale = FVector(UniformScale);
	}
	else
	{
		ReadVectorField(Params, TEXT("scale"), Scale, FVector(100.0, 100.0, 100.0));
	}

	FString MaterialPath;
	Params->TryGetStringField(TEXT("material"), MaterialPath);
	UMaterialInterface* Material = nullptr;
	if (!MaterialPath.IsEmpty())
	{
		Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
		if (!Material)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load Landscape material: %s"), *MaterialPath));
		}
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Create Landscape")));
	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transactional;
	ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), FTransform(Rotation, Location), SpawnParams);
	if (!Landscape)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to spawn Landscape actor"));
	}

	Landscape->Modify();
	Landscape->LandscapeMaterial = Material;
	Landscape->SetActorRelativeScale3D(Scale);
	Landscape->StaticLightingLOD = FMath::DivideAndRoundUp(FMath::CeilLogTwo((SizeX * SizeY) / (2048 * 2048) + 1), static_cast<uint32>(2));

	FString ActorLabel;
	if (Params->TryGetStringField(TEXT("actor_label"), ActorLabel) && !ActorLabel.IsEmpty())
	{
		Landscape->SetActorLabel(ActorLabel);
	}

	TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
	TArray<uint16> HeightData;
	HeightData.Init(LandscapeDataAccess::GetTexHeight(static_cast<float>(HeightValue)), SizeX * SizeY);
	HeightDataPerLayers.Add(FGuid(), MoveTemp(HeightData));
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
	MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

	Landscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSection, HeightDataPerLayers, TEXT(""), MaterialLayerDataPerLayers, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		LandscapeInfo->UpdateLayerInfoMap(Landscape);
	}
	Landscape->MarkPackageDirty();
	World->MarkPackageDirty();

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Landscape created"), LandscapeSummary(Landscape));
	AddChangedAsset(Result, Landscape);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetLandscapeHeightRegion(const TSharedRef<FJsonObject>& Params) const
{
	FString Error;
	ALandscape* Landscape = ResolveLandscape(Params, Error);
	if (!Landscape)
	{
		return FUnreelMCPResult::Error(Error);
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return FUnreelMCPResult::Error(TEXT("Landscape has no LandscapeInfo"));
	}

	double MinXValue = 0.0;
	double MinYValue = 0.0;
	double MaxXValue = 0.0;
	double MaxYValue = 0.0;
	if (!Params->TryGetNumberField(TEXT("min_x"), MinXValue) ||
		!Params->TryGetNumberField(TEXT("min_y"), MinYValue) ||
		!Params->TryGetNumberField(TEXT("max_x"), MaxXValue) ||
		!Params->TryGetNumberField(TEXT("max_y"), MaxYValue))
	{
		return FUnreelMCPResult::Error(TEXT("Missing required region fields: min_x, min_y, max_x, max_y"));
	}
	int32 MinX = static_cast<int32>(MinXValue);
	int32 MinY = static_cast<int32>(MinYValue);
	int32 MaxX = static_cast<int32>(MaxXValue);
	int32 MaxY = static_cast<int32>(MaxYValue);
	if (MaxX < MinX || MaxY < MinY)
	{
		return FUnreelMCPResult::Error(TEXT("Invalid landscape region bounds"));
	}

	const int32 Width = MaxX - MinX + 1;
	const int32 Height = MaxY - MinY + 1;
	TArray<uint16> HeightData;
	HeightData.SetNum(Width * Height);
	const TArray<TSharedPtr<FJsonValue>>* HeightValues = nullptr;
	if (Params->TryGetArrayField(TEXT("heights"), HeightValues) && HeightValues && HeightValues->Num() > 0)
	{
		if (HeightValues->Num() != HeightData.Num())
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("heights length must be %d for the requested region"), HeightData.Num()));
		}
		for (int32 Index = 0; Index < HeightValues->Num(); ++Index)
		{
			double HeightValue = 0.0;
			(*HeightValues)[Index]->TryGetNumber(HeightValue);
			HeightData[Index] = LandscapeDataAccess::GetTexHeight(static_cast<float>(HeightValue));
		}
	}
	else
	{
		double HeightValue = 0.0;
		Params->TryGetNumberField(TEXT("height"), HeightValue);
		HeightData.Init(LandscapeDataAccess::GetTexHeight(static_cast<float>(HeightValue)), HeightData.Num());
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Set Landscape Height Region")));
	Landscape->Modify();
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);
	LandscapeInfo->UpdateLayerInfoMap(Landscape);
	Landscape->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = LandscapeSummary(Landscape);
	Data->SetNumberField(TEXT("edited_width"), Width);
	Data->SetNumberField(TEXT("edited_height"), Height);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Landscape height region set"), Data);
	AddChangedAsset(Result, Landscape);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::PaintLandscapeLayerRegion(const TSharedRef<FJsonObject>& Params) const
{
	FString Error;
	ALandscape* Landscape = ResolveLandscape(Params, Error);
	if (!Landscape)
	{
		return FUnreelMCPResult::Error(Error);
	}
	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		return FUnreelMCPResult::Error(TEXT("Landscape has no LandscapeInfo"));
	}

	FString LayerInfoPath;
	if (!Params->TryGetStringField(TEXT("layer_info"), LayerInfoPath) || LayerInfoPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: layer_info"));
	}
	ULandscapeLayerInfoObject* LayerInfo = Cast<ULandscapeLayerInfoObject>(LoadAssetObject(LayerInfoPath));
	if (!LayerInfo)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load LandscapeLayerInfoObject: %s"), *LayerInfoPath));
	}

	double MinXValue = 0.0;
	double MinYValue = 0.0;
	double MaxXValue = 0.0;
	double MaxYValue = 0.0;
	if (!Params->TryGetNumberField(TEXT("min_x"), MinXValue) ||
		!Params->TryGetNumberField(TEXT("min_y"), MinYValue) ||
		!Params->TryGetNumberField(TEXT("max_x"), MaxXValue) ||
		!Params->TryGetNumberField(TEXT("max_y"), MaxYValue))
	{
		return FUnreelMCPResult::Error(TEXT("Missing required region fields: min_x, min_y, max_x, max_y"));
	}
	int32 MinX = static_cast<int32>(MinXValue);
	int32 MinY = static_cast<int32>(MinYValue);
	int32 MaxX = static_cast<int32>(MaxXValue);
	int32 MaxY = static_cast<int32>(MaxYValue);
	if (MaxX < MinX || MaxY < MinY)
	{
		return FUnreelMCPResult::Error(TEXT("Invalid landscape region bounds"));
	}
	double AlphaValue = 255.0;
	Params->TryGetNumberField(TEXT("alpha"), AlphaValue);
	const uint8 Alpha = static_cast<uint8>(FMath::Clamp(static_cast<int32>(AlphaValue), 0, 255));

	const int32 Width = MaxX - MinX + 1;
	const int32 Height = MaxY - MinY + 1;
	TArray<uint8> AlphaData;
	AlphaData.Init(Alpha, Width * Height);

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Paint Landscape Layer Region")));
	Landscape->Modify();
	if (!Landscape->HasTargetLayer(FLandscapeTargetLayerSettings(LayerInfo)))
	{
		Landscape->AddTargetLayer(LayerInfo->GetLayerName(), FLandscapeTargetLayerSettings(LayerInfo));
	}
	LandscapeInfo->UpdateLayerInfoMap(Landscape, true);
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LandscapeEdit.SetAlphaData(LayerInfo, MinX, MinY, MaxX, MaxY, AlphaData.GetData(), 0, ELandscapeLayerPaintingRestriction::None);
	Landscape->MarkPackageDirty();
	LayerInfo->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = LandscapeSummary(Landscape);
	Data->SetStringField(TEXT("layer_info"), LayerInfo->GetPathName());
	Data->SetNumberField(TEXT("alpha"), Alpha);
	Data->SetNumberField(TEXT("painted_width"), Width);
	Data->SetNumberField(TEXT("painted_height"), Height);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Landscape layer region painted"), Data);
	AddChangedAsset(Result, Landscape);
	AddChangedAsset(Result, LayerInfo);
	return Result;
}

USoundNode* FUnreelMCPAuthoringService::ResolveSoundCueNode(USoundCue* SoundCue, const TSharedRef<FJsonObject>& Params, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if (!SoundCue)
	{
		return nullptr;
	}
	double NodeIndex = -1;
	if (!Params->TryGetNumberField(TEXT("node_index"), NodeIndex) && !Params->TryGetNumberField(TEXT("parent_index"), NodeIndex))
	{
		return nullptr;
	}
	OutIndex = static_cast<int32>(NodeIndex);
	return SoundCue->AllNodes.IsValidIndex(OutIndex) ? SoundCue->AllNodes[OutIndex] : nullptr;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectSoundCue(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	if (!SoundCue)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load SoundCue"));
	}
	return FUnreelMCPResult::Success(TEXT("SoundCue inspected"), SoundCueSummary(SoundCue));
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddSoundCueNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or class_path"));
	}
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	UClass* NodeClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!NodeClass)
	{
		NodeClass = StaticLoadClass(USoundNode::StaticClass(), nullptr, *ClassPath);
	}
	if (!SoundCue || !NodeClass || !NodeClass->IsChildOf(USoundNode::StaticClass()) || NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve SoundCue or concrete USoundNode class: %s.%s"), *ClassPath, *UnreelMCPServiceUtils::ScriptLoadHint(ClassPath)));
	}
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddSoundCueNode", "Add SoundCue Node"));
	SoundCue->Modify();
	USoundNode* NewNode = SoundCue->ConstructSoundNode<USoundNode>(NodeClass, false);
	if (!NewNode)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to construct SoundCue node"));
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		FString Error;
		if (!ApplyJsonProperties(NewNode, *Properties, Error))
		{
			return FUnreelMCPResult::Error(Error);
		}
	}
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue node added"), SoundCueSummary(SoundCue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ConnectSoundCueNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	double ChildIndexNumber = -1;
	double InputIndexNumber = 0;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetNumberField(TEXT("child_index"), ChildIndexNumber))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or child_index"));
	}
	Params->TryGetNumberField(TEXT("input_index"), InputIndexNumber);
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	int32 ParentIndex = INDEX_NONE;
	USoundNode* ParentNode = ResolveSoundCueNode(SoundCue, Params, ParentIndex);
	const int32 ChildIndex = static_cast<int32>(ChildIndexNumber);
	const int32 InputIndex = static_cast<int32>(InputIndexNumber);
	if (!SoundCue || !ParentNode || !SoundCue->AllNodes.IsValidIndex(ChildIndex) || InputIndex < 0 || InputIndex >= USoundNode::MAX_ALLOWED_CHILD_NODES)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve parent/child SoundCue node or input_index"));
	}
	USoundNode* ChildNode = SoundCue->AllNodes[ChildIndex];
	if (!ChildNode || ChildNode == ParentNode)
	{
		return FUnreelMCPResult::Error(TEXT("Invalid child SoundCue node"));
	}
	if (ParentNode->GetMaxChildNodes() >= 0 && InputIndex >= ParentNode->GetMaxChildNodes())
	{
		return FUnreelMCPResult::Error(TEXT("input_index exceeds parent node max child count"));
	}
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "ConnectSoundCueNode", "Connect SoundCue Node"));
	SoundCue->Modify();
	ParentNode->Modify();
	while (ParentNode->ChildNodes.Num() <= InputIndex)
	{
		ParentNode->ChildNodes.Add(nullptr);
	}
	ParentNode->ChildNodes[InputIndex] = ChildNode;
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue nodes connected"), SoundCueSummary(SoundCue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::DisconnectSoundCueNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	double InputIndexNumber = 0;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	Params->TryGetNumberField(TEXT("input_index"), InputIndexNumber);
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	int32 ParentIndex = INDEX_NONE;
	USoundNode* ParentNode = ResolveSoundCueNode(SoundCue, Params, ParentIndex);
	const int32 InputIndex = static_cast<int32>(InputIndexNumber);
	if (!SoundCue || !ParentNode || !ParentNode->ChildNodes.IsValidIndex(InputIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve SoundCue parent node or child input"));
	}
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "DisconnectSoundCueNode", "Disconnect SoundCue Node"));
	SoundCue->Modify();
	ParentNode->Modify();
	ParentNode->ChildNodes[InputIndex] = nullptr;
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue node disconnected"), SoundCueSummary(SoundCue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetSoundCueOutput(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	int32 NodeIndex = INDEX_NONE;
	USoundNode* Node = ResolveSoundCueNode(SoundCue, Params, NodeIndex);
	if (!SoundCue || !Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve SoundCue node"));
	}
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetSoundCueOutput", "Set SoundCue Output"));
	SoundCue->Modify();
	SoundCue->FirstNode = Node;
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue output set"), SoundCueSummary(SoundCue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetSoundCueNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, property, or value"));
	}
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	int32 NodeIndex = INDEX_NONE;
	USoundNode* Node = ResolveSoundCueNode(SoundCue, Params, NodeIndex);
	if (!SoundCue || !Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve SoundCue node"));
	}
	FString Error;
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetSoundCueNodeProperty", "Set SoundCue Node Property"));
	SoundCue->Modify();
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	if (!ApplyJsonProperty(Node, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue node property set"), SoundCueSummary(SoundCue));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Node->GetPathName(), PropertyName, BeforeValue, AfterValue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveSoundCueNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	USoundCue* SoundCue = Cast<USoundCue>(LoadAssetObject(AssetPath));
	int32 NodeIndex = INDEX_NONE;
	USoundNode* Node = ResolveSoundCueNode(SoundCue, Params, NodeIndex);
	if (!SoundCue || !Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve SoundCue node"));
	}
	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveSoundCueNode", "Remove SoundCue Node"));
	SoundCue->Modify();
	if (SoundCue->FirstNode == Node)
	{
		SoundCue->FirstNode = nullptr;
	}
	for (USoundNode* Candidate : SoundCue->AllNodes)
	{
		if (!Candidate)
		{
			continue;
		}
		Candidate->Modify();
		for (TObjectPtr<USoundNode>& Child : Candidate->ChildNodes)
		{
			if (Child == Node)
			{
				Child = nullptr;
			}
		}
	}
	SoundCue->AllNodes.RemoveAt(NodeIndex);
	SoundCue->CacheAggregateValues();
	SoundCue->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("SoundCue node removed"), SoundCueSummary(SoundCue));
	AddChangedAsset(Result, SoundCue);
	return Result;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::SoundCueSummary(USoundCue* SoundCue) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!SoundCue)
	{
		return Data;
	}
	Data->SetStringField(TEXT("asset"), SoundCue->GetPathName());
	Data->SetNumberField(TEXT("node_count"), SoundCue->AllNodes.Num());
	Data->SetNumberField(TEXT("duration"), SoundCue->GetDuration());
	const int32 FirstNodeIndex = SoundCue->FirstNode ? SoundCue->AllNodes.IndexOfByKey(SoundCue->FirstNode) : INDEX_NONE;
	Data->SetNumberField(TEXT("first_node_index"), FirstNodeIndex);

	TArray<TSharedPtr<FJsonValue>> NodeValues;
	for (int32 Index = 0; Index < SoundCue->AllNodes.Num(); ++Index)
	{
		USoundNode* Node = SoundCue->AllNodes[Index];
		TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
		NodeJson->SetNumberField(TEXT("index"), Index);
		NodeJson->SetBoolField(TEXT("is_output"), Node && Node == SoundCue->FirstNode);
		NodeJson->SetStringField(TEXT("class"), Node ? Node->GetClass()->GetPathName() : FString());
		NodeJson->SetStringField(TEXT("name"), Node ? Node->GetName() : FString());
		NodeJson->SetNumberField(TEXT("max_children"), Node ? Node->GetMaxChildNodes() : 0);
		NodeJson->SetNumberField(TEXT("min_children"), Node ? Node->GetMinChildNodes() : 0);
		NodeJson->SetNumberField(TEXT("duration"), Node ? Node->GetDuration() : 0.0f);
		TArray<TSharedPtr<FJsonValue>> ChildValues;
		if (Node)
		{
			for (int32 ChildIndex = 0; ChildIndex < Node->ChildNodes.Num(); ++ChildIndex)
			{
				USoundNode* Child = Node->ChildNodes[ChildIndex];
				TSharedPtr<FJsonObject> ChildJson = MakeShared<FJsonObject>();
				ChildJson->SetNumberField(TEXT("input_index"), ChildIndex);
				ChildJson->SetNumberField(TEXT("node_index"), Child ? SoundCue->AllNodes.IndexOfByKey(Child) : INDEX_NONE);
				ChildJson->SetStringField(TEXT("class"), Child ? Child->GetClass()->GetPathName() : FString());
				ChildValues.Add(MakeShared<FJsonValueObject>(ChildJson));
			}
		}
		NodeJson->SetArrayField(TEXT("children"), ChildValues);
		NodeValues.Add(MakeShared<FJsonValueObject>(NodeJson));
	}
	Data->SetArrayField(TEXT("nodes"), NodeValues);
	return Data;
}

UObject* FUnreelMCPAuthoringService::LoadMetaSoundDocumentObject(const FString& AssetPath) const
{
	UObject* Object = LoadAssetObject(AssetPath);
	return Object && Object->GetClass()->ImplementsInterface(UMetaSoundDocumentInterface::StaticClass()) ? Object : nullptr;
}

UMetaSoundBuilderBase* FUnreelMCPAuthoringService::CreateMetaSoundBuilder(UObject* MetaSoundObject) const
{
	if (!MetaSoundObject)
	{
		return nullptr;
	}
	TScriptInterface<IMetaSoundDocumentInterface> MetaSound(MetaSoundObject);
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
	UMetaSoundBuilderBase* Builder = UMetaSoundEditorSubsystem::GetChecked().FindOrBeginBuilding(MetaSound, Result);
	return Result == EMetaSoundBuilderResult::Succeeded ? Builder : nullptr;
}

bool FUnreelMCPAuthoringService::BuildMetaSoundLiteral(const TSharedPtr<FJsonValue>& Value, const FString& DataType, FMetasoundFrontendLiteral& OutLiteral, FString& OutError) const
{
	const FString LowerType = DataType.ToLower();
	if (!Value.IsValid() || Value->Type == EJson::Null)
	{
		if (LowerType == TEXT("bool"))
		{
			OutLiteral.Set(false);
		}
		else if (LowerType == TEXT("int32") || LowerType == TEXT("int"))
		{
			OutLiteral.Set(0);
		}
		else if (LowerType == TEXT("float") || LowerType == TEXT("double"))
		{
			OutLiteral.Set(0.0f);
		}
		else if (LowerType == TEXT("string"))
		{
			OutLiteral.Set(FString());
		}
		return true;
	}
	if (LowerType == TEXT("bool"))
	{
		bool bValue = false;
		if (!Value->TryGetBool(bValue))
		{
			OutError = TEXT("Expected bool literal");
			return false;
		}
		OutLiteral.Set(bValue);
		return true;
	}
	if (LowerType == TEXT("int32") || LowerType == TEXT("int"))
	{
		double Number = 0.0;
		if (!Value->TryGetNumber(Number))
		{
			OutError = TEXT("Expected numeric int literal");
			return false;
		}
		OutLiteral.Set(static_cast<int32>(Number));
		return true;
	}
	if (LowerType == TEXT("float") || LowerType == TEXT("double"))
	{
		double Number = 0.0;
		if (!Value->TryGetNumber(Number))
		{
			OutError = TEXT("Expected numeric float literal");
			return false;
		}
		OutLiteral.Set(static_cast<float>(Number));
		return true;
	}
	if (LowerType == TEXT("string"))
	{
		FString StringValue;
		if (!Value->TryGetString(StringValue))
		{
			OutError = TEXT("Expected string literal");
			return false;
		}
		OutLiteral.Set(StringValue);
		return true;
	}
	if (LowerType == TEXT("object") || LowerType == TEXT("soundwave") || LowerType == TEXT("sound") || LowerType == TEXT("waveasset"))
	{
		FString ObjectPath;
		if (!Value->TryGetString(ObjectPath))
		{
			OutError = TEXT("Expected object asset path literal");
			return false;
		}
		UObject* ObjectValue = LoadAssetObject(ObjectPath);
		if (!ObjectValue)
		{
			OutError = TEXT("Failed to load object literal asset");
			return false;
		}
		OutLiteral.Set(ObjectValue);
		return true;
	}
	return true;
}

static bool ParseMetaSoundNodeGuid(const TSharedRef<FJsonObject>& Params, const TCHAR* FieldName, FGuid& OutGuid)
{
	FString GuidString;
	return Params->TryGetStringField(FieldName, GuidString) && FGuid::Parse(GuidString, OutGuid);
}

static bool BuildMetaSoundToAsset(UMetaSoundBuilderBase* Builder, UObject* MetaSoundObject)
{
	if (!Builder || !MetaSoundObject)
	{
		return false;
	}
	Builder->InitNodeLocations();
	Builder->ConformObjectToDocument();
	MetaSoundObject->MarkPackageDirty();
	return true;
}

static FMetasoundFrontendClassName ReadMetaSoundClassName(const TSharedRef<FJsonObject>& Params)
{
	FString ClassNameString;
	FMetasoundFrontendClassName ClassName;
	if (Params->TryGetStringField(TEXT("class_name"), ClassNameString) && FMetasoundFrontendClassName::Parse(ClassNameString, ClassName))
	{
		return ClassName;
	}
	FString Namespace;
	FString Name;
	FString Variant;
	Params->TryGetStringField(TEXT("namespace"), Namespace);
	Params->TryGetStringField(TEXT("name"), Name);
	Params->TryGetStringField(TEXT("variant"), Variant);
	return FMetasoundFrontendClassName(FName(*Namespace), FName(*Name), FName(*Variant));
}

FUnreelMCPResult FUnreelMCPAuthoringService::CreateMetaSoundSource(const TSharedRef<FJsonObject>& Params) const
{
	FString PackagePath;
	FString AssetName;
	FString Error;
	if (!ResolveAssetPath(Params, PackagePath, AssetName, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	if (LoadAssetObject(PackagePath / AssetName))
	{
		return FUnreelMCPResult::Error(TEXT("Asset already exists"));
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	FMetaSoundBuilderNodeOutputHandle OnPlayOutput;
	FMetaSoundBuilderNodeInputHandle OnFinishedInput;
	TArray<FMetaSoundBuilderNodeInputHandle> AudioOutputs;
	UMetaSoundSourceBuilder* Builder = UMetaSoundBuilderSubsystem::GetChecked().CreateSourceBuilder(
		FName(*AssetName),
		OnPlayOutput,
		OnFinishedInput,
		AudioOutputs,
		BuilderResult,
		EMetaSoundOutputAudioFormat::Mono,
		true);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound source builder"));
	}
	EMetaSoundBuilderResult BuildResult = EMetaSoundBuilderResult::Failed;
	TScriptInterface<IMetaSoundDocumentInterface> BuiltMetaSound = UMetaSoundEditorSubsystem::GetChecked().BuildToAsset(Builder, FString(), AssetName, PackagePath, BuildResult);
	if (BuildResult != EMetaSoundBuilderResult::Succeeded || !BuiltMetaSound.GetObject())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound source asset"));
	}
	UObject* Asset = BuiltMetaSound.GetObject();
	Asset->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound source created"), MetaSoundSummary(Asset));
	AddChangedAsset(Result, Asset);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectMetaSound(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	if (!MetaSoundObject)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load MetaSound document asset"));
	}
	return FUnreelMCPResult::Success(TEXT("MetaSound inspected"), MetaSoundSummary(MetaSoundObject));
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddMetaSoundNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	const FMetasoundFrontendClassName ClassName = ReadMetaSoundClassName(Params);
	if (ClassName == FMetasoundFrontendClassName::InvalidClassName || ClassName.Name.IsNone())
	{
		return FUnreelMCPResult::Error(TEXT("Missing or invalid MetaSound class name"));
	}
	double MajorVersionNumber = 1;
	Params->TryGetNumberField(TEXT("major_version"), MajorVersionNumber);
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	FMetaSoundNodeHandle NodeHandle = Builder->AddNodeByClassName(ClassName, BuilderResult, static_cast<int32>(MajorVersionNumber));
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !NodeHandle.IsSet())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add MetaSound node"));
	}
#if WITH_EDITOR
	double X = 0.0;
	double Y = 0.0;
	if (Params->TryGetNumberField(TEXT("x"), X) || Params->TryGetNumberField(TEXT("y"), Y))
	{
		Builder->SetNodeLocation(NodeHandle, FVector2D(X, Y), BuilderResult);
	}
#endif
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound node added"), MetaSoundSummary(MetaSoundObject));
	Result.Data->SetStringField(TEXT("node"), NodeHandle.NodeID.ToString(EGuidFormats::DigitsWithHyphens));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddMetaSoundGraphInput(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	FString DataType;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("name"), Name) || !Params->TryGetStringField(TEXT("data_type"), DataType))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, name, or data_type"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	FMetasoundFrontendLiteral Literal;
	FString Error;
	if (!BuildMetaSoundLiteral(Params->TryGetField(TEXT("default_value")), DataType, Literal, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	const FMetaSoundBuilderNodeOutputHandle OutputHandle = Builder->AddGraphInputNode(FName(*Name), FName(*DataType), Literal, BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !OutputHandle.NodeID.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add MetaSound graph input"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound graph input added"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddMetaSoundGraphOutput(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	FString DataType;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("name"), Name) || !Params->TryGetStringField(TEXT("data_type"), DataType))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, name, or data_type"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	FMetasoundFrontendLiteral Literal;
	FString Error;
	if (!BuildMetaSoundLiteral(Params->TryGetField(TEXT("default_value")), DataType, Literal, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	const FMetaSoundBuilderNodeInputHandle InputHandle = Builder->AddGraphOutputNode(FName(*Name), FName(*DataType), Literal, BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !InputHandle.NodeID.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add MetaSound graph output"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound graph output added"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::ConnectMetaSoundNodes(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString FromOutput;
	FString ToInput;
	FGuid FromNode;
	FGuid ToNode;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("from_output"), FromOutput) || !Params->TryGetStringField(TEXT("to_input"), ToInput) || !ParseMetaSoundNodeGuid(Params, TEXT("from_node"), FromNode) || !ParseMetaSoundNodeGuid(Params, TEXT("to_node"), ToNode))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, from_node, from_output, to_node, or to_input"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	Builder->ConnectNodes(FMetaSoundNodeHandle(FromNode), FName(*FromOutput), FMetaSoundNodeHandle(ToNode), FName(*ToInput), BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to connect MetaSound nodes"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound nodes connected"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::DisconnectMetaSoundInput(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString InputName;
	FGuid NodeGuid;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("input"), InputName) || !ParseMetaSoundNodeGuid(Params, TEXT("node"), NodeGuid))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node, or input"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	const FMetaSoundBuilderNodeInputHandle InputHandle = Builder->FindNodeInputByName(FMetaSoundNodeHandle(NodeGuid), FName(*InputName), BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !InputHandle.VertexID.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find MetaSound node input"));
	}
	Builder->DisconnectNodeInput(InputHandle, BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to disconnect MetaSound input"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound input disconnected"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetMetaSoundNodeInputDefault(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString InputName;
	FString DataType;
	FGuid NodeGuid;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("input"), InputName) || !Params->TryGetStringField(TEXT("data_type"), DataType) || !Value.IsValid() || !ParseMetaSoundNodeGuid(Params, TEXT("node"), NodeGuid))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node, input, data_type, or value"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	FMetasoundFrontendLiteral Literal;
	FString Error;
	if (!BuildMetaSoundLiteral(Value, DataType, Literal, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	const FMetaSoundBuilderNodeInputHandle InputHandle = Builder->FindNodeInputByName(FMetaSoundNodeHandle(NodeGuid), FName(*InputName), BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded || !InputHandle.VertexID.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find MetaSound node input"));
	}
	Builder->SetNodeInputDefault(InputHandle, Literal, BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to set MetaSound node input default"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound input default set"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveMetaSoundNode(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FGuid NodeGuid;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !ParseMetaSoundNodeGuid(Params, TEXT("node"), NodeGuid))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or node"));
	}
	UObject* MetaSoundObject = LoadMetaSoundDocumentObject(AssetPath);
	UMetaSoundBuilderBase* Builder = CreateMetaSoundBuilder(MetaSoundObject);
	if (!Builder)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create MetaSound builder"));
	}
	EMetaSoundBuilderResult BuilderResult = EMetaSoundBuilderResult::Failed;
	Builder->RemoveNode(FMetaSoundNodeHandle(NodeGuid), BuilderResult);
	if (BuilderResult != EMetaSoundBuilderResult::Succeeded)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to remove MetaSound node"));
	}
	if (!BuildMetaSoundToAsset(Builder, MetaSoundObject))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to build MetaSound back to asset"));
	}
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("MetaSound node removed"), MetaSoundSummary(MetaSoundObject));
	AddChangedAsset(Result, MetaSoundObject);
	return Result;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::MetaSoundSummary(UObject* MetaSoundObject) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!MetaSoundObject)
	{
		return Data;
	}
	const IMetaSoundDocumentInterface* DocumentInterface = Cast<IMetaSoundDocumentInterface>(MetaSoundObject);
	if (!DocumentInterface)
	{
		return Data;
	}
	const FMetasoundFrontendDocument& Document = DocumentInterface->GetConstDocument();
	const FMetasoundFrontendGraph& Graph = Document.RootGraph.FindConstGraphChecked(Metasound::Frontend::DefaultPageID);
	Data->SetStringField(TEXT("asset"), MetaSoundObject->GetPathName());
	Data->SetStringField(TEXT("class_name"), Document.RootGraph.Metadata.GetClassName().GetFullName().ToString());
	Data->SetNumberField(TEXT("node_count"), Graph.Nodes.Num());
	Data->SetNumberField(TEXT("edge_count"), Graph.Edges.Num());

	TArray<TSharedPtr<FJsonValue>> NodeValues;
	for (const FMetasoundFrontendNode& Node : Graph.Nodes)
	{
		TSharedPtr<FJsonObject> NodeJson = MakeShared<FJsonObject>();
		NodeJson->SetStringField(TEXT("id"), Node.GetID().ToString(EGuidFormats::DigitsWithHyphens));
		NodeJson->SetStringField(TEXT("name"), Node.Name.ToString());
		NodeJson->SetStringField(TEXT("class_id"), Node.ClassID.ToString(EGuidFormats::DigitsWithHyphens));
		TArray<TSharedPtr<FJsonValue>> Inputs;
		for (const FMetasoundFrontendVertex& Input : Node.Interface.Inputs)
		{
			TSharedPtr<FJsonObject> InputJson = MakeShared<FJsonObject>();
			InputJson->SetStringField(TEXT("id"), Input.VertexID.ToString(EGuidFormats::DigitsWithHyphens));
			InputJson->SetStringField(TEXT("name"), Input.Name.ToString());
			InputJson->SetStringField(TEXT("type"), Input.TypeName.ToString());
			Inputs.Add(MakeShared<FJsonValueObject>(InputJson));
		}
		NodeJson->SetArrayField(TEXT("inputs"), Inputs);
		TArray<TSharedPtr<FJsonValue>> Outputs;
		for (const FMetasoundFrontendVertex& Output : Node.Interface.Outputs)
		{
			TSharedPtr<FJsonObject> OutputJson = MakeShared<FJsonObject>();
			OutputJson->SetStringField(TEXT("id"), Output.VertexID.ToString(EGuidFormats::DigitsWithHyphens));
			OutputJson->SetStringField(TEXT("name"), Output.Name.ToString());
			OutputJson->SetStringField(TEXT("type"), Output.TypeName.ToString());
			Outputs.Add(MakeShared<FJsonValueObject>(OutputJson));
		}
		NodeJson->SetArrayField(TEXT("outputs"), Outputs);
		NodeValues.Add(MakeShared<FJsonValueObject>(NodeJson));
	}
	Data->SetArrayField(TEXT("nodes"), NodeValues);

	TArray<TSharedPtr<FJsonValue>> EdgeValues;
	for (const FMetasoundFrontendEdge& Edge : Graph.Edges)
	{
		TSharedPtr<FJsonObject> EdgeJson = MakeShared<FJsonObject>();
		EdgeJson->SetStringField(TEXT("from_node"), Edge.FromNodeID.ToString(EGuidFormats::DigitsWithHyphens));
		EdgeJson->SetStringField(TEXT("from_vertex"), Edge.FromVertexID.ToString(EGuidFormats::DigitsWithHyphens));
		EdgeJson->SetStringField(TEXT("to_node"), Edge.ToNodeID.ToString(EGuidFormats::DigitsWithHyphens));
		EdgeJson->SetStringField(TEXT("to_vertex"), Edge.ToVertexID.ToString(EGuidFormats::DigitsWithHyphens));
		EdgeValues.Add(MakeShared<FJsonValueObject>(EdgeJson));
	}
	Data->SetArrayField(TEXT("edges"), EdgeValues);

	TArray<TSharedPtr<FJsonValue>> GraphInputs;
	for (const FMetasoundFrontendClassInput& Input : Document.RootGraph.GetDefaultInterface().Inputs)
	{
		TSharedPtr<FJsonObject> InputJson = MakeShared<FJsonObject>();
		InputJson->SetStringField(TEXT("name"), Input.Name.ToString());
		InputJson->SetStringField(TEXT("type"), Input.TypeName.ToString());
		GraphInputs.Add(MakeShared<FJsonValueObject>(InputJson));
	}
	Data->SetArrayField(TEXT("graph_inputs"), GraphInputs);

	TArray<TSharedPtr<FJsonValue>> GraphOutputs;
	for (const FMetasoundFrontendClassOutput& Output : Document.RootGraph.GetDefaultInterface().Outputs)
	{
		TSharedPtr<FJsonObject> OutputJson = MakeShared<FJsonObject>();
		OutputJson->SetStringField(TEXT("name"), Output.Name.ToString());
		OutputJson->SetStringField(TEXT("type"), Output.TypeName.ToString());
		GraphOutputs.Add(MakeShared<FJsonValueObject>(OutputJson));
	}
	Data->SetArrayField(TEXT("graph_outputs"), GraphOutputs);
	return Data;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::NiagaraStackSummary(UNiagaraSystem* System, UNiagaraScript* Script, UNiagaraNodeOutput* OutputNode, const FString& EmitterName) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!System || !Script || !OutputNode)
	{
		return Data;
	}
	Data->SetStringField(TEXT("asset"), System->GetPathName());
	Data->SetStringField(TEXT("usage"), NiagaraUsageName(OutputNode->GetUsage()));
	Data->SetStringField(TEXT("usage_id"), OutputNode->GetUsageId().ToString(EGuidFormats::DigitsWithHyphens));
	Data->SetStringField(TEXT("emitter"), EmitterName);
	Data->SetStringField(TEXT("script"), Script->GetPathName());

	TArray<UNiagaraNodeFunctionCall*> Modules;
	NiagaraOrderedModules(*OutputNode, Modules);
	Data->SetNumberField(TEXT("module_count"), Modules.Num());
	TArray<TSharedPtr<FJsonValue>> ModuleValues;
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		UNiagaraNodeFunctionCall* Module = Modules[Index];
		TSharedPtr<FJsonObject> ModuleJson = MakeShared<FJsonObject>();
		ModuleJson->SetNumberField(TEXT("index"), Index);
		ModuleJson->SetStringField(TEXT("id"), Module ? Module->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString());
		ModuleJson->SetStringField(TEXT("name"), Module ? Module->GetName() : FString());
		ModuleJson->SetStringField(TEXT("function_name"), Module ? Module->GetFunctionName() : FString());
		ModuleJson->SetBoolField(TEXT("enabled"), Module ? Module->IsNodeEnabled() : false);
		if (Module && Module->FunctionScript)
		{
			ModuleJson->SetStringField(TEXT("function_script"), Module->FunctionScript->GetPathName());
			ModuleJson->SetStringField(TEXT("selected_version"), Module->SelectedScriptVersion.ToString(EGuidFormats::DigitsWithHyphens));
		}
		TArray<TSharedPtr<FJsonValue>> Inputs;
		if (Module)
		{
			TArray<UEdGraphPin*> InputPins;
			Module->GetInputPins(InputPins);
			for (UEdGraphPin* InputPin : InputPins)
			{
				if (!InputPin || InputPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc)
				{
					continue;
				}
				TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
				PinJson->SetStringField(TEXT("name"), InputPin->PinName.ToString());
				PinJson->SetStringField(TEXT("default_value"), InputPin->DefaultValue);
				PinJson->SetNumberField(TEXT("link_count"), InputPin->LinkedTo.Num());
				Inputs.Add(MakeShared<FJsonValueObject>(PinJson));
			}
		}
		ModuleJson->SetArrayField(TEXT("inputs"), Inputs);
		ModuleValues.Add(MakeShared<FJsonValueObject>(ModuleJson));
	}
	Data->SetArrayField(TEXT("modules"), ModuleValues);
	return Data;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectNiagaraStack(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	return FUnreelMCPResult::Success(TEXT("Niagara stack inspected"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddNiagaraScriptModule(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FString ModuleScriptPath;
	if (!Params->TryGetStringField(TEXT("module_script"), ModuleScriptPath) || ModuleScriptPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: module_script"));
	}
	UNiagaraScript* ModuleScript = Cast<UNiagaraScript>(LoadAssetObject(ModuleScriptPath));
	if (!ModuleScript)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Niagara module script"));
	}
	int32 Index = INDEX_NONE;
	Params->TryGetNumberField(TEXT("index"), Index);
	FString SuggestedName;
	Params->TryGetStringField(TEXT("name"), SuggestedName);
	FString VersionGuidText;
	FGuid VersionGuid;
	if (Params->TryGetStringField(TEXT("version_guid"), VersionGuidText))
	{
		FGuid::Parse(VersionGuidText, VersionGuid);
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddNiagaraScriptModule", "Add Niagara Script Module"));
	Stack.System->Modify();
	Stack.Script->Modify();
	UNiagaraNodeFunctionCall* AddedModule = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScript, *Stack.OutputNode, Index, SuggestedName, VersionGuid);
	if (!AddedModule)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add Niagara script module"));
	}
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara module added"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetStringField(TEXT("added_module"), AddedModule->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	AddChangedAsset(Result, Stack.System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddNiagaraParameterModule(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FString ParameterName;
	if (!Params->TryGetStringField(TEXT("parameter"), ParameterName) || ParameterName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: parameter"));
	}
	FString TypeName = TEXT("float");
	Params->TryGetStringField(TEXT("type"), TypeName);
	FString DefaultValue = TEXT("0.0");
	Params->TryGetStringField(TEXT("default_value"), DefaultValue);
	int32 Index = INDEX_NONE;
	Params->TryGetNumberField(TEXT("index"), Index);

	TArray<FNiagaraVariable> Variables;
	Variables.Add(FNiagaraVariable(ParseNiagaraTypeDefinition(TypeName), FName(*ParameterName)));
	TArray<FString> DefaultValues;
	DefaultValues.Add(DefaultValue);

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddNiagaraParameterModule", "Add Niagara Parameter Module"));
	Stack.System->Modify();
	Stack.Script->Modify();
	UNiagaraNodeAssignment* AddedModule = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *Stack.OutputNode, Index, DefaultValues);
	if (!AddedModule)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add Niagara parameter module"));
	}
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara parameter module added"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetStringField(TEXT("added_module"), AddedModule->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
	AddChangedAsset(Result, Stack.System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveNiagaraModule(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	int32 Index = INDEX_NONE;
	UNiagaraNodeFunctionCall* Module = FindNiagaraModule(Stack.OutputNode, Params, Index);
	if (!Module)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Niagara module"));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveNiagaraModule", "Remove Niagara Module"));
	Stack.System->Modify();
	Stack.Script->Modify();
	UNiagaraGraph* Graph = Module->GetNiagaraGraph();
	Module->Modify();
	TArray<UNiagaraNode*> RemovedNodes;
	if (!RemoveNiagaraModuleFromChain(Module, RemovedNodes))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to disconnect Niagara module from stack"));
	}
	if (Graph)
	{
		Graph->Modify();
		for (UNiagaraNode* RemovedNode : RemovedNodes)
		{
			if (RemovedNode)
			{
				RemovedNode->Modify();
				Graph->RemoveNode(RemovedNode);
			}
		}
	}
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara module removed"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetNumberField(TEXT("removed_index"), Index);
	AddChangedAsset(Result, Stack.System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::MoveNiagaraModule(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	int32 TargetIndex = INDEX_NONE;
	if (!Params->TryGetNumberField(TEXT("target_index"), TargetIndex) || TargetIndex < 0)
	{
		return FUnreelMCPResult::Error(TEXT("Missing or invalid target_index"));
	}
	int32 SourceIndex = INDEX_NONE;
	UNiagaraNodeFunctionCall* Module = FindNiagaraModule(Stack.OutputNode, Params, SourceIndex);
	if (!Module)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Niagara module"));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "MoveNiagaraModule", "Move Niagara Module"));
	Stack.System->Modify();
	Stack.Script->Modify();
	Module->Modify();
	FNiagaraModuleChainGroup ModuleGroup;
	if (!ResolveNiagaraModuleChainGroup(Module, ModuleGroup)
		|| !DisconnectNiagaraModuleGroup(ModuleGroup)
		|| !InsertNiagaraModuleIntoChain(Stack.OutputNode, ModuleGroup, TargetIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to reorder Niagara module"));
	}
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara module moved"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetNumberField(TEXT("source_index"), SourceIndex);
	Result.Data->SetNumberField(TEXT("target_index"), TargetIndex);
	AddChangedAsset(Result, Stack.System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetNiagaraModuleEnabled(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	bool bEnabled = true;
	if (!Params->TryGetBoolField(TEXT("enabled"), bEnabled))
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: enabled"));
	}
	int32 Index = INDEX_NONE;
	UNiagaraNodeFunctionCall* Module = FindNiagaraModule(Stack.OutputNode, Params, Index);
	if (!Module)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Niagara module"));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetNiagaraModuleEnabled", "Set Niagara Module Enabled"));
	Stack.System->Modify();
	Stack.Script->Modify();
	FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Module, bEnabled);
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara module enabled state changed"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetNumberField(TEXT("module_index"), Index);
	Result.Data->SetBoolField(TEXT("enabled"), bEnabled);
	AddChangedAsset(Result, Stack.System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::SetNiagaraModuleInputDefault(const TSharedRef<FJsonObject>& Params) const
{
	FResolvedNiagaraStack Stack;
	FString Error;
	if (!ResolveNiagaraStack(Params, Stack, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FString InputName;
	FString DefaultValue;
	if (!Params->TryGetStringField(TEXT("input"), InputName) || InputName.IsEmpty() || !Params->TryGetStringField(TEXT("default_value"), DefaultValue))
	{
		return FUnreelMCPResult::Error(TEXT("Missing required input or default_value"));
	}
	FString TypeName = TEXT("float");
	Params->TryGetStringField(TEXT("type"), TypeName);
	int32 Index = INDEX_NONE;
	UNiagaraNodeFunctionCall* Module = FindNiagaraModule(Stack.OutputNode, Params, Index);
	if (!Module)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Niagara module"));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetNiagaraModuleInputDefault", "Set Niagara Module Input Default"));
	Stack.System->Modify();
	Stack.Script->Modify();
	FNiagaraParameterHandle ModuleHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(FName(*InputName));
	FNiagaraParameterHandle AliasedHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, Module);
	UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Module, AliasedHandle, ParseNiagaraTypeDefinition(TypeName), FGuid(), FGuid());
	if (const UEdGraphSchema_Niagara* Schema = Cast<UEdGraphSchema_Niagara>(OverridePin.GetSchema()))
	{
		Schema->TrySetDefaultValue(OverridePin, DefaultValue, true);
	}
	else
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Niagara graph schema for override pin"));
	}
	FinalizeNiagaraEdit(Stack);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara module input default set"), NiagaraStackSummary(Stack.System, Stack.Script, Stack.OutputNode, Stack.EmitterName));
	Result.Data->SetNumberField(TEXT("module_index"), Index);
	Result.Data->SetStringField(TEXT("input"), InputName);
	AddChangedAsset(Result, Stack.System);
	return Result;
}

TSharedPtr<FJsonObject> FUnreelMCPAuthoringService::NiagaraScratchPadSummary(UNiagaraSystem* System) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!System)
	{
		return Data;
	}
	Data->SetStringField(TEXT("asset"), System->GetPathName());
	TArray<TSharedPtr<FJsonValue>> SystemScripts;
	for (int32 Index = 0; Index < System->ScratchPadScripts.Num(); ++Index)
	{
		UNiagaraScript* Script = System->ScratchPadScripts[Index];
		TSharedPtr<FJsonObject> ScriptJson = MakeShared<FJsonObject>();
		ScriptJson->SetNumberField(TEXT("index"), Index);
		ScriptJson->SetStringField(TEXT("name"), Script ? Script->GetName() : FString());
		ScriptJson->SetStringField(TEXT("path"), Script ? Script->GetPathName() : FString());
		ScriptJson->SetStringField(TEXT("usage"), Script ? NiagaraUsageName(Script->GetUsage()) : FString());
		SystemScripts.Add(MakeShared<FJsonValueObject>(ScriptJson));
	}
	Data->SetArrayField(TEXT("system_scripts"), SystemScripts);
	Data->SetNumberField(TEXT("system_script_count"), System->ScratchPadScripts.Num());

	TArray<TSharedPtr<FJsonValue>> Emitters;
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
		EmitterJson->SetStringField(TEXT("name"), Handle.GetName().ToString());
		EmitterJson->SetStringField(TEXT("id"), Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens));
		TArray<TSharedPtr<FJsonValue>> Scripts;
		if (EmitterData && EmitterData->ScratchPads)
		{
			for (int32 Index = 0; Index < EmitterData->ScratchPads->Scripts.Num(); ++Index)
			{
				UNiagaraScript* Script = EmitterData->ScratchPads->Scripts[Index];
				TSharedPtr<FJsonObject> ScriptJson = MakeShared<FJsonObject>();
				ScriptJson->SetNumberField(TEXT("index"), Index);
				ScriptJson->SetStringField(TEXT("name"), Script ? Script->GetName() : FString());
				ScriptJson->SetStringField(TEXT("path"), Script ? Script->GetPathName() : FString());
				ScriptJson->SetStringField(TEXT("usage"), Script ? NiagaraUsageName(Script->GetUsage()) : FString());
				Scripts.Add(MakeShared<FJsonValueObject>(ScriptJson));
			}
			EmitterJson->SetNumberField(TEXT("script_count"), EmitterData->ScratchPads->Scripts.Num());
		}
		else
		{
			EmitterJson->SetNumberField(TEXT("script_count"), 0);
		}
		EmitterJson->SetArrayField(TEXT("scripts"), Scripts);
		Emitters.Add(MakeShared<FJsonValueObject>(EmitterJson));
	}
	Data->SetArrayField(TEXT("emitters"), Emitters);
	return Data;
}

FUnreelMCPResult FUnreelMCPAuthoringService::InspectNiagaraScratchPad(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UNiagaraSystem* System = Cast<UNiagaraSystem>(LoadAssetObject(AssetPath));
	if (!System)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Niagara System asset"));
	}
	return FUnreelMCPResult::Success(TEXT("Niagara scratch pad inspected"), NiagaraScratchPadSummary(System));
}

FUnreelMCPResult FUnreelMCPAuthoringService::AddNiagaraScratchPadScript(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString SourceScriptPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("source_script"), SourceScriptPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing required asset_path or source_script"));
	}
	UNiagaraSystem* System = Cast<UNiagaraSystem>(LoadAssetObject(AssetPath));
	UNiagaraScript* SourceScript = Cast<UNiagaraScript>(LoadAssetObject(SourceScriptPath));
	if (!System || !SourceScript)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Niagara System or source script"));
	}
	FString NewName = SourceScript->GetName() + TEXT("_Scratch");
	Params->TryGetStringField(TEXT("name"), NewName);
	FString EmitterIdentifier;
	Params->TryGetStringField(TEXT("emitter"), EmitterIdentifier);
	FVersionedNiagaraEmitterData* TargetEmitterData = nullptr;
	if (!EmitterIdentifier.IsEmpty())
	{
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (EmitterIdentifier == Handle.GetName().ToString() || EmitterIdentifier == Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens) || EmitterIdentifier == Handle.GetUniqueInstanceName())
			{
				TargetEmitterData = Handle.GetEmitterData();
				break;
			}
		}
		if (!TargetEmitterData)
		{
			return FUnreelMCPResult::Error(TEXT("Failed to resolve target emitter"));
		}
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddNiagaraScratchPadScript", "Add Niagara Scratch Pad Script"));
	System->Modify();
	UNiagaraScript* NewScript = Cast<UNiagaraScript>(StaticDuplicateObject(SourceScript, System, FName(*NewName)));
	if (!NewScript)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to duplicate Niagara scratch pad script"));
	}
	NewScript->SetFlags(RF_Transactional);
	NewScript->Modify();
	if (EmitterIdentifier.IsEmpty())
	{
		System->ScratchPadScripts.Add(NewScript);
	}
	else
	{
		if (!TargetEmitterData->ScratchPads)
		{
			TargetEmitterData->ScratchPads = NewObject<UNiagaraScratchPadContainer>(System, NAME_None, RF_Transactional);
		}
		TargetEmitterData->ScratchPads->Modify();
		TargetEmitterData->ScratchPads->Scripts.Add(NewScript);
	}
	System->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara scratch pad script added"), NiagaraScratchPadSummary(System));
	Result.Data->SetStringField(TEXT("added_script"), NewScript->GetPathName());
	AddChangedAsset(Result, System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::RemoveNiagaraScratchPadScript(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UNiagaraSystem* System = Cast<UNiagaraSystem>(LoadAssetObject(AssetPath));
	if (!System)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Niagara System asset"));
	}
	FString EmitterIdentifier;
	Params->TryGetStringField(TEXT("emitter"), EmitterIdentifier);
	TArray<TObjectPtr<UNiagaraScript>>* Scripts = &System->ScratchPadScripts;
	if (!EmitterIdentifier.IsEmpty())
	{
		Scripts = nullptr;
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (EmitterIdentifier == Handle.GetName().ToString() || EmitterIdentifier == Handle.GetId().ToString(EGuidFormats::DigitsWithHyphens) || EmitterIdentifier == Handle.GetUniqueInstanceName())
			{
				FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
				if (EmitterData && EmitterData->ScratchPads)
				{
					Scripts = &EmitterData->ScratchPads->Scripts;
				}
				break;
			}
		}
	}
	if (!Scripts)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve scratch pad script list"));
	}
	int32 Index = INDEX_NONE;
	Params->TryGetNumberField(TEXT("index"), Index);
	FString ScriptIdentifier;
	Params->TryGetStringField(TEXT("script"), ScriptIdentifier);
	if (!Scripts->IsValidIndex(Index))
	{
		Index = Scripts->IndexOfByPredicate([&ScriptIdentifier](const TObjectPtr<UNiagaraScript>& Script)
		{
			return Script && (ScriptIdentifier == Script->GetName() || ScriptIdentifier == Script->GetPathName());
		});
	}
	if (!Scripts->IsValidIndex(Index))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve scratch pad script"));
	}

	const FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveNiagaraScratchPadScript", "Remove Niagara Scratch Pad Script"));
	System->Modify();
	UNiagaraScript* RemovedScript = (*Scripts)[Index].Get();
	Scripts->RemoveAt(Index);
	if (RemovedScript)
	{
		RemovedScript->Modify();
		RemovedScript->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
	}
	System->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Niagara scratch pad script removed"), NiagaraScratchPadSummary(System));
	Result.Data->SetNumberField(TEXT("removed_index"), Index);
	AddChangedAsset(Result, System);
	return Result;
}

FUnreelMCPResult FUnreelMCPAuthoringService::UnsupportedBestEffort(const FString& OperationId, const FString& Reason) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("operation_id"), OperationId);
	Data->SetStringField(TEXT("status"), TEXT("best_effort_unsupported"));
	Data->SetStringField(TEXT("reason"), Reason);
	FUnreelMCPResult Result = FUnreelMCPResult::Error(Reason);
	Result.Data = Data;
	Result.Warnings.Add(TEXT("This operation is intentionally exposed as a limitation marker so agents do not repeatedly attempt private editor workflows."));
	return Result;
}
