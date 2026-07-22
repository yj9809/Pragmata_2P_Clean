#include "MCP/Services/UnreelMCPParityService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "Algo/Count.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"
#include "AnimationGraph.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimStateAliasNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "Components/ActorComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "K2Node_AddComponentByClass.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunctionInterface.h"
#include "MaterialEditingLibrary.h"
#include "Engine/Texture.h"
#include "MCP/Services/UnreelMCPTransactionService.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/FieldIterator.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace
{
	FUnreelMCPParameter ParityParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FUnreelMCPOperation ParityOp(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FUnreelMCPParameter>& Params, bool bReadOnly, EUnreelMCPRisk Risk, const FString& Example)
	{
		FUnreelMCPOperation Op;
		Op.Id = Id;
		Op.Domain = Domain;
		Op.Operation = Operation;
		Op.Description = Description;
		Op.Keywords = { Domain, Operation, TEXT("authoring"), TEXT("parity") };
		Op.Parameters = Params;
		Op.bReadOnly = bReadOnly;
		Op.bRequiresTransaction = !bReadOnly;
		Op.Risk = Risk;
		Op.ExampleJson = Example;
		return Op;
	}

	FString NodeId(const UEdGraphNode* Node)
	{
		return Node ? Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens) : FString();
	}

	int32 ReadInt(const TSharedRef<FJsonObject>& Params, const FString& Field, int32 DefaultValue)
	{
		double Number = DefaultValue;
		Params->TryGetNumberField(Field, Number);
		return static_cast<int32>(Number);
	}

	int32 FindNotifyTrackIndex(const UAnimSequenceBase* Asset, FName TrackName)
	{
#if WITH_EDITORONLY_DATA
		if (!Asset)
		{
			return INDEX_NONE;
		}
		return Asset->AnimNotifyTracks.IndexOfByPredicate([&](const FAnimNotifyTrack& Track)
		{
			return Track.TrackName == TrackName;
		});
#else
		return INDEX_NONE;
#endif
	}

	int32 EnsureNotifyTrack(UAnimSequenceBase* Asset, FName TrackName)
	{
		if (!Asset)
		{
			return INDEX_NONE;
		}
#if WITH_EDITOR
		if (Asset->AnimNotifyTracks.Num() == 0)
		{
			Asset->InitializeNotifyTrack();
		}
#endif
		int32 TrackIndex = FindNotifyTrackIndex(Asset, TrackName);
#if WITH_EDITORONLY_DATA
		if (TrackIndex == INDEX_NONE)
		{
			FAnimNotifyTrack NewTrack;
			NewTrack.TrackName = TrackName;
			NewTrack.TrackColor = FLinearColor::White;
			TrackIndex = Asset->AnimNotifyTracks.Add(NewTrack);
		}
#endif
		return TrackIndex;
	}

	bool IsValidNotifyTime(const UAnimSequenceBase* Asset, double Time)
	{
		return Asset && Time >= 0.0 && Time <= static_cast<double>(Asset->GetPlayLength());
	}

	FAnimNotifyEvent* ResolveNotifyEvent(UAnimSequenceBase* Asset, const TSharedRef<FJsonObject>& Params, int32& OutIndex)
	{
		OutIndex = INDEX_NONE;
		if (!Asset)
		{
			return nullptr;
		}
		double IndexNumber = -1;
		if (Params->TryGetNumberField(TEXT("index"), IndexNumber))
		{
			OutIndex = static_cast<int32>(IndexNumber);
			return Asset->Notifies.IsValidIndex(OutIndex) ? &Asset->Notifies[OutIndex] : nullptr;
		}
		FString GuidString;
		if (Params->TryGetStringField(TEXT("guid"), GuidString))
		{
			FGuid Guid;
			if (!FGuid::Parse(GuidString, Guid))
			{
				return nullptr;
			}
			for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
			{
				if (Asset->Notifies[Index].Guid == Guid)
				{
					OutIndex = Index;
					return &Asset->Notifies[Index];
				}
			}
		}
		return nullptr;
	}

	int32 FindNotifyIndexByGuid(const UAnimSequenceBase* Asset, const FGuid& Guid)
	{
		if (!Asset)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
		{
			if (Asset->Notifies[Index].Guid == Guid)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FindNotifyIndexBySignature(const UAnimSequenceBase* Asset, const FName NotifyName, const UClass* NotifyClass, const UClass* NotifyStateClass, double Time)
	{
		if (!Asset)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
		{
			const FAnimNotifyEvent& Event = Asset->Notifies[Index];
			const bool bNameMatches = NotifyName.IsNone() || Event.NotifyName == NotifyName;
			const bool bClassMatches = (!NotifyClass && !NotifyStateClass)
				|| (NotifyClass && Event.Notify && Event.Notify->IsA(NotifyClass))
				|| (NotifyStateClass && Event.NotifyStateClass && Event.NotifyStateClass->IsA(NotifyStateClass));
			if (bNameMatches && bClassMatches && FMath::IsNearlyEqual(Event.GetTime(), static_cast<float>(Time), KINDA_SMALL_NUMBER))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 FindMontageSlotIndex(const UAnimMontage* Montage, FName SlotName)
	{
		if (!Montage)
		{
			return INDEX_NONE;
		}
		return Montage->SlotAnimTracks.IndexOfByPredicate([&](const FSlotAnimationTrack& Track)
		{
			return Track.SlotName == SlotName;
		});
	}

	FSlotAnimationTrack* ResolveMontageSlot(UAnimMontage* Montage, const TSharedRef<FJsonObject>& Params, int32& OutSlotIndex)
	{
		OutSlotIndex = INDEX_NONE;
		if (!Montage)
		{
			return nullptr;
		}
		double SlotIndexNumber = -1;
		if (Params->TryGetNumberField(TEXT("slot_index"), SlotIndexNumber))
		{
			OutSlotIndex = static_cast<int32>(SlotIndexNumber);
		}
		else
		{
			FString SlotName;
			if (Params->TryGetStringField(TEXT("slot_name"), SlotName) && !SlotName.IsEmpty())
			{
				OutSlotIndex = FindMontageSlotIndex(Montage, FName(*SlotName));
			}
		}
		return Montage->SlotAnimTracks.IsValidIndex(OutSlotIndex) ? &Montage->SlotAnimTracks[OutSlotIndex] : nullptr;
	}

	int32 ResolveMontageSectionIndex(const UAnimMontage* Montage, const TSharedRef<FJsonObject>& Params)
	{
		if (!Montage)
		{
			return INDEX_NONE;
		}
		double SectionIndexNumber = -1;
		if (Params->TryGetNumberField(TEXT("section_index"), SectionIndexNumber))
		{
			const int32 SectionIndex = static_cast<int32>(SectionIndexNumber);
			return Montage->CompositeSections.IsValidIndex(SectionIndex) ? SectionIndex : INDEX_NONE;
		}
		FString SectionName;
		if (Params->TryGetStringField(TEXT("section_name"), SectionName) && !SectionName.IsEmpty())
		{
			return Montage->GetSectionIndex(FName(*SectionName));
		}
		return INDEX_NONE;
	}

	FAnimSegment* ResolveMontageSegment(UAnimMontage* Montage, const TSharedRef<FJsonObject>& Params, int32& OutSlotIndex, int32& OutSegmentIndex)
	{
		OutSlotIndex = INDEX_NONE;
		OutSegmentIndex = INDEX_NONE;
		FSlotAnimationTrack* Slot = ResolveMontageSlot(Montage, Params, OutSlotIndex);
		if (!Slot)
		{
			return nullptr;
		}
		double SegmentIndexNumber = -1;
		if (!Params->TryGetNumberField(TEXT("segment_index"), SegmentIndexNumber))
		{
			return nullptr;
		}
		OutSegmentIndex = static_cast<int32>(SegmentIndexNumber);
		return Slot->AnimTrack.AnimSegments.IsValidIndex(OutSegmentIndex) ? &Slot->AnimTrack.AnimSegments[OutSegmentIndex] : nullptr;
	}

	bool IsValidMontageRange(double StartPos, double Length)
	{
		return StartPos >= 0.0 && Length >= 0.0;
	}

	bool IsPosePin(const UEdGraphPin* Pin)
	{
		if (!Pin || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			return false;
		}
		const UObject* SubCategoryObject = Pin->PinType.PinSubCategoryObject.Get();
		const FString SubCategoryPath = SubCategoryObject ? SubCategoryObject->GetPathName() : FString();
		return SubCategoryPath.Contains(TEXT("PoseLink")) || SubCategoryPath.Contains(TEXT("ComponentSpacePoseLink"));
	}

	FString GraphType(const UEdGraph* Graph)
	{
		if (Cast<UAnimationStateMachineGraph>(Graph))
		{
			return TEXT("state_machine");
		}
		if (Cast<UAnimationGraph>(Graph))
		{
			return TEXT("anim_graph");
		}
		const FString GraphClass = Graph ? Graph->GetClass()->GetName() : FString();
		const FString SchemaClass = Graph && Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetName() : FString();
		if (GraphClass.Contains(TEXT("Transition")) || SchemaClass.Contains(TEXT("Transition")))
		{
			return TEXT("transition");
		}
		if (GraphClass.Contains(TEXT("Animation")) || SchemaClass.Contains(TEXT("Animation")) || GraphClass.Contains(TEXT("Anim")) || SchemaClass.Contains(TEXT("Anim")))
		{
			return TEXT("anim_related");
		}
		return TEXT("other");
	}
}

FString FUnreelMCPParityService::GetServiceName() const
{
	return TEXT("parity");
}

TArray<FUnreelMCPOperation> FUnreelMCPParityService::GetOperations() const
{
	return {
		ParityOp(TEXT("blueprint.graph.list"), TEXT("blueprint"), TEXT("graph.list"), TEXT("Inspect Blueprint graphs and nodes."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"blueprint.graph.list\",\"params\":{\"asset_path\":\"/Game/BP_Test\"}}")),
		ParityOp(TEXT("blueprint.function.create"), TEXT("blueprint"), TEXT("function.create"), TEXT("Create a Blueprint function graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("name"), TEXT("string"), TEXT("Function graph name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.function.create\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"name\":\"DoWork\"}}")),
		ParityOp(TEXT("blueprint.graph.add_event"), TEXT("blueprint"), TEXT("graph.add_event"), TEXT("Add a custom event node to a Blueprint graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("event_name"), TEXT("string"), TEXT("Custom event name."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false, TEXT("0")), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false, TEXT("0")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_event\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"event_name\":\"MCP_Event\"}}")),
		ParityOp(TEXT("blueprint.graph.add_function_call"), TEXT("blueprint"), TEXT("graph.add_function_call"), TEXT("Add a function call node to a Blueprint graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Class containing the function."), true), ParityParam(TEXT("function_name"), TEXT("string"), TEXT("Function name."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false, TEXT("300")), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false, TEXT("0")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_function_call\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"class_path\":\"/Script/Engine.KismetSystemLibrary\",\"function_name\":\"PrintString\"}}")),
		ParityOp(TEXT("blueprint.graph.add_variable_get"), TEXT("blueprint"), TEXT("graph.add_variable_get"), TEXT("Add a variable getter node to a Blueprint graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("variable_name"), TEXT("string"), TEXT("Member variable name."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_variable_get\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"variable_name\":\"Health\"}}")),
		ParityOp(TEXT("blueprint.graph.add_variable_set"), TEXT("blueprint"), TEXT("graph.add_variable_set"), TEXT("Add a variable setter node to a Blueprint graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("variable_name"), TEXT("string"), TEXT("Member variable name."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_variable_set\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"variable_name\":\"Health\"}}")),
		ParityOp(TEXT("blueprint.graph.add_node_by_class"), TEXT("blueprint"), TEXT("graph.add_node_by_class"), TEXT("Add an arbitrary UEdGraphNode/UK2Node subclass to a Blueprint graph using Unreal node lifecycle calls."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Node class path, e.g. /Script/BlueprintGraph.K2Node_IfThenElse."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected node properties applied before pin allocation."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_node_by_class\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"class_path\":\"/Script/BlueprintGraph.K2Node_IfThenElse\"}}")),
		ParityOp(TEXT("blueprint.graph.add_common_node"), TEXT("blueprint"), TEXT("graph.add_common_node"), TEXT("Add a commonly used Blueprint node with required setup before pin allocation. Supported kind values: branch, sequence, dynamic_cast, select, timeline, add_component_by_class, add_delegate, assign_delegate, macro_instance."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("kind"), TEXT("string"), TEXT("branch, sequence, dynamic_cast, select, timeline, add_component_by_class, add_delegate, assign_delegate, or macro_instance."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false), ParityParam(TEXT("target_class_path"), TEXT("string"), TEXT("Target class for dynamic_cast."), false), ParityParam(TEXT("component_class_path"), TEXT("string"), TEXT("Component class for add_component_by_class."), false), ParityParam(TEXT("delegate_name"), TEXT("string"), TEXT("Multicast delegate property name for add_delegate or assign_delegate."), false), ParityParam(TEXT("owner_class_path"), TEXT("string"), TEXT("Owner class for delegate lookup."), false), ParityParam(TEXT("timeline_name"), TEXT("string"), TEXT("Timeline variable name."), false), ParityParam(TEXT("macro_graph_name"), TEXT("string"), TEXT("Macro graph to create or instance."), false), ParityParam(TEXT("enum_path"), TEXT("string"), TEXT("Optional enum path for select."), false), ParityParam(TEXT("option_count"), TEXT("integer"), TEXT("Minimum option pins for select."), false, TEXT("2")), ParityParam(TEXT("then_pins"), TEXT("integer"), TEXT("Minimum then pins for sequence."), false, TEXT("2")), ParityParam(TEXT("pure"), TEXT("boolean"), TEXT("Use pure dynamic_cast pins."), false, TEXT("false")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.add_common_node\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"kind\":\"dynamic_cast\",\"target_class_path\":\"/Script/Engine.Actor\"}}")),
		ParityOp(TEXT("blueprint.graph.set_node_property"), TEXT("blueprint"), TEXT("graph.set_node_property"), TEXT("Set a reflected property on an existing Blueprint graph node and reconstruct it."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("node"), TEXT("string"), TEXT("Node GUID or object name."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through Unreal import text."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.set_node_property\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"node\":\"...\",\"property\":\"NodeComment\",\"value\":\"Created by MCP\"}}")),
		ParityOp(TEXT("blueprint.graph.set_pin_default"), TEXT("blueprint"), TEXT("graph.set_pin_default"), TEXT("Set a Blueprint pin default value/object/text through the graph schema."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("node"), TEXT("string"), TEXT("Node GUID or object name."), true), ParityParam(TEXT("pin"), TEXT("string"), TEXT("Pin name."), true), ParityParam(TEXT("direction"), TEXT("string"), TEXT("input, output, or empty for any."), false), ParityParam(TEXT("default_value"), TEXT("string"), TEXT("Default value string."), false), ParityParam(TEXT("default_object"), TEXT("string"), TEXT("Object path for object/class defaults."), false), ParityParam(TEXT("default_text"), TEXT("string"), TEXT("Text default."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.set_pin_default\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"node\":\"...\",\"pin\":\"Condition\",\"default_value\":\"true\"}}")),
		ParityOp(TEXT("blueprint.graph.connect_pins"), TEXT("blueprint"), TEXT("graph.connect_pins"), TEXT("Connect two pins in a Blueprint graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false), ParityParam(TEXT("from_node"), TEXT("string"), TEXT("Source node GUID."), true), ParityParam(TEXT("from_pin"), TEXT("string"), TEXT("Source pin name."), true), ParityParam(TEXT("to_node"), TEXT("string"), TEXT("Target node GUID."), true), ParityParam(TEXT("to_pin"), TEXT("string"), TEXT("Target pin name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blueprint.graph.connect_pins\",\"params\":{\"asset_path\":\"/Game/BP_Test\",\"from_node\":\"...\",\"from_pin\":\"then\",\"to_node\":\"...\",\"to_pin\":\"execute\"}}")),
		ParityOp(TEXT("blueprint.graph.layout"), TEXT("blueprint"), TEXT("graph.layout"), TEXT("Apply a simple grid layout to Blueprint graph nodes."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name."), false) }, false, EUnreelMCPRisk::Low, TEXT("{\"operation_id\":\"blueprint.graph.layout\",\"params\":{\"asset_path\":\"/Game/BP_Test\"}}")),

		ParityOp(TEXT("material.graph.list"), TEXT("material"), TEXT("graph.list"), TEXT("Inspect material expressions."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"material.graph.list\",\"params\":{\"asset_path\":\"/Game/M_Test\"}}")),
		ParityOp(TEXT("material.graph.add_expression"), TEXT("material"), TEXT("graph.add_expression"), TEXT("Add a material expression node."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Material expression class path."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected property values."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.add_expression\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"class_path\":\"/Script/Engine.MaterialExpressionConstant3Vector\"}}")),
		ParityOp(TEXT("material.graph.add_parameter"), TEXT("material"), TEXT("graph.add_parameter"), TEXT("Add scalar, vector, or texture parameter expression nodes with parameter name and default value setup."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("type"), TEXT("string"), TEXT("scalar, vector, or texture."), true), ParityParam(TEXT("parameter"), TEXT("string"), TEXT("Parameter name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("Scalar number, vector {r,g,b,a}, or texture asset path."), false), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.add_parameter\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"type\":\"scalar\",\"parameter\":\"Roughness\",\"value\":0.5}}")),
		ParityOp(TEXT("material.graph.set_texture"), TEXT("material"), TEXT("graph.set_texture"), TEXT("Assign a texture asset to a texture sample or texture parameter expression."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("expression_index"), TEXT("integer"), TEXT("Texture expression index."), true), ParityParam(TEXT("texture"), TEXT("string"), TEXT("Texture asset path."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.set_texture\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"expression_index\":0,\"texture\":\"/Engine/EngineResources/DefaultTexture\"}}")),
		ParityOp(TEXT("material.graph.add_function_call"), TEXT("material"), TEXT("graph.add_function_call"), TEXT("Add a material function call expression and refresh its function inputs/outputs."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("function_path"), TEXT("string"), TEXT("MaterialFunction or MaterialFunctionInterface path."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.add_function_call\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"function_path\":\"/Engine/Functions/Engine_MaterialFunctions02/Utility/BreakOutFloat3Components\"}}")),
		ParityOp(TEXT("material.graph.set_expression_property"), TEXT("material"), TEXT("graph.set_expression_property"), TEXT("Set a reflected property on a material expression."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("expression_index"), TEXT("integer"), TEXT("Expression index from material.graph.list."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through Unreal import text."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.set_expression_property\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"expression_index\":0,\"property\":\"Desc\",\"value\":\"base color\"}}")),
		ParityOp(TEXT("material.graph.connect"), TEXT("material"), TEXT("graph.connect"), TEXT("Connect two material expressions."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("from_expression"), TEXT("integer"), TEXT("Source expression index."), true), ParityParam(TEXT("from_output"), TEXT("string"), TEXT("Source output name."), false), ParityParam(TEXT("to_expression"), TEXT("integer"), TEXT("Target expression index."), true), ParityParam(TEXT("to_input"), TEXT("string"), TEXT("Target input name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.connect\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"from_expression\":0,\"to_expression\":1,\"to_input\":\"A\"}}")),
		ParityOp(TEXT("material.graph.set_output"), TEXT("material"), TEXT("graph.set_output"), TEXT("Connect a material expression to a material output property."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true), ParityParam(TEXT("expression_index"), TEXT("integer"), TEXT("Expression index."), true), ParityParam(TEXT("from_output"), TEXT("string"), TEXT("Source output name."), false), ParityParam(TEXT("property"), TEXT("string"), TEXT("Material property, e.g. BaseColor or MP_BaseColor."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.set_output\",\"params\":{\"asset_path\":\"/Game/M_Test\",\"expression_index\":0,\"property\":\"BaseColor\"}}")),
		ParityOp(TEXT("material.graph.layout"), TEXT("material"), TEXT("graph.layout"), TEXT("Layout material expressions."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true) }, false, EUnreelMCPRisk::Low, TEXT("{\"operation_id\":\"material.graph.layout\",\"params\":{\"asset_path\":\"/Game/M_Test\"}}")),
		ParityOp(TEXT("material.graph.compile"), TEXT("material"), TEXT("graph.compile"), TEXT("Apply and recompile a material graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Material asset path."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"material.graph.compile\",\"params\":{\"asset_path\":\"/Game/M_Test\"}}")),

		ParityOp(TEXT("input.mapping.list"), TEXT("input"), TEXT("mapping.list"), TEXT("List Enhanced Input mappings."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"input.mapping.list\",\"params\":{\"asset_path\":\"/Game/IMC_Player\"}}")),
		ParityOp(TEXT("input.mapping.add"), TEXT("input"), TEXT("mapping.add"), TEXT("Add an Enhanced Input key mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("action_path"), TEXT("string"), TEXT("Input Action asset path."), true), ParityParam(TEXT("key"), TEXT("string"), TEXT("Unreal key name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.add\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"action_path\":\"/Game/IA_Jump\",\"key\":\"SpaceBar\"}}")),
		ParityOp(TEXT("input.mapping.remove"), TEXT("input"), TEXT("mapping.remove"), TEXT("Remove an Enhanced Input key mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("action_path"), TEXT("string"), TEXT("Input Action asset path."), true), ParityParam(TEXT("key"), TEXT("string"), TEXT("Unreal key name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.remove\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"action_path\":\"/Game/IA_Jump\",\"key\":\"SpaceBar\"}}")),
		ParityOp(TEXT("input.action.set_value_type"), TEXT("input"), TEXT("action.set_value_type"), TEXT("Set an Input Action value type."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Action asset path."), true), ParityParam(TEXT("value_type"), TEXT("string"), TEXT("Boolean, Axis1D, Axis2D, or Axis3D."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.action.set_value_type\",\"params\":{\"asset_path\":\"/Game/IA_Move\",\"value_type\":\"Axis2D\"}}")),
		ParityOp(TEXT("input.mapping.set_triggers"), TEXT("input"), TEXT("mapping.set_triggers"), TEXT("Replace trigger objects on an Enhanced Input mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("mapping_index"), TEXT("integer"), TEXT("Mapping index."), true), ParityParam(TEXT("classes"), TEXT("array"), TEXT("Trigger class paths."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.set_triggers\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"mapping_index\":0,\"classes\":[\"/Script/EnhancedInput.InputTriggerPressed\"]}}")),
		ParityOp(TEXT("input.mapping.set_modifiers"), TEXT("input"), TEXT("mapping.set_modifiers"), TEXT("Replace modifier objects on an Enhanced Input mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("mapping_index"), TEXT("integer"), TEXT("Mapping index."), true), ParityParam(TEXT("classes"), TEXT("array"), TEXT("Modifier class paths."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.set_modifiers\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"mapping_index\":0,\"classes\":[\"/Script/EnhancedInput.InputModifierNegate\"]}}")),
		ParityOp(TEXT("input.mapping.set_trigger_property"), TEXT("input"), TEXT("mapping.set_trigger_property"), TEXT("Set a reflected property on a trigger object in an Enhanced Input mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("mapping_index"), TEXT("integer"), TEXT("Mapping index."), true), ParityParam(TEXT("object_index"), TEXT("integer"), TEXT("Trigger index."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("Property value."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.set_trigger_property\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"mapping_index\":0,\"object_index\":0,\"property\":\"ActuationThreshold\",\"value\":0.5}}")),
		ParityOp(TEXT("input.mapping.set_modifier_property"), TEXT("input"), TEXT("mapping.set_modifier_property"), TEXT("Set a reflected property on a modifier object in an Enhanced Input mapping."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Input Mapping Context asset path."), true), ParityParam(TEXT("mapping_index"), TEXT("integer"), TEXT("Mapping index."), true), ParityParam(TEXT("object_index"), TEXT("integer"), TEXT("Modifier index."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("Property value."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"input.mapping.set_modifier_property\",\"params\":{\"asset_path\":\"/Game/IMC_Player\",\"mapping_index\":0,\"object_index\":0,\"property\":\"bX\",\"value\":true}}")),

		ParityOp(TEXT("animation.anim_blueprint.inspect"), TEXT("animation"), TEXT("anim_blueprint.inspect"), TEXT("Inspect animation Blueprint graphs and variables."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.anim_blueprint.inspect\",\"params\":{\"asset_path\":\"/Game/ABP_Player\"}}")),
		ParityOp(TEXT("animation.anim_graph.inspect"), TEXT("animation"), TEXT("anim_graph.inspect"), TEXT("Inspect AnimGraph-specific graphs, nodes, pose pins, pose links, animation assets, and state-machine nodes."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("include_all_graphs"), TEXT("boolean"), TEXT("Include non-animation graphs in the result."), false, TEXT("false")) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.anim_graph.inspect\",\"params\":{\"asset_path\":\"/Game/ABP_Player\"}}")),
		ParityOp(TEXT("animation.anim_graph.add_node_by_class"), TEXT("animation"), TEXT("anim_graph.add_node_by_class"), TEXT("Add a concrete UAnimGraphNode_Base subclass to an AnimGraph or animation-related graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name; defaults to the first AnimGraph."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("AnimGraph node class path, e.g. /Script/AnimGraph.AnimGraphNode_LocalRefPose."), true), ParityParam(TEXT("animation_asset"), TEXT("string"), TEXT("Optional animation asset for UAnimGraphNode_AssetPlayerBase nodes."), false), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional direct reflected node properties applied before pin allocation."), false) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.anim_graph.add_node_by_class\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"class_path\":\"/Script/AnimGraph.AnimGraphNode_LocalRefPose\"}}")),
		ParityOp(TEXT("animation.anim_graph.set_node_property"), TEXT("animation"), TEXT("anim_graph.set_node_property"), TEXT("Set a reflected property on an AnimGraph node and reconstruct it."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name; defaults to the first AnimGraph."), false), ParityParam(TEXT("node"), TEXT("string"), TEXT("Node GUID or object name."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through Unreal import text."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.anim_graph.set_node_property\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"node\":\"...\",\"property\":\"NodeComment\",\"value\":\"Created by MCP\"}}")),
		ParityOp(TEXT("animation.anim_graph.set_node_animation_asset"), TEXT("animation"), TEXT("anim_graph.set_node_animation_asset"), TEXT("Set the animation asset on a UAnimGraphNode_AssetPlayerBase node using its public SetAnimationAsset API."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name; defaults to the first AnimGraph."), false), ParityParam(TEXT("node"), TEXT("string"), TEXT("Node GUID or object name."), true), ParityParam(TEXT("animation_asset"), TEXT("string"), TEXT("Animation asset path."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.anim_graph.set_node_animation_asset\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"node\":\"...\",\"animation_asset\":\"/Game/Walk\"}}")),
		ParityOp(TEXT("animation.anim_graph.connect_pose_pins"), TEXT("animation"), TEXT("anim_graph.connect_pose_pins"), TEXT("Connect pose pins in an AnimGraph through the graph schema."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("Graph name; defaults to the first AnimGraph."), false), ParityParam(TEXT("from_node"), TEXT("string"), TEXT("Source node GUID or object name."), true), ParityParam(TEXT("from_pin"), TEXT("string"), TEXT("Source output pose pin name."), true), ParityParam(TEXT("to_node"), TEXT("string"), TEXT("Target node GUID or object name."), true), ParityParam(TEXT("to_pin"), TEXT("string"), TEXT("Target input pose pin name."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.anim_graph.connect_pose_pins\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"from_node\":\"...\",\"from_pin\":\"Pose\",\"to_node\":\"...\",\"to_pin\":\"Result\"}}")),
		ParityOp(TEXT("animation.state_machine.add"), TEXT("animation"), TEXT("state_machine.add"), TEXT("Add a state-machine node to an Animation Blueprint AnimGraph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("graph_name"), TEXT("string"), TEXT("AnimGraph name; defaults to the first AnimGraph."), false), ParityParam(TEXT("name"), TEXT("string"), TEXT("Optional state-machine graph name."), false), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.add\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"name\":\"Locomotion\"}}")),
		ParityOp(TEXT("animation.state_machine.inspect"), TEXT("animation"), TEXT("state_machine.inspect"), TEXT("Inspect state-machine states, transitions, bound graphs, and entry links."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id; defaults to first state machine."), false) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.state_machine.inspect\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"state_machine\":\"Locomotion\"}}")),
		ParityOp(TEXT("animation.state_machine.add_state"), TEXT("animation"), TEXT("state_machine.add_state"), TEXT("Add a state node and its bound animation state graph."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id."), false), ParityParam(TEXT("name"), TEXT("string"), TEXT("State name."), true), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.add_state\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"name\":\"Idle\"}}")),
		ParityOp(TEXT("animation.state_machine.add_alias"), TEXT("animation"), TEXT("state_machine.add_alias"), TEXT("Add a state alias node and optionally assign aliased states."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id."), false), ParityParam(TEXT("name"), TEXT("string"), TEXT("Alias name."), true), ParityParam(TEXT("aliased_states"), TEXT("array"), TEXT("Optional state names, node ids, or object names to alias."), false), ParityParam(TEXT("global"), TEXT("boolean"), TEXT("Alias all states in the state machine."), false, TEXT("false")), ParityParam(TEXT("x"), TEXT("integer"), TEXT("Node X position."), false), ParityParam(TEXT("y"), TEXT("integer"), TEXT("Node Y position."), false) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.add_alias\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"state_machine\":\"Locomotion\",\"name\":\"AnyAttack\",\"aliased_states\":[\"Idle\",\"Run\"]}}")),
		ParityOp(TEXT("animation.state_machine.set_alias"), TEXT("animation"), TEXT("state_machine.set_alias"), TEXT("Rename a state alias, set global alias mode, or replace its aliased state list."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id."), false), ParityParam(TEXT("alias"), TEXT("string"), TEXT("Alias node GUID, object name, or alias name."), true), ParityParam(TEXT("name"), TEXT("string"), TEXT("Optional new alias name."), false), ParityParam(TEXT("global"), TEXT("boolean"), TEXT("Optional global alias flag."), false), ParityParam(TEXT("aliased_states"), TEXT("array"), TEXT("Optional replacement state names, node ids, or object names."), false) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.set_alias\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"state_machine\":\"Locomotion\",\"alias\":\"AnyAttack\",\"global\":false,\"aliased_states\":[\"Idle\"]}}")),
		ParityOp(TEXT("animation.state_machine.connect"), TEXT("animation"), TEXT("state_machine.connect"), TEXT("Connect entry/state/state nodes through the state-machine schema, auto-creating transition graphs where Unreal does so."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id."), false), ParityParam(TEXT("from"), TEXT("string"), TEXT("Source node id/name/state name, or Entry."), true), ParityParam(TEXT("to"), TEXT("string"), TEXT("Target node id/name/state name."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.connect\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"from\":\"Entry\",\"to\":\"Idle\"}}")),
		ParityOp(TEXT("animation.state_machine.set_node_property"), TEXT("animation"), TEXT("state_machine.set_node_property"), TEXT("Set a reflected property on a state or transition node."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation Blueprint asset path."), true), ParityParam(TEXT("state_machine"), TEXT("string"), TEXT("State-machine graph name or owning node id."), false), ParityParam(TEXT("node"), TEXT("string"), TEXT("Node GUID, object name, or state name."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through Unreal import text."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"animation.state_machine.set_node_property\",\"params\":{\"asset_path\":\"/Game/ABP_Player\",\"node\":\"Idle\",\"property\":\"NodeComment\",\"value\":\"Created by MCP\"}}")),
		ParityOp(TEXT("animation.metadata.list"), TEXT("animation"), TEXT("metadata.list"), TEXT("List UAnimMetaData instances on an animation asset."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.metadata.list\",\"params\":{\"asset_path\":\"/Game/A_Walk\"}}")),
		ParityOp(TEXT("animation.metadata.add"), TEXT("animation"), TEXT("metadata.add"), TEXT("Add a concrete UAnimMetaData instance to an animation asset."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path."), true), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Concrete UAnimMetaData class path."), true), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected metadata properties."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.metadata.add\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"class_path\":\"/Game/BP_MyAnimMeta.BP_MyAnimMeta_C\"}}")),
		ParityOp(TEXT("animation.metadata.remove"), TEXT("animation"), TEXT("metadata.remove"), TEXT("Remove animation metadata by index or class path."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path."), true), ParityParam(TEXT("index"), TEXT("integer"), TEXT("Metadata index from animation.metadata.list."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Optional metadata class path to remove all matching entries."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.metadata.remove\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"index\":0}}")),
		ParityOp(TEXT("animation.metadata.set_property"), TEXT("animation"), TEXT("metadata.set_property"), TEXT("Set a reflected property on an animation metadata instance."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation asset path."), true), ParityParam(TEXT("index"), TEXT("integer"), TEXT("Metadata index."), true), ParityParam(TEXT("property"), TEXT("string"), TEXT("Reflected property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through Unreal import text."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.metadata.set_property\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"index\":0,\"property\":\"Tag\",\"value\":\"Footstep\"}}")),
		ParityOp(TEXT("animation.notify.list"), TEXT("animation"), TEXT("notify.list"), TEXT("List notify tracks and notify/state events on an AnimSequenceBase asset, including montages."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequence, AnimMontage, or other UAnimSequenceBase asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.notify.list\",\"params\":{\"asset_path\":\"/Game/A_Walk\"}}")),
		ParityOp(TEXT("animation.notify.add"), TEXT("animation"), TEXT("notify.add"), TEXT("Add a name-only or class-based UAnimNotify event."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequenceBase asset path."), true), ParityParam(TEXT("time"), TEXT("number"), TEXT("Trigger time in seconds."), true), ParityParam(TEXT("track"), TEXT("string"), TEXT("Notify track name; created if missing."), false, TEXT("MCP")), ParityParam(TEXT("notify_name"), TEXT("string"), TEXT("Name-only notify event when class_path is omitted."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Optional UAnimNotify class path."), false), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties for the notify object."), false), ParityParam(TEXT("event_properties"), TEXT("object"), TEXT("Optional reflected FAnimNotifyEvent fields."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.notify.add\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"time\":0.1,\"notify_name\":\"Footstep\"}}")),
		ParityOp(TEXT("animation.notify.add_state"), TEXT("animation"), TEXT("notify.add_state"), TEXT("Add a class-based UAnimNotifyState event with duration."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequenceBase asset path."), true), ParityParam(TEXT("time"), TEXT("number"), TEXT("Start time in seconds."), true), ParityParam(TEXT("duration"), TEXT("number"), TEXT("State duration in seconds."), true), ParityParam(TEXT("track"), TEXT("string"), TEXT("Notify track name; created if missing."), false, TEXT("MCP")), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("UAnimNotifyState class path."), true), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties for the notify state object."), false), ParityParam(TEXT("event_properties"), TEXT("object"), TEXT("Optional reflected FAnimNotifyEvent fields."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.notify.add_state\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"time\":0.2,\"duration\":0.4,\"class_path\":\"/Script/Engine.AnimNotifyState_DisableRootMotion\"}}")),
		ParityOp(TEXT("animation.notify.move"), TEXT("animation"), TEXT("notify.move"), TEXT("Move an existing notify by index or GUID, optionally changing duration and track."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequenceBase asset path."), true), ParityParam(TEXT("index"), TEXT("integer"), TEXT("Notify index from animation.notify.list."), false), ParityParam(TEXT("guid"), TEXT("string"), TEXT("Notify GUID from animation.notify.list."), false), ParityParam(TEXT("time"), TEXT("number"), TEXT("New trigger/start time in seconds."), false), ParityParam(TEXT("duration"), TEXT("number"), TEXT("New state duration in seconds."), false), ParityParam(TEXT("track"), TEXT("string"), TEXT("Optional target track name; created if missing."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.notify.move\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"index\":0,\"time\":0.25}}")),
		ParityOp(TEXT("animation.notify.remove"), TEXT("animation"), TEXT("notify.remove"), TEXT("Remove notifies by index, GUID, name, or class path."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequenceBase asset path."), true), ParityParam(TEXT("index"), TEXT("integer"), TEXT("Notify index."), false), ParityParam(TEXT("guid"), TEXT("string"), TEXT("Notify GUID."), false), ParityParam(TEXT("notify_name"), TEXT("string"), TEXT("Remove all name-matching notifies."), false), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Remove all class-matching notify/state events."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.notify.remove\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"index\":0}}")),
		ParityOp(TEXT("animation.notify.set_property"), TEXT("animation"), TEXT("notify.set_property"), TEXT("Set a reflected property on an FAnimNotifyEvent or its notify/state object."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimSequenceBase asset path."), true), ParityParam(TEXT("index"), TEXT("integer"), TEXT("Notify index."), false), ParityParam(TEXT("guid"), TEXT("string"), TEXT("Notify GUID."), false), ParityParam(TEXT("target"), TEXT("string"), TEXT("'event' or 'object'. Defaults to object when a notify/state object exists."), false), ParityParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), ParityParam(TEXT("value"), TEXT("any"), TEXT("JSON value converted through reflection."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.notify.set_property\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"index\":0,\"target\":\"event\",\"property\":\"NotifyTriggerChance\",\"value\":0.5}}")),
		ParityOp(TEXT("animation.montage.inspect"), TEXT("animation"), TEXT("montage.inspect"), TEXT("Inspect montage slots, segments, sections, next-section links, and notify count."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"animation.montage.inspect\",\"params\":{\"asset_path\":\"/Game/AM_Attack\"}}")),
		ParityOp(TEXT("animation.montage.add_slot"), TEXT("animation"), TEXT("montage.add_slot"), TEXT("Add a slot track to an animation montage."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("slot_name"), TEXT("string"), TEXT("Slot name."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.add_slot\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"slot_name\":\"DefaultSlot\"}}")),
		ParityOp(TEXT("animation.montage.remove_slot"), TEXT("animation"), TEXT("montage.remove_slot"), TEXT("Remove a montage slot track by slot name or slot index."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("slot_name"), TEXT("string"), TEXT("Slot name."), false), ParityParam(TEXT("slot_index"), TEXT("integer"), TEXT("Slot index from animation.montage.inspect."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.remove_slot\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"slot_name\":\"UpperBody\"}}")),
		ParityOp(TEXT("animation.montage.add_section"), TEXT("animation"), TEXT("montage.add_section"), TEXT("Add a composite section to an animation montage."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("section_name"), TEXT("string"), TEXT("Section name."), true), ParityParam(TEXT("time"), TEXT("number"), TEXT("Section start time."), false, TEXT("0")), ParityParam(TEXT("next_section"), TEXT("string"), TEXT("Optional next section link name."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.add_section\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"section_name\":\"Windup\",\"time\":0.0}}")),
		ParityOp(TEXT("animation.montage.set_section"), TEXT("animation"), TEXT("montage.set_section"), TEXT("Edit a montage section name, time, or next-section link."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("section_name"), TEXT("string"), TEXT("Section name to edit."), false), ParityParam(TEXT("section_index"), TEXT("integer"), TEXT("Section index to edit."), false), ParityParam(TEXT("new_section_name"), TEXT("string"), TEXT("Optional new section name."), false), ParityParam(TEXT("time"), TEXT("number"), TEXT("Optional new section start time."), false), ParityParam(TEXT("next_section"), TEXT("string"), TEXT("Optional next section link name, or empty for none."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.set_section\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"section_name\":\"Windup\",\"next_section\":\"Loop\"}}")),
		ParityOp(TEXT("animation.montage.remove_section"), TEXT("animation"), TEXT("montage.remove_section"), TEXT("Remove a montage section by name or index."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("section_name"), TEXT("string"), TEXT("Section name."), false), ParityParam(TEXT("section_index"), TEXT("integer"), TEXT("Section index."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.remove_section\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"section_name\":\"Windup\"}}")),
		ParityOp(TEXT("animation.montage.add_segment"), TEXT("animation"), TEXT("montage.add_segment"), TEXT("Add an animation segment to a montage slot track."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("slot_name"), TEXT("string"), TEXT("Target slot name."), false), ParityParam(TEXT("slot_index"), TEXT("integer"), TEXT("Target slot index."), false), ParityParam(TEXT("animation_asset"), TEXT("string"), TEXT("AnimSequence or AnimComposite asset path."), true), ParityParam(TEXT("start_pos"), TEXT("number"), TEXT("Montage track start position."), false, TEXT("0")), ParityParam(TEXT("anim_start_time"), TEXT("number"), TEXT("Source animation start time."), false, TEXT("0")), ParityParam(TEXT("anim_end_time"), TEXT("number"), TEXT("Source animation end time. Defaults to source length."), false), ParityParam(TEXT("play_rate"), TEXT("number"), TEXT("Segment play rate."), false, TEXT("1")), ParityParam(TEXT("looping_count"), TEXT("integer"), TEXT("Loop count."), false, TEXT("1")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.add_segment\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"slot_name\":\"DefaultSlot\",\"animation_asset\":\"/Game/A_Attack\"}}")),
		ParityOp(TEXT("animation.montage.set_segment"), TEXT("animation"), TEXT("montage.set_segment"), TEXT("Edit a montage segment's animation asset, timing, play rate, or loop count."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("slot_name"), TEXT("string"), TEXT("Slot name."), false), ParityParam(TEXT("slot_index"), TEXT("integer"), TEXT("Slot index."), false), ParityParam(TEXT("segment_index"), TEXT("integer"), TEXT("Segment index."), true), ParityParam(TEXT("animation_asset"), TEXT("string"), TEXT("Optional replacement animation asset path."), false), ParityParam(TEXT("start_pos"), TEXT("number"), TEXT("Optional montage track start position."), false), ParityParam(TEXT("anim_start_time"), TEXT("number"), TEXT("Optional source animation start time."), false), ParityParam(TEXT("anim_end_time"), TEXT("number"), TEXT("Optional source animation end time."), false), ParityParam(TEXT("play_rate"), TEXT("number"), TEXT("Optional play rate."), false), ParityParam(TEXT("looping_count"), TEXT("integer"), TEXT("Optional loop count."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.set_segment\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"slot_name\":\"DefaultSlot\",\"segment_index\":0,\"play_rate\":1.25}}")),
		ParityOp(TEXT("animation.montage.remove_segment"), TEXT("animation"), TEXT("montage.remove_segment"), TEXT("Remove a montage animation segment by slot and segment index."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("AnimMontage asset path."), true), ParityParam(TEXT("slot_name"), TEXT("string"), TEXT("Slot name."), false), ParityParam(TEXT("slot_index"), TEXT("integer"), TEXT("Slot index."), false), ParityParam(TEXT("segment_index"), TEXT("integer"), TEXT("Segment index."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.montage.remove_segment\",\"params\":{\"asset_path\":\"/Game/AM_Attack\",\"slot_name\":\"DefaultSlot\",\"segment_index\":0}}")),
		ParityOp(TEXT("animation.sequence.set_metadata"), TEXT("animation"), TEXT("sequence.set_metadata"), TEXT("Compatibility alias for animation.metadata.add."), { ParityParam(TEXT("asset_path"), TEXT("string"), TEXT("Animation sequence asset path."), true), ParityParam(TEXT("class_path"), TEXT("string"), TEXT("Concrete UAnimMetaData class path."), true), ParityParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected metadata properties."), false) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"animation.sequence.set_metadata\",\"params\":{\"asset_path\":\"/Game/A_Walk\",\"class_path\":\"/Game/BP_MyAnimMeta.BP_MyAnimMeta_C\"}}"))
	};
}

FUnreelMCPResult FUnreelMCPParityService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("blueprint.graph.list")) return ListBlueprintGraphs(Params);
	if (OperationId == TEXT("blueprint.function.create")) return CreateBlueprintFunction(Params);
	if (OperationId == TEXT("blueprint.graph.add_event")) return AddBlueprintEvent(Params);
	if (OperationId == TEXT("blueprint.graph.add_function_call")) return AddBlueprintFunctionCall(Params);
	if (OperationId == TEXT("blueprint.graph.add_variable_get")) return AddBlueprintVariableNode(Params, false);
	if (OperationId == TEXT("blueprint.graph.add_variable_set")) return AddBlueprintVariableNode(Params, true);
	if (OperationId == TEXT("blueprint.graph.add_node_by_class")) return AddBlueprintNodeByClass(Params);
	if (OperationId == TEXT("blueprint.graph.add_common_node")) return AddBlueprintCommonNode(Params);
	if (OperationId == TEXT("blueprint.graph.set_node_property")) return SetBlueprintNodeProperty(Params);
	if (OperationId == TEXT("blueprint.graph.set_pin_default")) return SetBlueprintPinDefault(Params);
	if (OperationId == TEXT("blueprint.graph.connect_pins")) return ConnectBlueprintPins(Params);
	if (OperationId == TEXT("blueprint.graph.layout")) return LayoutBlueprintGraph(Params);
	if (OperationId == TEXT("material.graph.list")) return ListMaterialGraph(Params);
	if (OperationId == TEXT("material.graph.add_expression")) return AddMaterialExpression(Params);
	if (OperationId == TEXT("material.graph.add_parameter")) return AddMaterialParameter(Params);
	if (OperationId == TEXT("material.graph.set_texture")) return SetMaterialTexture(Params);
	if (OperationId == TEXT("material.graph.add_function_call")) return AddMaterialFunctionCall(Params);
	if (OperationId == TEXT("material.graph.set_expression_property")) return SetMaterialExpressionProperty(Params);
	if (OperationId == TEXT("material.graph.connect")) return ConnectMaterialExpressions(Params);
	if (OperationId == TEXT("material.graph.set_output")) return SetMaterialOutput(Params);
	if (OperationId == TEXT("material.graph.layout")) return LayoutMaterialGraph(Params);
	if (OperationId == TEXT("material.graph.compile")) return CompileMaterial(Params);
	if (OperationId == TEXT("input.mapping.list")) return ListInputMappings(Params);
	if (OperationId == TEXT("input.mapping.add")) return AddInputMapping(Params);
	if (OperationId == TEXT("input.mapping.remove")) return RemoveInputMapping(Params);
	if (OperationId == TEXT("input.action.set_value_type")) return SetInputActionValueType(Params);
	if (OperationId == TEXT("input.mapping.set_triggers")) return SetInputMappingObjects(Params, true);
	if (OperationId == TEXT("input.mapping.set_modifiers")) return SetInputMappingObjects(Params, false);
	if (OperationId == TEXT("input.mapping.set_trigger_property")) return SetInputMappingObjectProperty(Params, true);
	if (OperationId == TEXT("input.mapping.set_modifier_property")) return SetInputMappingObjectProperty(Params, false);
	if (OperationId == TEXT("animation.anim_blueprint.inspect")) return InspectAnimBlueprint(Params);
	if (OperationId == TEXT("animation.anim_graph.inspect")) return InspectAnimGraph(Params);
	if (OperationId == TEXT("animation.anim_graph.add_node_by_class")) return AddAnimGraphNodeByClass(Params);
	if (OperationId == TEXT("animation.anim_graph.set_node_property")) return SetAnimGraphNodeProperty(Params);
	if (OperationId == TEXT("animation.anim_graph.set_node_animation_asset")) return SetAnimGraphNodeAnimationAsset(Params);
	if (OperationId == TEXT("animation.anim_graph.connect_pose_pins")) return ConnectAnimGraphPosePins(Params);
	if (OperationId == TEXT("animation.state_machine.add")) return AddStateMachine(Params);
	if (OperationId == TEXT("animation.state_machine.inspect")) return InspectStateMachine(Params);
	if (OperationId == TEXT("animation.state_machine.add_state")) return AddStateMachineState(Params);
	if (OperationId == TEXT("animation.state_machine.add_alias")) return AddStateMachineAlias(Params);
	if (OperationId == TEXT("animation.state_machine.set_alias")) return SetStateMachineAlias(Params);
	if (OperationId == TEXT("animation.state_machine.connect")) return ConnectStateMachineStates(Params);
	if (OperationId == TEXT("animation.state_machine.set_node_property")) return SetStateMachineNodeProperty(Params);
	if (OperationId == TEXT("animation.metadata.list")) return ListAnimationMetadata(Params);
	if (OperationId == TEXT("animation.metadata.add")) return AddAnimationMetadata(Params);
	if (OperationId == TEXT("animation.metadata.remove")) return RemoveAnimationMetadata(Params);
	if (OperationId == TEXT("animation.metadata.set_property")) return SetAnimationMetadataProperty(Params);
	if (OperationId == TEXT("animation.notify.list")) return ListAnimationNotifies(Params);
	if (OperationId == TEXT("animation.notify.add")) return AddAnimationNotify(Params, false);
	if (OperationId == TEXT("animation.notify.add_state")) return AddAnimationNotify(Params, true);
	if (OperationId == TEXT("animation.notify.move")) return MoveAnimationNotify(Params);
	if (OperationId == TEXT("animation.notify.remove")) return RemoveAnimationNotify(Params);
	if (OperationId == TEXT("animation.notify.set_property")) return SetAnimationNotifyProperty(Params);
	if (OperationId == TEXT("animation.montage.inspect")) return InspectMontage(Params);
	if (OperationId == TEXT("animation.montage.add_slot")) return AddMontageSlot(Params);
	if (OperationId == TEXT("animation.montage.remove_slot")) return RemoveMontageSlot(Params);
	if (OperationId == TEXT("animation.montage.add_section")) return AddMontageSection(Params);
	if (OperationId == TEXT("animation.montage.set_section")) return SetMontageSection(Params);
	if (OperationId == TEXT("animation.montage.remove_section")) return RemoveMontageSection(Params);
	if (OperationId == TEXT("animation.montage.add_segment")) return AddMontageSegment(Params);
	if (OperationId == TEXT("animation.montage.set_segment")) return SetMontageSegment(Params);
	if (OperationId == TEXT("animation.montage.remove_segment")) return RemoveMontageSegment(Params);
	if (OperationId == TEXT("animation.sequence.set_metadata")) return AddAnimationMetadata(Params);
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown parity operation: %s"), *OperationId));
}

UObject* FUnreelMCPParityService::LoadAssetObject(const FString& AssetPath) const
{
	UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Object && !AssetPath.Contains(TEXT(".")))
	{
		const FString AssetName = FPaths::GetBaseFilename(AssetPath);
		Object = StaticLoadObject(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + AssetName));
	}
	return Object;
}

UClass* FUnreelMCPParityService::ResolveAnimationMetadataClass(const FString& ClassPath) const
{
	UClass* MetadataClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!MetadataClass)
	{
		MetadataClass = StaticLoadClass(UAnimMetaData::StaticClass(), nullptr, *ClassPath);
	}

	if (!MetadataClass && ClassPath.EndsWith(TEXT("_C")))
	{
		const FString BlueprintPath = ClassPath.LeftChop(2);
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(BlueprintPath)))
		{
			MetadataClass = Blueprint->GeneratedClass;
		}
	}

	if (!MetadataClass)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(ClassPath)))
		{
			MetadataClass = Blueprint->GeneratedClass;
		}
	}

	if (!MetadataClass || !MetadataClass->IsChildOf(UAnimMetaData::StaticClass()) || MetadataClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	return MetadataClass;
}

UClass* FUnreelMCPParityService::ResolveAnimNotifyClass(const FString& ClassPath, bool bState) const
{
	UClass* BaseClass = bState ? UAnimNotifyState::StaticClass() : UAnimNotify::StaticClass();
	UClass* NotifyClass = FindObject<UClass>(nullptr, *ClassPath);
	if (!NotifyClass)
	{
		NotifyClass = StaticLoadClass(BaseClass, nullptr, *ClassPath);
	}
	if (!NotifyClass && ClassPath.EndsWith(TEXT("_C")))
	{
		const FString BlueprintPath = ClassPath.LeftChop(2);
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(BlueprintPath)))
		{
			NotifyClass = Blueprint->GeneratedClass;
		}
	}
	if (!NotifyClass)
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadAssetObject(ClassPath)))
		{
			NotifyClass = Blueprint->GeneratedClass;
		}
	}
	if (!NotifyClass || !NotifyClass->IsChildOf(BaseClass) || NotifyClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	return NotifyClass;
}

UBlueprint* FUnreelMCPParityService::LoadBlueprint(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return nullptr;
	}
	return Cast<UBlueprint>(LoadAssetObject(AssetPath));
}

UEdGraph* FUnreelMCPParityService::FindBlueprintGraph(UBlueprint* Blueprint, const FString& GraphName) const
{
	if (!Blueprint)
	{
		return nullptr;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	if (GraphName.IsEmpty())
	{
		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName() == TEXT("EventGraph"))
			{
				return Graph;
			}
		}
	}
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && (GraphName.IsEmpty() || Graph->GetName() == GraphName))
		{
			return Graph;
		}
	}
	return nullptr;
}

UEdGraph* FUnreelMCPParityService::FindAnimGraph(UBlueprint* Blueprint, const FString& GraphName) const
{
	if (!Blueprint)
	{
		return nullptr;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		if (!GraphName.IsEmpty() && Graph->GetName() == GraphName)
		{
			return Graph;
		}
		if (GraphName.IsEmpty() && Cast<UAnimationGraph>(Graph))
		{
			return Graph;
		}
	}
	return nullptr;
}

UAnimationStateMachineGraph* FUnreelMCPParityService::FindStateMachineGraph(UBlueprint* Blueprint, const FString& StateMachine) const
{
	if (!Blueprint)
	{
		return nullptr;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		UAnimationStateMachineGraph* MachineGraph = Cast<UAnimationStateMachineGraph>(Graph);
		if (!MachineGraph)
		{
			continue;
		}
		if (StateMachine.IsEmpty() || Graph->GetName() == StateMachine)
		{
			return MachineGraph;
		}
		if (MachineGraph->OwnerAnimGraphNode &&
			(NodeId(MachineGraph->OwnerAnimGraphNode).Equals(StateMachine, ESearchCase::IgnoreCase) ||
				MachineGraph->OwnerAnimGraphNode->GetName() == StateMachine ||
				MachineGraph->OwnerAnimGraphNode->GetStateMachineName() == StateMachine))
		{
			return MachineGraph;
		}
	}
	return nullptr;
}

UAnimStateNodeBase* FUnreelMCPParityService::FindStateMachineStateNode(UAnimationStateMachineGraph* Graph, const FString& Key) const
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(Node);
		if (StateNode &&
			(NodeId(StateNode).Equals(Key, ESearchCase::IgnoreCase) ||
				StateNode->GetName() == Key ||
				StateNode->GetStateName() == Key))
		{
			return StateNode;
		}
	}
	return nullptr;
}

UEdGraphNode* FUnreelMCPParityService::FindNode(UEdGraph* Graph, const FString& InNodeId) const
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node && (NodeId(Node).Equals(InNodeId, ESearchCase::IgnoreCase) || Node->GetName() == InNodeId))
		{
			return Node;
		}
	}
	return nullptr;
}

UEdGraphPin* FUnreelMCPParityService::FindPin(UEdGraphNode* Node, const FString& PinName, const FString& Direction) const
{
	if (!Node)
	{
		return nullptr;
	}
	const bool bWantInput = Direction.Equals(TEXT("input"), ESearchCase::IgnoreCase);
	const bool bWantOutput = Direction.Equals(TEXT("output"), ESearchCase::IgnoreCase);
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (!Pin || Pin->PinName.ToString() != PinName)
		{
			continue;
		}
		if (bWantInput && Pin->Direction != EGPD_Input)
		{
			continue;
		}
		if (bWantOutput && Pin->Direction != EGPD_Output)
		{
			continue;
		}
		return Pin;
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FUnreelMCPParityService::NodeSummary(const UEdGraphNode* Node) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), NodeId(Node));
	Json->SetStringField(TEXT("name"), Node ? Node->GetName() : TEXT(""));
	Json->SetStringField(TEXT("class"), Node ? Node->GetClass()->GetPathName() : TEXT(""));
	Json->SetStringField(TEXT("title"), Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : TEXT(""));
	Json->SetNumberField(TEXT("x"), Node ? Node->NodePosX : 0);
	Json->SetNumberField(TEXT("y"), Node ? Node->NodePosY : 0);
	TArray<TSharedPtr<FJsonValue>> Pins;
	if (Node)
	{
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
			PinJson->SetStringField(TEXT("name"), Pin ? Pin->PinName.ToString() : TEXT(""));
			PinJson->SetStringField(TEXT("direction"), Pin && Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
			PinJson->SetStringField(TEXT("type"), Pin ? Pin->PinType.PinCategory.ToString() : TEXT(""));
			PinJson->SetNumberField(TEXT("linked_to"), Pin ? Pin->LinkedTo.Num() : 0);
			Pins.Add(MakeShared<FJsonValueObject>(PinJson));
		}
	}
	Json->SetArrayField(TEXT("pins"), Pins);
	return Json;
}

FUnreelMCPResult FUnreelMCPParityService::ListBlueprintGraphs(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Blueprint from asset_path"));
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TArray<TSharedPtr<FJsonValue>> GraphJson;
	for (UEdGraph* Graph : Graphs)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("name"), Graph ? Graph->GetName() : TEXT(""));
		Json->SetStringField(TEXT("schema"), Graph && Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetPathName() : TEXT(""));
		TArray<TSharedPtr<FJsonValue>> Nodes;
		if (Graph)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (Node)
				{
					Nodes.Add(MakeShared<FJsonValueObject>(NodeSummary(Node)));
				}
			}
		}
		Json->SetArrayField(TEXT("nodes"), Nodes);
		GraphJson.Add(MakeShared<FJsonValueObject>(Json));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Blueprint->GetPathName());
	Data->SetArrayField(TEXT("graphs"), GraphJson);
	TArray<TSharedPtr<FJsonValue>> Variables;
	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		TSharedPtr<FJsonObject> VariableJson = MakeShared<FJsonObject>();
		VariableJson->SetStringField(TEXT("name"), Variable.VarName.ToString());
		VariableJson->SetStringField(TEXT("guid"), Variable.VarGuid.ToString(EGuidFormats::DigitsWithHyphens));
		VariableJson->SetStringField(TEXT("category"), Variable.Category.ToString());
		VariableJson->SetStringField(TEXT("friendly_name"), Variable.FriendlyName);
		VariableJson->SetStringField(TEXT("default_value"), Variable.DefaultValue);
		VariableJson->SetStringField(TEXT("pin_category"), Variable.VarType.PinCategory.ToString());
		VariableJson->SetStringField(TEXT("pin_sub_category"), Variable.VarType.PinSubCategory.ToString());
		VariableJson->SetStringField(TEXT("pin_sub_category_object"), Variable.VarType.PinSubCategoryObject.IsValid() ? Variable.VarType.PinSubCategoryObject->GetPathName() : TEXT(""));
		VariableJson->SetBoolField(TEXT("is_array"), Variable.VarType.ContainerType == EPinContainerType::Array);
		VariableJson->SetBoolField(TEXT("is_set"), Variable.VarType.ContainerType == EPinContainerType::Set);
		VariableJson->SetBoolField(TEXT("is_map"), Variable.VarType.ContainerType == EPinContainerType::Map);
		Variables.Add(MakeShared<FJsonValueObject>(VariableJson));
	}
	Data->SetArrayField(TEXT("variables"), Variables);
	return FUnreelMCPResult::Success(TEXT("Blueprint graphs loaded"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::CreateBlueprintFunction(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Name;
	if (!Blueprint || !Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or function name"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "CreateBlueprintFunction", "Create Blueprint Function"));
	Blueprint->Modify();
	UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*Name), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	FBlueprintEditorUtils::AddFunctionGraph(Blueprint, Graph, false, static_cast<UFunction*>(nullptr));
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graph"), Graph->GetName());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint function graph created"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddBlueprintEvent(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString EventName;
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("event_name"), EventName) || EventName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or event_name"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find Blueprint graph"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddBlueprintEvent", "Add Blueprint Event"));
	Graph->Modify();
	UK2Node_CustomEvent* Node = NewObject<UK2Node_CustomEvent>(Graph);
	Node->CustomFunctionName = FName(*EventName);
	Node->NodePosX = ReadInt(Params, TEXT("x"), 0);
	Node->NodePosY = ReadInt(Params, TEXT("y"), 0);
	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = NodeSummary(Node);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint event node added"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddBlueprintFunctionCall(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString ClassPath;
	FString FunctionName;
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("class_path"), ClassPath) || !Params->TryGetStringField(TEXT("function_name"), FunctionName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path, class_path, or function_name"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	UClass* Class = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
	UFunction* Function = Class ? Class->FindFunctionByName(FName(*FunctionName)) : nullptr;
	if (!Graph || !Function)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve graph or function: %s.%s"), *ClassPath, *UnreelMCPServiceUtils::ScriptLoadHint(ClassPath)));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddBlueprintCall", "Add Blueprint Function Call"));
	Graph->Modify();
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
	Node->SetFromFunction(Function);
	Node->NodePosX = ReadInt(Params, TEXT("x"), 300);
	Node->NodePosY = ReadInt(Params, TEXT("y"), 0);
	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint function call node added"), NodeSummary(Node));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddBlueprintVariableNode(const TSharedRef<FJsonObject>& Params, bool bSetter) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString VariableName;
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("variable_name"), VariableName) || VariableName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or variable_name"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find Blueprint graph"));
	}
	FProperty* Property = Blueprint->SkeletonGeneratedClass ? FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, FName(*VariableName)) : nullptr;
	if (!Property && Blueprint->GeneratedClass)
	{
		Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, FName(*VariableName));
	}
	if (!Property)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to find Blueprint variable: %s"), *VariableName));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddBlueprintVariableNode", "Add Blueprint Variable Node"));
	Graph->Modify();
	UEdGraphNode* Node = nullptr;
	const UEdGraphSchema_K2* Schema = Cast<UEdGraphSchema_K2>(Graph->GetSchema());
	if (Schema)
	{
		const FVector2D Position(ReadInt(Params, TEXT("x"), 0), ReadInt(Params, TEXT("y"), 120));
		Node = bSetter
			? static_cast<UEdGraphNode*>(Schema->SpawnVariableSetNode(Position, Graph, FName(*VariableName), Blueprint->SkeletonGeneratedClass))
			: static_cast<UEdGraphNode*>(Schema->SpawnVariableGetNode(Position, Graph, FName(*VariableName), Blueprint->SkeletonGeneratedClass));
	}
	if (!Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to spawn Blueprint variable node"));
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(bSetter ? TEXT("Blueprint variable set node added") : TEXT("Blueprint variable get node added"), NodeSummary(Node));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddBlueprintNodeByClass(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString ClassPath;
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or class_path"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	UClass* NodeClass = StaticLoadClass(UEdGraphNode::StaticClass(), nullptr, *ClassPath);
	if (!Graph || !NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()) || NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve graph or concrete UEdGraphNode class: %s.%s"), *ClassPath, *UnreelMCPServiceUtils::ScriptLoadHint(ClassPath)));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddBlueprintNodeByClass", "Add Blueprint Node By Class"));
	Graph->Modify();
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeClass);
	Node->NodePosX = ReadInt(Params, TEXT("x"), 0);
	Node->NodePosY = ReadInt(Params, TEXT("y"), 0);

	const TSharedPtr<FJsonObject>* PropertiesField = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesField) && PropertiesField && PropertiesField->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropertiesField)->Values)
		{
			FString Error;
			if (!ApplyJsonProperty(Node, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}

	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint node added"), NodeSummary(Node));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddBlueprintCommonNode(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Kind;
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("kind"), Kind) || Kind.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or kind"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find Blueprint graph"));
	}

	auto FinalizeNode = [this, Blueprint, Graph, &Params](UEdGraphNode* Node, const TCHAR* TransactionText) -> FUnreelMCPResult
	{
		FUnreelMCPScopedTransaction Transaction(FText::FromString(TransactionText));
		Graph->Modify();
		Node->NodePosX = ReadInt(Params, TEXT("x"), 0);
		Node->NodePosY = ReadInt(Params, TEXT("y"), 0);
		Graph->AddNode(Node, true, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint common node added"), NodeSummary(Node));
		Result.ChangedAssets.Add(Blueprint->GetPathName());
		return Result;
	};

	if (Kind.Equals(TEXT("branch"), ESearchCase::IgnoreCase))
	{
		return FinalizeNode(NewObject<UK2Node_IfThenElse>(Graph), TEXT("UnreelMCP Add Branch Node"));
	}
	if (Kind.Equals(TEXT("sequence"), ESearchCase::IgnoreCase))
	{
		UK2Node_ExecutionSequence* Node = NewObject<UK2Node_ExecutionSequence>(Graph);
		FUnreelMCPResult Result = FinalizeNode(Node, TEXT("UnreelMCP Add Sequence Node"));
		const int32 DesiredThenPins = FMath::Max(2, ReadInt(Params, TEXT("then_pins"), 2));
		for (;;)
		{
			int32 ThenPins = 0;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					++ThenPins;
				}
			}
			if (ThenPins >= DesiredThenPins)
			{
				break;
			}
			Node->AddInputPin();
		}
		Node->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Result.Data = NodeSummary(Node);
		return Result;
	}
	if (Kind.Equals(TEXT("dynamic_cast"), ESearchCase::IgnoreCase))
	{
		FString TargetClassPath;
		if (!Params->TryGetStringField(TEXT("target_class_path"), TargetClassPath) || TargetClassPath.IsEmpty())
		{
			return FUnreelMCPResult::Error(TEXT("dynamic_cast requires target_class_path"));
		}
		UClass* TargetClass = StaticLoadClass(UObject::StaticClass(), nullptr, *TargetClassPath);
		if (!TargetClass)
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("target class"), TargetClassPath));
		}
		UK2Node_DynamicCast* Node = NewObject<UK2Node_DynamicCast>(Graph);
		Node->TargetType = TargetClass;
		FUnreelMCPResult Result = FinalizeNode(Node, TEXT("UnreelMCP Add Dynamic Cast Node"));
		bool bPure = false;
		if (Params->TryGetBoolField(TEXT("pure"), bPure))
		{
			Node->SetPurity(bPure);
		}
		Node->ReconstructNode();
		Result.Data = NodeSummary(Node);
		return Result;
	}
	if (Kind.Equals(TEXT("select"), ESearchCase::IgnoreCase))
	{
		UK2Node_Select* Node = NewObject<UK2Node_Select>(Graph);
		FString EnumPath;
		if (Params->TryGetStringField(TEXT("enum_path"), EnumPath) && !EnumPath.IsEmpty())
		{
			UEnum* Enum = LoadObject<UEnum>(nullptr, *EnumPath);
			if (!Enum)
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load enum: %s"), *EnumPath));
			}
			Node->SetEnum(Enum, true);
		}
		FUnreelMCPResult Result = FinalizeNode(Node, TEXT("UnreelMCP Add Select Node"));
		const int32 DesiredOptionPins = FMath::Max(2, ReadInt(Params, TEXT("option_count"), 2));
		for (;;)
		{
			TArray<UEdGraphPin*> OptionPins;
			Node->GetOptionPins(OptionPins);
			if (OptionPins.Num() >= DesiredOptionPins || !Node->CanAddPin())
			{
				break;
			}
			Node->AddInputPin();
		}
		Node->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		Result.Data = NodeSummary(Node);
		return Result;
	}
	if (Kind.Equals(TEXT("timeline"), ESearchCase::IgnoreCase))
	{
		if (!FBlueprintEditorUtils::DoesSupportTimelines(Blueprint))
		{
			return FUnreelMCPResult::Error(TEXT("This Blueprint does not support timelines"));
		}
		FString TimelineNameString;
		Params->TryGetStringField(TEXT("timeline_name"), TimelineNameString);
		const FName TimelineName = TimelineNameString.IsEmpty() ? FBlueprintEditorUtils::FindUniqueTimelineName(Blueprint) : FName(*TimelineNameString);
		UTimelineTemplate* Timeline = FBlueprintEditorUtils::AddNewTimeline(Blueprint, TimelineName);
		if (!Timeline)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to create timeline: %s"), *TimelineName.ToString()));
		}
		UK2Node_Timeline* Node = NewObject<UK2Node_Timeline>(Graph);
		Node->TimelineName = Timeline->GetVariableName();
		Node->TimelineGuid = Timeline->TimelineGuid;
		bool bFlag = false;
		if (Params->TryGetBoolField(TEXT("auto_play"), bFlag)) Node->bAutoPlay = bFlag;
		if (Params->TryGetBoolField(TEXT("loop"), bFlag)) Node->bLoop = bFlag;
		if (Params->TryGetBoolField(TEXT("replicated"), bFlag)) Node->bReplicated = bFlag;
		if (Params->TryGetBoolField(TEXT("ignore_time_dilation"), bFlag)) Node->bIgnoreTimeDilation = bFlag;
		return FinalizeNode(Node, TEXT("UnreelMCP Add Timeline Node"));
	}
	if (Kind.Equals(TEXT("add_component_by_class"), ESearchCase::IgnoreCase))
	{
		FString ComponentClassPath;
		if (!Params->TryGetStringField(TEXT("component_class_path"), ComponentClassPath) || ComponentClassPath.IsEmpty())
		{
			return FUnreelMCPResult::Error(TEXT("add_component_by_class requires component_class_path"));
		}
		UClass* ComponentClass = StaticLoadClass(UActorComponent::StaticClass(), nullptr, *ComponentClassPath);
		if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("ActorComponent class"), ComponentClassPath));
		}
		UK2Node_AddComponentByClass* Node = NewObject<UK2Node_AddComponentByClass>(Graph);
		FUnreelMCPResult Result = FinalizeNode(Node, TEXT("UnreelMCP Add Component By Class Node"));
		if (UEdGraphPin* ClassPin = Node->FindPin(TEXT("Class"), EGPD_Input))
		{
			Graph->GetSchema()->TrySetDefaultObject(*ClassPin, ComponentClass);
		}
		Node->ReconstructNode();
		Result.Data = NodeSummary(Node);
		return Result;
	}
	if (Kind.Equals(TEXT("add_delegate"), ESearchCase::IgnoreCase) || Kind.Equals(TEXT("assign_delegate"), ESearchCase::IgnoreCase))
	{
		FString DelegateName;
		FString OwnerClassPath;
		if (!Params->TryGetStringField(TEXT("delegate_name"), DelegateName) || DelegateName.IsEmpty())
		{
			return FUnreelMCPResult::Error(TEXT("Delegate nodes require delegate_name"));
		}
		Params->TryGetStringField(TEXT("owner_class_path"), OwnerClassPath);
		UClass* OwnerClass = nullptr;
		if (OwnerClassPath.IsEmpty())
		{
			OwnerClass = Blueprint->GeneratedClass;
		}
		else
		{
			OwnerClass = StaticLoadClass(UObject::StaticClass(), nullptr, *OwnerClassPath);
		}
		if (!OwnerClass)
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("owner class"), OwnerClassPath));
		}
		FMulticastDelegateProperty* DelegateProperty = FindFProperty<FMulticastDelegateProperty>(OwnerClass, FName(*DelegateName));
		if (!DelegateProperty)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to find multicast delegate property: %s"), *DelegateName));
		}
		UK2Node_BaseMCDelegate* Node = Kind.Equals(TEXT("assign_delegate"), ESearchCase::IgnoreCase)
			? static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_AssignDelegate>(Graph))
			: static_cast<UK2Node_BaseMCDelegate*>(NewObject<UK2Node_AddDelegate>(Graph));
		Node->SetFromProperty(DelegateProperty, true, OwnerClass);
		return FinalizeNode(Node, TEXT("UnreelMCP Add Delegate Node"));
	}
	if (Kind.Equals(TEXT("macro_instance"), ESearchCase::IgnoreCase))
	{
		FString MacroGraphName;
		if (!Params->TryGetStringField(TEXT("macro_graph_name"), MacroGraphName) || MacroGraphName.IsEmpty())
		{
			return FUnreelMCPResult::Error(TEXT("macro_instance requires macro_graph_name"));
		}
		UEdGraph* MacroGraph = nullptr;
		for (UEdGraph* ExistingGraph : Blueprint->MacroGraphs)
		{
			if (ExistingGraph && ExistingGraph->GetName() == MacroGraphName)
			{
				MacroGraph = ExistingGraph;
				break;
			}
		}
		if (!MacroGraph)
		{
			MacroGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*MacroGraphName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
			FBlueprintEditorUtils::AddMacroGraph(Blueprint, MacroGraph, true, nullptr);
		}
		UK2Node_MacroInstance* Node = NewObject<UK2Node_MacroInstance>(Graph);
		Node->SetMacroGraph(MacroGraph);
		return FinalizeNode(Node, TEXT("UnreelMCP Add Macro Instance Node"));
	}

	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unsupported common node kind: %s"), *Kind));
}

FUnreelMCPResult FUnreelMCPParityService::SetBlueprintNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString NodeIdParam;
	FString PropertyName;
	const TSharedPtr<FJsonValue>* Value = Params->Values.Find(TEXT("value"));
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("node"), NodeIdParam) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value)
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path, node, property, or value"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	UEdGraphNode* Node = FindNode(Graph, NodeIdParam);
	if (!Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Blueprint graph node"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetBlueprintNodeProperty", "Set Blueprint Node Property"));
	FString Error;
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	if (!ApplyJsonProperty(Node, PropertyName, *Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint node property set"), NodeSummary(Node));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Node->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetBlueprintPinDefault(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString NodeIdParam;
	FString PinName;
	FString Direction;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	Params->TryGetStringField(TEXT("direction"), Direction);
	if (!Blueprint || !Params->TryGetStringField(TEXT("node"), NodeIdParam) || !Params->TryGetStringField(TEXT("pin"), PinName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path, node, or pin"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	UEdGraphNode* Node = FindNode(Graph, NodeIdParam);
	UEdGraphPin* Pin = FindPin(Node, PinName, Direction);
	if (!Graph || !Node || !Pin)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve graph, node, or pin"));
	}

	FString DefaultValue;
	FString DefaultObjectPath;
	FString DefaultText;
	const bool bHasValue = Params->TryGetStringField(TEXT("default_value"), DefaultValue);
	const bool bHasObject = Params->TryGetStringField(TEXT("default_object"), DefaultObjectPath);
	const bool bHasText = Params->TryGetStringField(TEXT("default_text"), DefaultText);
	if (!bHasValue && !bHasObject && !bHasText)
	{
		return FUnreelMCPResult::Error(TEXT("Set one of default_value, default_object, or default_text"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetBlueprintPinDefault", "Set Blueprint Pin Default"));
	Node->Modify();
	if (bHasObject)
	{
		UObject* DefaultObject = LoadAssetObject(DefaultObjectPath);
		if (!DefaultObject)
		{
			DefaultObject = StaticLoadClass(UObject::StaticClass(), nullptr, *DefaultObjectPath);
		}
		if (!DefaultObject)
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("default_object"), DefaultObjectPath));
		}
		Graph->GetSchema()->TrySetDefaultObject(*Pin, DefaultObject);
	}
	if (bHasText)
	{
		const FText NewText = FText::FromString(DefaultText);
		Graph->GetSchema()->TrySetDefaultText(*Pin, NewText);
		Pin->DefaultTextValue = NewText;
	}
	if (bHasValue)
	{
		const FString Validation = Graph->GetSchema()->IsPinDefaultValid(Pin, DefaultValue, nullptr, FText::GetEmpty());
		if (!Validation.IsEmpty())
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Pin default rejected: %s"), *Validation));
		}
		Graph->GetSchema()->TrySetDefaultValue(*Pin, DefaultValue);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("node"), NodeId(Node));
	Data->SetStringField(TEXT("pin"), Pin->PinName.ToString());
	Data->SetStringField(TEXT("default_value"), Pin->DefaultValue);
	Data->SetStringField(TEXT("default_object"), Pin->DefaultObject ? Pin->DefaultObject->GetPathName() : TEXT(""));
	Data->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint pin default set"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ConnectBlueprintPins(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString FromNodeId;
	FString ToNodeId;
	FString FromPinName;
	FString ToPinName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("from_node"), FromNodeId) || !Params->TryGetStringField(TEXT("to_node"), ToNodeId) || !Params->TryGetStringField(TEXT("from_pin"), FromPinName) || !Params->TryGetStringField(TEXT("to_pin"), ToPinName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing Blueprint asset_path or pin connection fields"));
	}
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	UEdGraphNode* FromNode = FindNode(Graph, FromNodeId);
	UEdGraphNode* ToNode = FindNode(Graph, ToNodeId);
	UEdGraphPin* FromPin = FromNode ? FromNode->FindPin(FName(*FromPinName), EGPD_Output) : nullptr;
	UEdGraphPin* ToPin = ToNode ? ToNode->FindPin(FName(*ToPinName), EGPD_Input) : nullptr;
	if (!Graph || !FromPin || !ToPin)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve graph, nodes, or pins"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "ConnectBlueprintPins", "Connect Blueprint Pins"));
	Graph->Modify();
	const bool bConnected = Graph->GetSchema()->TryCreateConnection(FromPin, ToPin);
	if (!bConnected)
	{
		return FUnreelMCPResult::Error(TEXT("Blueprint schema rejected the pin connection"));
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint pins connected"));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::LayoutBlueprintGraph(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	UEdGraph* Graph = FindBlueprintGraph(Blueprint, GraphName);
	if (!Blueprint || !Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Blueprint graph"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "LayoutBlueprintGraph", "Layout Blueprint Graph"));
	Graph->Modify();
	for (int32 Index = 0; Index < Graph->Nodes.Num(); ++Index)
	{
		if (UEdGraphNode* Node = Graph->Nodes[Index])
		{
			Node->Modify();
			Node->NodePosX = (Index % 6) * 320;
			Node->NodePosY = (Index / 6) * 180;
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blueprint graph laid out"));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

UMaterial* FUnreelMCPParityService::LoadMaterial(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return nullptr;
	}
	return Cast<UMaterial>(LoadAssetObject(AssetPath));
}

UMaterialExpression* FUnreelMCPParityService::GetMaterialExpression(UMaterial* Material, int32 Index) const
{
	if (!Material || Index < 0 || Index >= Material->GetExpressions().Num())
	{
		return nullptr;
	}
	return Material->GetExpressions()[Index];
}

TSharedPtr<FJsonObject> FUnreelMCPParityService::ExpressionSummary(const UMaterialExpression* Expression, int32 Index) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("index"), Index);
	Json->SetStringField(TEXT("class"), Expression ? Expression->GetClass()->GetPathName() : TEXT(""));
	Json->SetStringField(TEXT("name"), Expression ? Expression->GetName() : TEXT(""));
	Json->SetStringField(TEXT("desc"), Expression ? Expression->Desc : TEXT(""));
	Json->SetNumberField(TEXT("x"), Expression ? Expression->MaterialExpressionEditorX : 0);
	Json->SetNumberField(TEXT("y"), Expression ? Expression->MaterialExpressionEditorY : 0);
	if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		Json->SetStringField(TEXT("parameter_name"), Scalar->ParameterName.ToString());
		Json->SetNumberField(TEXT("default_value"), Scalar->DefaultValue);
	}
	else if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		Json->SetStringField(TEXT("parameter_name"), Vector->ParameterName.ToString());
		TSharedPtr<FJsonObject> DefaultValue = MakeShared<FJsonObject>();
		DefaultValue->SetNumberField(TEXT("r"), Vector->DefaultValue.R);
		DefaultValue->SetNumberField(TEXT("g"), Vector->DefaultValue.G);
		DefaultValue->SetNumberField(TEXT("b"), Vector->DefaultValue.B);
		DefaultValue->SetNumberField(TEXT("a"), Vector->DefaultValue.A);
		Json->SetObjectField(TEXT("default_value"), DefaultValue);
	}
	else if (const UMaterialExpressionTextureSampleParameter2D* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
	{
		Json->SetStringField(TEXT("parameter_name"), TextureParameter->ParameterName.ToString());
		Json->SetStringField(TEXT("texture"), TextureParameter->Texture ? TextureParameter->Texture->GetPathName() : TEXT(""));
	}
	else if (const UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression))
	{
		Json->SetStringField(TEXT("texture"), TextureSample->Texture ? TextureSample->Texture->GetPathName() : TEXT(""));
	}
	else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
	{
		Json->SetStringField(TEXT("function"), FunctionCall->MaterialFunction ? FunctionCall->MaterialFunction->GetPathName() : TEXT(""));
	}
	TArray<TSharedPtr<FJsonValue>> Inputs;
	if (Expression)
	{
		UMaterialExpression* MutableExpression = const_cast<UMaterialExpression*>(Expression);
		for (FExpressionInputIterator It{ MutableExpression }; It; ++It)
		{
			const FExpressionInput* Input = It.Input;
			TSharedPtr<FJsonObject> InputJson = MakeShared<FJsonObject>();
			InputJson->SetNumberField(TEXT("index"), It.Index);
			InputJson->SetStringField(TEXT("name"), Input ? Input->InputName.ToString() : TEXT(""));
			InputJson->SetBoolField(TEXT("connected"), Input && Input->Expression);
			InputJson->SetStringField(TEXT("source_expression"), Input && Input->Expression ? Input->Expression->GetName() : TEXT(""));
			InputJson->SetStringField(TEXT("source_class"), Input && Input->Expression ? Input->Expression->GetClass()->GetPathName() : TEXT(""));
			InputJson->SetNumberField(TEXT("source_output"), Input ? Input->OutputIndex : INDEX_NONE);
			Inputs.Add(MakeShared<FJsonValueObject>(InputJson));
		}
	}
	Json->SetArrayField(TEXT("inputs"), Inputs);
	return Json;
}

FUnreelMCPResult FUnreelMCPParityService::ListMaterialGraph(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	if (!Material)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Material from asset_path"));
	}
	TArray<TSharedPtr<FJsonValue>> Expressions;
	for (int32 Index = 0; Index < Material->GetExpressions().Num(); ++Index)
	{
		Expressions.Add(MakeShared<FJsonValueObject>(ExpressionSummary(Material->GetExpressions()[Index], Index)));
	}
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Material->GetPathName());
	Data->SetArrayField(TEXT("expressions"), Expressions);
	return FUnreelMCPResult::Success(TEXT("Material graph loaded"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddMaterialExpression(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	FString ClassPath;
	if (!Material || !Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Material asset_path or class_path"));
	}
	UClass* Class = StaticLoadClass(UMaterialExpression::StaticClass(), nullptr, *ClassPath);
	if (!Class)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("material expression class"), ClassPath));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMaterialExpression", "Add Material Expression"));
	Material->Modify();
	UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, ReadInt(Params, TEXT("x"), 0), ReadInt(Params, TEXT("y"), 0));
	if (!Expression)
	{
		return FUnreelMCPResult::Error(TEXT("Material expression creation failed"));
	}
	const TSharedPtr<FJsonObject>* PropertiesField = nullptr;
	TSharedPtr<FJsonObject> Properties;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesField) && PropertiesField)
	{
		Properties = *PropertiesField;
	}
	if (Properties.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
		{
			FString Error;
			if (!ApplyJsonProperty(Expression, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material expression added"), ExpressionSummary(Expression, Material->GetExpressions().Find(Expression)));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddMaterialParameter(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	FString Type;
	FString ParameterName;
	if (!Material || !Params->TryGetStringField(TEXT("type"), Type) || !Params->TryGetStringField(TEXT("parameter"), ParameterName) || ParameterName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing material asset_path, type, or parameter"));
	}

	UClass* ExpressionClass = nullptr;
	if (Type.Equals(TEXT("scalar"), ESearchCase::IgnoreCase))
	{
		ExpressionClass = UMaterialExpressionScalarParameter::StaticClass();
	}
	else if (Type.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
	{
		ExpressionClass = UMaterialExpressionVectorParameter::StaticClass();
	}
	else if (Type.Equals(TEXT("texture"), ESearchCase::IgnoreCase))
	{
		ExpressionClass = UMaterialExpressionTextureSampleParameter2D::StaticClass();
	}
	else
	{
		return FUnreelMCPResult::Error(TEXT("type must be scalar, vector, or texture"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMaterialParameter", "Add Material Parameter"));
	Material->Modify();
	UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionClass, ReadInt(Params, TEXT("x"), 0), ReadInt(Params, TEXT("y"), 0));
	if (!Expression)
	{
		return FUnreelMCPResult::Error(TEXT("CreateMaterialExpression returned null"));
	}
	Expression->SetParameterName(FName(*ParameterName));
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		double Number = Scalar->DefaultValue;
		if (Value.IsValid())
		{
			Value->TryGetNumber(Number);
		}
		Scalar->DefaultValue = static_cast<float>(Number);
	}
	else if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		const TSharedPtr<FJsonObject>* ObjectValue = nullptr;
		if (Params->TryGetObjectField(TEXT("value"), ObjectValue) && ObjectValue && ObjectValue->IsValid())
		{
			double R = Vector->DefaultValue.R, G = Vector->DefaultValue.G, B = Vector->DefaultValue.B, A = Vector->DefaultValue.A;
			(*ObjectValue)->TryGetNumberField(TEXT("r"), R);
			(*ObjectValue)->TryGetNumberField(TEXT("g"), G);
			(*ObjectValue)->TryGetNumberField(TEXT("b"), B);
			(*ObjectValue)->TryGetNumberField(TEXT("a"), A);
			Vector->DefaultValue = FLinearColor(R, G, B, A);
		}
	}
	else if (UMaterialExpressionTextureSampleParameter2D* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
	{
		FString TexturePath;
		if (Params->TryGetStringField(TEXT("value"), TexturePath) && !TexturePath.IsEmpty())
		{
			UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
			if (!Texture)
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load texture: %s"), *TexturePath));
			}
			TextureParameter->Texture = Texture;
			TextureParameter->AutoSetSampleType();
		}
	}
	Material->PostEditChange();
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material parameter added"), ExpressionSummary(Expression, Material->GetExpressions().Find(Expression)));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetMaterialTexture(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	FString TexturePath;
	if (!Material || !Params->TryGetStringField(TEXT("texture"), TexturePath) || TexturePath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing material asset_path or texture"));
	}
	UMaterialExpression* Expression = GetMaterialExpression(Material, ReadInt(Params, TEXT("expression_index"), INDEX_NONE));
	UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);
	if (!TextureSample)
	{
		return FUnreelMCPResult::Error(TEXT("Expression is not a texture sample"));
	}
	UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
	if (!Texture)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load texture: %s"), *TexturePath));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetMaterialTexture", "Set Material Texture"));
	Material->Modify();
	TextureSample->Modify();
	TextureSample->Texture = Texture;
	TextureSample->AutoSetSampleType();
	Material->PostEditChange();
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material texture set"), ExpressionSummary(TextureSample, Material->GetExpressions().Find(TextureSample)));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddMaterialFunctionCall(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	FString FunctionPath;
	if (!Material || !Params->TryGetStringField(TEXT("function_path"), FunctionPath) || FunctionPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing material asset_path or function_path"));
	}
	UMaterialFunctionInterface* Function = LoadObject<UMaterialFunctionInterface>(nullptr, *FunctionPath);
	if (!Function)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load material function: %s"), *FunctionPath));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMaterialFunctionCall", "Add Material Function Call"));
	Material->Modify();
	UMaterialExpressionMaterialFunctionCall* Expression = Cast<UMaterialExpressionMaterialFunctionCall>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMaterialFunctionCall::StaticClass(), ReadInt(Params, TEXT("x"), 0), ReadInt(Params, TEXT("y"), 0)));
	if (!Expression)
	{
		return FUnreelMCPResult::Error(TEXT("CreateMaterialExpression returned null"));
	}
	if (!Expression->SetMaterialFunction(Function))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("SetMaterialFunction failed: %s"), *FunctionPath));
	}
	Expression->UpdateFromFunctionResource(true);
	Material->PostEditChange();
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material function call added"), ExpressionSummary(Expression, Material->GetExpressions().Find(Expression)));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetMaterialExpressionProperty(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	const int32 Index = ReadInt(Params, TEXT("expression_index"), INDEX_NONE);
	UMaterialExpression* Expression = GetMaterialExpression(Material, Index);
	FString PropertyName;
	const TSharedPtr<FJsonValue>* Value = Params->Values.Find(TEXT("value"));
	if (!Expression || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value)
	{
		return FUnreelMCPResult::Error(TEXT("Missing material expression, property, or value"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetMaterialExpressionProperty", "Set Material Expression Property"));
	Expression->Modify();
	FString Error;
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Expression, PropertyName);
	if (!ApplyJsonProperty(Expression, PropertyName, *Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Expression, PropertyName);
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material expression property set"), ExpressionSummary(Expression, Index));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Expression->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ConnectMaterialExpressions(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	UMaterialExpression* From = GetMaterialExpression(Material, ReadInt(Params, TEXT("from_expression"), INDEX_NONE));
	UMaterialExpression* To = GetMaterialExpression(Material, ReadInt(Params, TEXT("to_expression"), INDEX_NONE));
	FString FromOutput;
	FString ToInput;
	Params->TryGetStringField(TEXT("from_output"), FromOutput);
	if (!From || !To || !Params->TryGetStringField(TEXT("to_input"), ToInput))
	{
		return FUnreelMCPResult::Error(TEXT("Missing material expression connection fields"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "ConnectMaterialExpressions", "Connect Material Expressions"));
	const bool bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput);
	if (!bConnected)
	{
		return FUnreelMCPResult::Error(TEXT("Material editor rejected expression connection"));
	}
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material expressions connected"));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetMaterialOutput(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	UMaterialExpression* Expression = GetMaterialExpression(Material, ReadInt(Params, TEXT("expression_index"), INDEX_NONE));
	FString FromOutput;
	FString PropertyName;
	Params->TryGetStringField(TEXT("from_output"), FromOutput);
	if (!Expression || !Params->TryGetStringField(TEXT("property"), PropertyName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing material expression or output property"));
	}
	FString EnumName = PropertyName.StartsWith(TEXT("MP_")) ? PropertyName : TEXT("MP_") + PropertyName;
	const UEnum* Enum = StaticEnum<EMaterialProperty>();
	const int64 EnumValue = Enum ? Enum->GetValueByNameString(EnumName) : INDEX_NONE;
	if (EnumValue == INDEX_NONE)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown material property: %s"), *PropertyName));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetMaterialOutput", "Set Material Output"));
	const bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(Expression, FromOutput, static_cast<EMaterialProperty>(EnumValue));
	if (!bConnected)
	{
		return FUnreelMCPResult::Error(TEXT("Material editor rejected output connection"));
	}
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material output connected"));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::LayoutMaterialGraph(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	if (!Material)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Material from asset_path"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "LayoutMaterialGraph", "Layout Material Graph"));
	UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material graph laid out"));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::CompileMaterial(const TSharedRef<FJsonObject>& Params) const
{
	UMaterial* Material = LoadMaterial(Params);
	if (!Material)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Material from asset_path"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "CompileMaterial", "Compile Material"));
	UMaterialEditingLibrary::RecompileMaterial(Material);
	Material->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Material recompiled"));
	Result.ChangedAssets.Add(Material->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ListInputMappings(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UInputMappingContext* Context = Cast<UInputMappingContext>(LoadAssetObject(AssetPath));
	if (!Context)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Input Mapping Context"));
	}
	TArray<TSharedPtr<FJsonValue>> Mappings;
	const TArray<FEnhancedActionKeyMapping>& RawMappings = Context->GetMappings();
	for (int32 Index = 0; Index < RawMappings.Num(); ++Index)
	{
		const FEnhancedActionKeyMapping& Mapping = RawMappings[Index];
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("index"), Index);
		Json->SetStringField(TEXT("action"), Mapping.Action ? Mapping.Action->GetPathName() : TEXT(""));
		Json->SetStringField(TEXT("key"), Mapping.Key.ToString());
		Json->SetNumberField(TEXT("triggers"), Mapping.Triggers.Num());
		Json->SetNumberField(TEXT("modifiers"), Mapping.Modifiers.Num());
		TArray<TSharedPtr<FJsonValue>> TriggerObjects;
		for (int32 TriggerIndex = 0; TriggerIndex < Mapping.Triggers.Num(); ++TriggerIndex)
		{
			const UInputTrigger* Trigger = Mapping.Triggers[TriggerIndex];
			TSharedPtr<FJsonObject> TriggerJson = MakeShared<FJsonObject>();
			TriggerJson->SetNumberField(TEXT("index"), TriggerIndex);
			TriggerJson->SetStringField(TEXT("class"), Trigger ? Trigger->GetClass()->GetPathName() : TEXT(""));
			TriggerJson->SetStringField(TEXT("name"), Trigger ? Trigger->GetName() : TEXT(""));
			TriggerObjects.Add(MakeShared<FJsonValueObject>(TriggerJson));
		}
		Json->SetArrayField(TEXT("trigger_objects"), TriggerObjects);
		TArray<TSharedPtr<FJsonValue>> ModifierObjects;
		for (int32 ModifierIndex = 0; ModifierIndex < Mapping.Modifiers.Num(); ++ModifierIndex)
		{
			const UInputModifier* Modifier = Mapping.Modifiers[ModifierIndex];
			TSharedPtr<FJsonObject> ModifierJson = MakeShared<FJsonObject>();
			ModifierJson->SetNumberField(TEXT("index"), ModifierIndex);
			ModifierJson->SetStringField(TEXT("class"), Modifier ? Modifier->GetClass()->GetPathName() : TEXT(""));
			ModifierJson->SetStringField(TEXT("name"), Modifier ? Modifier->GetName() : TEXT(""));
			ModifierObjects.Add(MakeShared<FJsonValueObject>(ModifierJson));
		}
		Json->SetArrayField(TEXT("modifier_objects"), ModifierObjects);
		Mappings.Add(MakeShared<FJsonValueObject>(Json));
	}
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("mappings"), Mappings);
	return FUnreelMCPResult::Success(TEXT("Input mappings loaded"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddInputMapping(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ActionPath;
	FString KeyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("action_path"), ActionPath) || !Params->TryGetStringField(TEXT("key"), KeyName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, action_path, or key"));
	}
	UInputMappingContext* Context = Cast<UInputMappingContext>(LoadAssetObject(AssetPath));
	const UInputAction* Action = Cast<UInputAction>(LoadAssetObject(ActionPath));
	const FKey Key(*KeyName);
	if (!Context || !Action || !Key.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Input Mapping Context, Input Action, or key"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddInputMapping", "Add Input Mapping"));
	Context->Modify();
	Context->MapKey(Action, Key);
	Context->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Input mapping added"));
	Result.ChangedAssets.Add(Context->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveInputMapping(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ActionPath;
	FString KeyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("action_path"), ActionPath) || !Params->TryGetStringField(TEXT("key"), KeyName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, action_path, or key"));
	}
	UInputMappingContext* Context = Cast<UInputMappingContext>(LoadAssetObject(AssetPath));
	const UInputAction* Action = Cast<UInputAction>(LoadAssetObject(ActionPath));
	const FKey Key(*KeyName);
	if (!Context || !Action || !Key.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Input Mapping Context, Input Action, or key"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveInputMapping", "Remove Input Mapping"));
	Context->Modify();
	Context->UnmapKey(Action, Key);
	Context->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Input mapping removed"));
	Result.ChangedAssets.Add(Context->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetInputActionValueType(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ValueType;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("value_type"), ValueType))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or value_type"));
	}
	UInputAction* Action = Cast<UInputAction>(LoadAssetObject(AssetPath));
	const UEnum* Enum = StaticEnum<EInputActionValueType>();
	FString EnumName = ValueType.StartsWith(TEXT("EInputActionValueType::")) ? ValueType : TEXT("EInputActionValueType::") + ValueType;
	int64 EnumValue = Enum ? Enum->GetValueByNameString(EnumName) : INDEX_NONE;
	if (EnumValue == INDEX_NONE && Enum)
	{
		EnumValue = Enum->GetValueByNameString(ValueType);
	}
	if (!Action || EnumValue == INDEX_NONE)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Input Action or value_type"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetInputActionValueType", "Set Input Action Value Type"));
	Action->Modify();
	Action->ValueType = static_cast<EInputActionValueType>(EnumValue);
	Action->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Input Action value type set"));
	Result.ChangedAssets.Add(Action->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetInputMappingObjects(const TSharedRef<FJsonObject>& Params, bool bTriggers) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::MissingAssetPathMessage());
	}
	UInputMappingContext* Context = Cast<UInputMappingContext>(LoadAssetObject(AssetPath));
	const int32 MappingIndex = ReadInt(Params, TEXT("mapping_index"), INDEX_NONE);
	if (!Context || MappingIndex < 0 || MappingIndex >= Context->GetMappings().Num())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Input Mapping Context or mapping index"));
	}
	const TArray<TSharedPtr<FJsonValue>>* Classes = nullptr;
	Params->TryGetArrayField(TEXT("classes"), Classes);
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetInputMappingObjects", "Set Input Mapping Objects"));
	Context->Modify();
	FEnhancedActionKeyMapping& Mapping = Context->GetMapping(MappingIndex);
	if (bTriggers)
	{
		Mapping.Triggers.Reset();
	}
	else
	{
		Mapping.Modifiers.Reset();
	}
	if (Classes)
	{
		for (const TSharedPtr<FJsonValue>& ClassValue : *Classes)
		{
			const FString ClassPath = ClassValue.IsValid() ? ClassValue->AsString() : FString();
			UClass* Class = StaticLoadClass(bTriggers ? UInputTrigger::StaticClass() : UInputModifier::StaticClass(), nullptr, *ClassPath);
			if (!Class)
			{
				return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("input object class"), ClassPath));
			}
			if (bTriggers)
			{
				Mapping.Triggers.Add(NewObject<UInputTrigger>(Context, Class));
			}
			else
			{
				Mapping.Modifiers.Add(NewObject<UInputModifier>(Context, Class));
			}
		}
	}
	Context->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(bTriggers ? TEXT("Input mapping triggers set") : TEXT("Input mapping modifiers set"));
	Result.ChangedAssets.Add(Context->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetInputMappingObjectProperty(const TSharedRef<FJsonObject>& Params, bool bTriggers) const
{
	FString AssetPath;
	FString PropertyName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("property"), PropertyName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or property"));
	}
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: value"));
	}
	UInputMappingContext* Context = Cast<UInputMappingContext>(LoadAssetObject(AssetPath));
	const int32 MappingIndex = ReadInt(Params, TEXT("mapping_index"), INDEX_NONE);
	const int32 ObjectIndex = ReadInt(Params, TEXT("object_index"), INDEX_NONE);
	if (!Context || MappingIndex < 0 || MappingIndex >= Context->GetMappings().Num())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve Input Mapping Context or mapping index"));
	}
	FEnhancedActionKeyMapping& Mapping = Context->GetMapping(MappingIndex);
	UObject* Target = nullptr;
	if (bTriggers)
	{
		if (ObjectIndex < 0 || ObjectIndex >= Mapping.Triggers.Num())
		{
			return FUnreelMCPResult::Error(TEXT("Failed to resolve trigger index"));
		}
		Target = Mapping.Triggers[ObjectIndex];
	}
	else
	{
		if (ObjectIndex < 0 || ObjectIndex >= Mapping.Modifiers.Num())
		{
			return FUnreelMCPResult::Error(TEXT("Failed to resolve modifier index"));
		}
		Target = Mapping.Modifiers[ObjectIndex];
	}
	if (!Target)
	{
		return FUnreelMCPResult::Error(TEXT("Input mapping object is null"));
	}
	FString Error;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetInputMappingObjectProperty", "Set Input Mapping Object Property"));
	Context->Modify();
	Target->Modify();
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Target, PropertyName);
	if (!ApplyJsonProperty(Target, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Target, PropertyName);
	Context->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(bTriggers ? TEXT("Input trigger property set") : TEXT("Input modifier property set"), MakeShared<FJsonObject>());
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Target->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Context->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::InspectAnimBlueprint(const TSharedRef<FJsonObject>& Params) const
{
	return ListBlueprintGraphs(Params);
}

FUnreelMCPResult FUnreelMCPParityService::InspectAnimGraph(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load Animation Blueprint from asset_path"));
	}

	bool bIncludeAllGraphs = false;
	Params->TryGetBoolField(TEXT("include_all_graphs"), bIncludeAllGraphs);

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	TArray<TSharedPtr<FJsonValue>> GraphJson;
	TArray<TSharedPtr<FJsonValue>> StateMachines;
	int32 AnimGraphCount = 0;
	int32 StateMachineGraphCount = 0;
	int32 AnimNodeCount = 0;
	int32 PosePinCount = 0;
	int32 PoseLinkCount = 0;
	int32 StateMachineNodeCount = 0;
	int32 AssetPlayerNodeCount = 0;

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		const FString Type = GraphType(Graph);
		if (!bIncludeAllGraphs && Type == TEXT("other"))
		{
			continue;
		}

		if (Type == TEXT("anim_graph"))
		{
			++AnimGraphCount;
		}
		else if (Type == TEXT("state_machine"))
		{
			++StateMachineGraphCount;
		}

		TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
		GraphObject->SetStringField(TEXT("name"), Graph->GetName());
		GraphObject->SetStringField(TEXT("type"), Type);
		GraphObject->SetStringField(TEXT("class"), Graph->GetClass()->GetPathName());
		GraphObject->SetStringField(TEXT("schema"), Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetPathName() : TEXT(""));

		TArray<TSharedPtr<FJsonValue>> Nodes;
		int32 GraphAnimNodes = 0;
		int32 GraphPosePins = 0;
		int32 GraphPoseLinks = 0;
		int32 GraphStateMachineNodes = 0;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			TSharedPtr<FJsonObject> NodeObject = NodeSummary(Node);
			TSharedPtr<FJsonObject> Semantics = MakeShared<FJsonObject>();

			UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
			UAnimGraphNode_AssetPlayerBase* AssetPlayerNode = Cast<UAnimGraphNode_AssetPlayerBase>(Node);
			UAnimGraphNode_StateMachineBase* StateMachineNode = Cast<UAnimGraphNode_StateMachineBase>(Node);
			UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node);
			UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node);

			Semantics->SetBoolField(TEXT("is_anim_node"), AnimNode != nullptr);
			Semantics->SetBoolField(TEXT("is_asset_player"), AssetPlayerNode != nullptr);
			Semantics->SetBoolField(TEXT("is_state_machine_node"), StateMachineNode != nullptr);
			Semantics->SetBoolField(TEXT("is_state_node"), StateNode != nullptr);
			Semantics->SetBoolField(TEXT("is_transition_node"), TransitionNode != nullptr);

			if (AnimNode)
			{
				++AnimNodeCount;
				++GraphAnimNodes;
				Semantics->SetStringField(TEXT("category"), AnimNode->GetNodeCategory());
				Semantics->SetBoolField(TEXT("is_sink"), AnimNode->IsSinkNode());
				Semantics->SetBoolField(TEXT("pose_watchable"), AnimNode->IsPoseWatchable());
				UAnimationAsset* AnimationAsset = AnimNode->GetAnimationAsset();
				Semantics->SetStringField(TEXT("animation_asset"), AnimationAsset ? AnimationAsset->GetPathName() : TEXT(""));
				UClass* AnimationAssetClass = AnimNode->GetAnimationAssetClass();
				Semantics->SetStringField(TEXT("animation_asset_class"), AnimationAssetClass ? AnimationAssetClass->GetPathName() : TEXT(""));
			}

			if (AssetPlayerNode)
			{
				++AssetPlayerNodeCount;
			}

			if (StateMachineNode)
			{
				++StateMachineNodeCount;
				++GraphStateMachineNodes;
				TSharedPtr<FJsonObject> MachineObject = MakeShared<FJsonObject>();
				MachineObject->SetStringField(TEXT("graph"), Graph->GetName());
				MachineObject->SetStringField(TEXT("node_id"), NodeId(Node));
				MachineObject->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				MachineObject->SetStringField(TEXT("state_machine_name"), StateMachineNode->GetStateMachineName());
				MachineObject->SetStringField(TEXT("editor_graph"), StateMachineNode->EditorStateMachineGraph ? StateMachineNode->EditorStateMachineGraph->GetName() : TEXT(""));
				StateMachines.Add(MakeShared<FJsonValueObject>(MachineObject));
			}

			if (StateNode)
			{
				Semantics->SetStringField(TEXT("state_name"), StateNode->GetStateName());
				Semantics->SetStringField(TEXT("bound_graph"), StateNode->BoundGraph ? StateNode->BoundGraph->GetName() : TEXT(""));
			}

			if (TransitionNode)
			{
				Semantics->SetStringField(TEXT("bound_graph"), TransitionNode->BoundGraph ? TransitionNode->BoundGraph->GetName() : TEXT(""));
				Semantics->SetStringField(TEXT("custom_transition_graph"), TransitionNode->CustomTransitionGraph ? TransitionNode->CustomTransitionGraph->GetName() : TEXT(""));
				Semantics->SetNumberField(TEXT("crossfade_duration"), TransitionNode->CrossfadeDuration);
				Semantics->SetNumberField(TEXT("priority_order"), TransitionNode->PriorityOrder);
			}

			TArray<TSharedPtr<FJsonValue>> PosePins;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!IsPosePin(Pin))
				{
					continue;
				}
				++PosePinCount;
				++GraphPosePins;
				TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
				PinObject->SetStringField(TEXT("name"), Pin->PinName.ToString());
				PinObject->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
				PinObject->SetStringField(TEXT("struct"), Pin->PinType.PinSubCategoryObject.IsValid() ? Pin->PinType.PinSubCategoryObject->GetPathName() : TEXT(""));

				TArray<TSharedPtr<FJsonValue>> LinkedNodes;
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !LinkedPin->GetOwningNode())
					{
						continue;
					}
					++PoseLinkCount;
					++GraphPoseLinks;
					TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
					LinkObject->SetStringField(TEXT("node_id"), NodeId(LinkedPin->GetOwningNode()));
					LinkObject->SetStringField(TEXT("node_class"), LinkedPin->GetOwningNode()->GetClass()->GetPathName());
					LinkObject->SetStringField(TEXT("node_title"), LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
					LinkObject->SetStringField(TEXT("pin"), LinkedPin->PinName.ToString());
					LinkedNodes.Add(MakeShared<FJsonValueObject>(LinkObject));
				}
				PinObject->SetNumberField(TEXT("linked_to"), LinkedNodes.Num());
				PinObject->SetArrayField(TEXT("linked_nodes"), LinkedNodes);
				PosePins.Add(MakeShared<FJsonValueObject>(PinObject));
			}
			Semantics->SetArrayField(TEXT("pose_pins"), PosePins);
			NodeObject->SetObjectField(TEXT("anim_semantics"), Semantics);
			Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
		}

		GraphObject->SetNumberField(TEXT("node_count"), Nodes.Num());
		GraphObject->SetNumberField(TEXT("anim_node_count"), GraphAnimNodes);
		GraphObject->SetNumberField(TEXT("pose_pin_count"), GraphPosePins);
		GraphObject->SetNumberField(TEXT("pose_link_count"), GraphPoseLinks);
		GraphObject->SetNumberField(TEXT("state_machine_node_count"), GraphStateMachineNodes);
		GraphObject->SetArrayField(TEXT("nodes"), Nodes);
		GraphJson.Add(MakeShared<FJsonValueObject>(GraphObject));
	}

	TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
	Summary->SetNumberField(TEXT("graph_count"), GraphJson.Num());
	Summary->SetNumberField(TEXT("anim_graph_count"), AnimGraphCount);
	Summary->SetNumberField(TEXT("state_machine_graph_count"), StateMachineGraphCount);
	Summary->SetNumberField(TEXT("anim_node_count"), AnimNodeCount);
	Summary->SetNumberField(TEXT("asset_player_node_count"), AssetPlayerNodeCount);
	Summary->SetNumberField(TEXT("state_machine_node_count"), StateMachineNodeCount);
	Summary->SetNumberField(TEXT("pose_pin_count"), PosePinCount);
	Summary->SetNumberField(TEXT("pose_link_count"), PoseLinkCount);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Blueprint->GetPathName());
	Data->SetArrayField(TEXT("anim_graphs"), GraphJson);
	Data->SetArrayField(TEXT("state_machines"), StateMachines);
	Data->SetObjectField(TEXT("summary"), Summary);

	return FUnreelMCPResult::Success(TEXT("AnimGraph inspection loaded"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddAnimGraphNodeByClass(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString ClassPath;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing Animation Blueprint asset_path or class_path"));
	}

	UEdGraph* Graph = FindAnimGraph(Blueprint, GraphName);
	UClass* NodeClass = StaticLoadClass(UAnimGraphNode_Base::StaticClass(), nullptr, *ClassPath);
	if (!Graph || !NodeClass || !NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass()) || NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve animation graph or concrete UAnimGraphNode_Base class: %s.%s"), *ClassPath, *UnreelMCPServiceUtils::ScriptLoadHint(ClassPath)));
	}
	const FString Type = GraphType(Graph);
	if (Type == TEXT("other"))
	{
		return FUnreelMCPResult::Error(TEXT("Target graph is not an animation graph"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddAnimGraphNodeByClass", "Add AnimGraph Node By Class"));
	Graph->Modify();
	UAnimGraphNode_Base* Node = NewObject<UAnimGraphNode_Base>(Graph, NodeClass, NAME_None, RF_Transactional);
	Node->NodePosX = ReadInt(Params, TEXT("x"), 0);
	Node->NodePosY = ReadInt(Params, TEXT("y"), 0);

	const TSharedPtr<FJsonObject>* PropertiesField = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesField) && PropertiesField && PropertiesField->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropertiesField)->Values)
		{
			FString Error;
			if (!ApplyJsonProperty(Node, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}

	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();

	FString AnimationAssetPath;
	if (Params->TryGetStringField(TEXT("animation_asset"), AnimationAssetPath) && !AnimationAssetPath.IsEmpty())
	{
		UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(LoadAssetObject(AnimationAssetPath));
		UAnimGraphNode_AssetPlayerBase* AssetPlayerNode = Cast<UAnimGraphNode_AssetPlayerBase>(Node);
		if (!AnimationAsset || !AssetPlayerNode)
		{
			return FUnreelMCPResult::Error(TEXT("animation_asset requires a valid animation asset and a UAnimGraphNode_AssetPlayerBase node"));
		}
		AssetPlayerNode->SetAnimationAsset(AnimationAsset);
		Node->ReconstructNode();
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("AnimGraph node added"), NodeSummary(Node));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetAnimGraphNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString NodeIdParam;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("node"), NodeIdParam) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node, property, or value"));
	}
	UEdGraph* Graph = FindAnimGraph(Blueprint, GraphName);
	UEdGraphNode* Node = FindNode(Graph, NodeIdParam);
	if (!Graph || !Node || !Cast<UAnimGraphNode_Base>(Node))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find AnimGraph node"));
	}

	FString Error;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetAnimGraphNodeProperty", "Set AnimGraph Node Property"));
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	if (!ApplyJsonProperty(Node, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("AnimGraph node property set"), NodeSummary(Node));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Node->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetAnimGraphNodeAnimationAsset(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString NodeIdParam;
	FString AnimationAssetPath;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint || !Params->TryGetStringField(TEXT("node"), NodeIdParam) || !Params->TryGetStringField(TEXT("animation_asset"), AnimationAssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node, or animation_asset"));
	}
	UEdGraph* Graph = FindAnimGraph(Blueprint, GraphName);
	UAnimGraphNode_AssetPlayerBase* Node = Cast<UAnimGraphNode_AssetPlayerBase>(FindNode(Graph, NodeIdParam));
	UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(LoadAssetObject(AnimationAssetPath));
	if (!Graph || !Node || !AnimationAsset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find asset-player AnimGraph node or animation asset"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetAnimGraphNodeAnimationAsset", "Set AnimGraph Node Animation Asset"));
	Node->Modify();
	Node->SetAnimationAsset(AnimationAsset);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("AnimGraph node animation asset set"), NodeSummary(Node));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ConnectAnimGraphPosePins(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString FromNodeId;
	FString FromPinName;
	FString ToNodeId;
	FString ToPinName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	if (!Blueprint ||
		!Params->TryGetStringField(TEXT("from_node"), FromNodeId) ||
		!Params->TryGetStringField(TEXT("from_pin"), FromPinName) ||
		!Params->TryGetStringField(TEXT("to_node"), ToNodeId) ||
		!Params->TryGetStringField(TEXT("to_pin"), ToPinName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, from_node/from_pin, or to_node/to_pin"));
	}

	UEdGraph* Graph = FindAnimGraph(Blueprint, GraphName);
	UEdGraphNode* FromNode = FindNode(Graph, FromNodeId);
	UEdGraphNode* ToNode = FindNode(Graph, ToNodeId);
	UEdGraphPin* FromPin = FindPin(FromNode, FromPinName, TEXT("output"));
	UEdGraphPin* ToPin = FindPin(ToNode, ToPinName, TEXT("input"));
	if (!Graph || !FromNode || !ToNode || !FromPin || !ToPin)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve graph, nodes, or pins"));
	}
	if (!IsPosePin(FromPin) || !IsPosePin(ToPin))
	{
		return FUnreelMCPResult::Error(TEXT("Both pins must be pose pins"));
	}
	const UEdGraphSchema* Schema = Graph->GetSchema();
	if (!Schema)
	{
		return FUnreelMCPResult::Error(TEXT("Animation graph has no schema"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "ConnectAnimGraphPosePins", "Connect AnimGraph Pose Pins"));
	Graph->Modify();
	FromNode->Modify();
	ToNode->Modify();
	if (!Schema->TryCreateConnection(FromPin, ToPin))
	{
		return FUnreelMCPResult::Error(TEXT("Animation graph schema rejected the pose pin connection"));
	}
	FromNode->PinConnectionListChanged(FromPin);
	ToNode->PinConnectionListChanged(ToPin);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetObjectField(TEXT("from_node"), NodeSummary(FromNode));
	Data->SetObjectField(TEXT("to_node"), NodeSummary(ToNode));
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("AnimGraph pose pins connected"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddStateMachine(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString GraphName;
	FString MachineName;
	Params->TryGetStringField(TEXT("graph_name"), GraphName);
	Params->TryGetStringField(TEXT("name"), MachineName);
	if (!Blueprint)
	{
		return FUnreelMCPResult::Error(TEXT("Missing Animation Blueprint asset_path"));
	}
	UEdGraph* Graph = FindAnimGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find AnimGraph"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddStateMachine", "Add Animation State Machine"));
	Graph->Modify();
	UAnimGraphNode_StateMachine* Node = NewObject<UAnimGraphNode_StateMachine>(Graph, UAnimGraphNode_StateMachine::StaticClass(), NAME_None, RF_Transactional);
	Node->NodePosX = ReadInt(Params, TEXT("x"), 0);
	Node->NodePosY = ReadInt(Params, TEXT("y"), 220);
	Graph->AddNode(Node, true, false);
	Node->CreateNewGuid();
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	if (!MachineName.IsEmpty() && Node->EditorStateMachineGraph)
	{
		FBlueprintEditorUtils::RenameGraph(Node->EditorStateMachineGraph, MachineName);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = NodeSummary(Node);
	Data->SetStringField(TEXT("state_machine_graph"), Node->EditorStateMachineGraph ? Node->EditorStateMachineGraph->GetName() : TEXT(""));
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation state machine added"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::InspectStateMachine(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state-machine graph"));
	}

	TArray<TSharedPtr<FJsonValue>> States;
	TArray<TSharedPtr<FJsonValue>> Aliases;
	TArray<TSharedPtr<FJsonValue>> Transitions;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UAnimStateAliasNode* AliasNode = Cast<UAnimStateAliasNode>(Node))
		{
			TSharedPtr<FJsonObject> Alias = NodeSummary(AliasNode);
			Alias->SetStringField(TEXT("alias_name"), AliasNode->GetStateName());
			Alias->SetBoolField(TEXT("global"), AliasNode->bGlobalAlias);
			TArray<TSharedPtr<FJsonValue>> AliasedStates;
			for (const TWeakObjectPtr<UAnimStateNodeBase>& WeakState : AliasNode->GetAliasedStates())
			{
				if (const UAnimStateNodeBase* AliasedState = WeakState.Get())
				{
					TSharedPtr<FJsonObject> State = MakeShared<FJsonObject>();
					State->SetStringField(TEXT("id"), NodeId(AliasedState));
					State->SetStringField(TEXT("name"), AliasedState->GetName());
					State->SetStringField(TEXT("state_name"), AliasedState->GetStateName());
					State->SetStringField(TEXT("class"), AliasedState->GetClass()->GetPathName());
					AliasedStates.Add(MakeShared<FJsonValueObject>(State));
				}
			}
			Alias->SetArrayField(TEXT("aliased_states"), AliasedStates);
			Alias->SetNumberField(TEXT("aliased_state_count"), AliasedStates.Num());
			Aliases.Add(MakeShared<FJsonValueObject>(Alias));
		}
		else if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
		{
			TSharedPtr<FJsonObject> State = NodeSummary(StateNode);
			State->SetStringField(TEXT("state_name"), StateNode->GetStateName());
			State->SetStringField(TEXT("bound_graph"), StateNode->BoundGraph ? StateNode->BoundGraph->GetName() : TEXT(""));
			States.Add(MakeShared<FJsonValueObject>(State));
		}
		else if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node))
		{
			TSharedPtr<FJsonObject> Transition = NodeSummary(TransitionNode);
			Transition->SetStringField(TEXT("bound_graph"), TransitionNode->BoundGraph ? TransitionNode->BoundGraph->GetName() : TEXT(""));
			Transition->SetNumberField(TEXT("priority_order"), TransitionNode->PriorityOrder);
			Transition->SetNumberField(TEXT("crossfade_duration"), TransitionNode->CrossfadeDuration);
			Transitions.Add(MakeShared<FJsonValueObject>(Transition));
		}
	}

	TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
	if (Graph->EntryNode)
	{
		Entry = NodeSummary(Graph->EntryNode);
		Entry->SetNumberField(TEXT("linked_to"), Graph->EntryNode->GetOutputPin() ? Graph->EntryNode->GetOutputPin()->LinkedTo.Num() : 0);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graph"), Graph->GetName());
	Data->SetStringField(TEXT("owner_node"), Graph->OwnerAnimGraphNode ? NodeId(Graph->OwnerAnimGraphNode) : TEXT(""));
	Data->SetObjectField(TEXT("entry"), Entry);
	Data->SetArrayField(TEXT("states"), States);
	Data->SetArrayField(TEXT("aliases"), Aliases);
	Data->SetArrayField(TEXT("transitions"), Transitions);
	Data->SetNumberField(TEXT("state_count"), States.Num());
	Data->SetNumberField(TEXT("alias_count"), Aliases.Num());
	Data->SetNumberField(TEXT("transition_count"), Transitions.Num());
	return FUnreelMCPResult::Success(TEXT("Animation state machine inspected"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddStateMachineState(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	FString StateName;
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	if (!Blueprint || !Params->TryGetStringField(TEXT("name"), StateName) || StateName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or state name"));
	}
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state-machine graph"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddStateMachineState", "Add Animation State"));
	UAnimStateNode* StateNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateNode>(
		Graph,
		NewObject<UAnimStateNode>(),
		FVector2f(ReadInt(Params, TEXT("x"), 250), ReadInt(Params, TEXT("y"), 0)),
		false);
	if (!StateNode)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create animation state node"));
	}
	StateNode->OnRenameNode(StateName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation state added"), NodeSummary(StateNode));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddStateMachineAlias(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	FString AliasName;
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	if (!Blueprint || !Params->TryGetStringField(TEXT("name"), AliasName) || AliasName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or alias name"));
	}
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state-machine graph"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddStateMachineAlias", "Add Animation State Alias"));
	UAnimStateAliasNode* AliasNode = FEdGraphSchemaAction_NewStateNode::SpawnNodeFromTemplate<UAnimStateAliasNode>(
		Graph,
		NewObject<UAnimStateAliasNode>(),
		FVector2f(ReadInt(Params, TEXT("x"), 250), ReadInt(Params, TEXT("y"), 180)),
		false);
	if (!AliasNode)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to create animation state alias node"));
	}
	AliasNode->OnRenameNode(AliasName);
	bool bGlobal = false;
	if (Params->TryGetBoolField(TEXT("global"), bGlobal))
	{
		AliasNode->bGlobalAlias = bGlobal;
	}

	const TArray<TSharedPtr<FJsonValue>>* AliasedStateValues = nullptr;
	if (Params->TryGetArrayField(TEXT("aliased_states"), AliasedStateValues) && AliasedStateValues)
	{
		AliasNode->Modify();
		AliasNode->GetAliasedStates().Reset();
		for (const TSharedPtr<FJsonValue>& StateValue : *AliasedStateValues)
		{
			const FString StateKey = StateValue.IsValid() ? StateValue->AsString() : FString();
			UAnimStateNodeBase* StateNode = FindStateMachineStateNode(Graph, StateKey);
			if (!Cast<UAnimStateNode>(StateNode))
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to find concrete state for alias target: %s"), *StateKey));
			}
			AliasNode->GetAliasedStates().Add(StateNode);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation state alias added"), NodeSummary(AliasNode));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetStateMachineAlias(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	FString AliasKey;
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	if (!Blueprint || !Params->TryGetStringField(TEXT("alias"), AliasKey) || AliasKey.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or alias"));
	}
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	UAnimStateAliasNode* AliasNode = Cast<UAnimStateAliasNode>(FindStateMachineStateNode(Graph, AliasKey));
	if (!Graph || !AliasNode)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state alias node"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetStateMachineAlias", "Set Animation State Alias"));
	AliasNode->Modify();

	FString NewName;
	if (Params->TryGetStringField(TEXT("name"), NewName) && !NewName.IsEmpty())
	{
		AliasNode->OnRenameNode(NewName);
	}

	bool bGlobal = false;
	if (Params->TryGetBoolField(TEXT("global"), bGlobal))
	{
		AliasNode->bGlobalAlias = bGlobal;
	}

	const TArray<TSharedPtr<FJsonValue>>* AliasedStateValues = nullptr;
	if (Params->TryGetArrayField(TEXT("aliased_states"), AliasedStateValues) && AliasedStateValues)
	{
		AliasNode->GetAliasedStates().Reset();
		for (const TSharedPtr<FJsonValue>& StateValue : *AliasedStateValues)
		{
			const FString StateKey = StateValue.IsValid() ? StateValue->AsString() : FString();
			UAnimStateNodeBase* StateNode = FindStateMachineStateNode(Graph, StateKey);
			if (!Cast<UAnimStateNode>(StateNode))
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to find concrete state for alias target: %s"), *StateKey));
			}
			AliasNode->GetAliasedStates().Add(StateNode);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation state alias updated"), NodeSummary(AliasNode));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ConnectStateMachineStates(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	FString From;
	FString To;
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	if (!Blueprint || !Params->TryGetStringField(TEXT("from"), From) || !Params->TryGetStringField(TEXT("to"), To))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, from, or to"));
	}
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state-machine graph"));
	}

	UEdGraphPin* FromPin = nullptr;
	UEdGraphPin* ToPin = nullptr;
	if (From.Equals(TEXT("Entry"), ESearchCase::IgnoreCase))
	{
		FromPin = Graph->EntryNode ? Graph->EntryNode->GetOutputPin() : nullptr;
	}
	else if (UAnimStateNodeBase* FromNode = FindStateMachineStateNode(Graph, From))
	{
		FromPin = FromNode->GetOutputPin();
	}
	if (UAnimStateNodeBase* ToNode = FindStateMachineStateNode(Graph, To))
	{
		ToPin = ToNode->GetInputPin();
	}
	if (!FromPin || !ToPin || !Graph->GetSchema())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve state-machine connection pins"));
	}

	const int32 BeforeTransitions = Algo::CountIf(Graph->Nodes, [](const UEdGraphNode* Node) { return Cast<UAnimStateTransitionNode>(Node) != nullptr; });
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "ConnectStateMachineStates", "Connect Animation States"));
	Graph->Modify();
	if (!Graph->GetSchema()->TryCreateConnection(FromPin, ToPin))
	{
		return FUnreelMCPResult::Error(TEXT("State-machine schema rejected the connection"));
	}
	const int32 AfterTransitions = Algo::CountIf(Graph->Nodes, [](const UEdGraphNode* Node) { return Cast<UAnimStateTransitionNode>(Node) != nullptr; });
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graph"), Graph->GetName());
	Data->SetNumberField(TEXT("transition_count_before"), BeforeTransitions);
	Data->SetNumberField(TEXT("transition_count_after"), AfterTransitions);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation states connected"), Data);
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetStateMachineNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	UBlueprint* Blueprint = LoadBlueprint(Params);
	FString Machine;
	FString NodeKey;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	Params->TryGetStringField(TEXT("state_machine"), Machine);
	if (!Blueprint || !Params->TryGetStringField(TEXT("node"), NodeKey) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, node, property, or value"));
	}
	UAnimationStateMachineGraph* Graph = FindStateMachineGraph(Blueprint, Machine);
	UEdGraphNode* Node = FindNode(Graph, NodeKey);
	if (!Node && Graph)
	{
		for (UEdGraphNode* Candidate : Graph->Nodes)
		{
			if (UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(Candidate))
			{
				if (StateNode->GetStateName() == NodeKey)
				{
					Node = StateNode;
					break;
				}
			}
		}
	}
	if (!Graph || !Node)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to find state-machine node"));
	}

	FString Error;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetStateMachineNodeProperty", "Set State Machine Node Property"));
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	if (!ApplyJsonProperty(Node, PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Node, PropertyName);
	Node->ReconstructNode();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("State-machine node property set"), NodeSummary(Node));
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Node->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Blueprint->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ListAnimationMetadata(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimationAsset* Asset = Cast<UAnimationAsset>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load animation asset"));
	}

	TArray<TSharedPtr<FJsonValue>> Items;
	const TArray<UAnimMetaData*>& Metadata = Asset->GetMetaData();
	for (int32 Index = 0; Index < Metadata.Num(); ++Index)
	{
		UAnimMetaData* Entry = Metadata[Index];
		if (!Entry)
		{
			continue;
		}
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("index"), Index);
		Item->SetStringField(TEXT("name"), Entry->GetName());
		Item->SetStringField(TEXT("class"), Entry->GetClass()->GetPathName());
		Item->SetStringField(TEXT("path"), Entry->GetPathName());

		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Entry->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property || Property->HasAnyPropertyFlags(CPF_Transient))
			{
				continue;
			}
			FString Exported;
			Property->ExportTextItem_Direct(Exported, Property->ContainerPtrToValuePtr<void>(Entry), nullptr, Entry, PPF_None);
			Properties->SetStringField(Property->GetName(), Exported);
		}
		Item->SetObjectField(TEXT("properties"), Properties);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Asset->GetPathName());
	Data->SetArrayField(TEXT("metadata"), Items);
	Data->SetNumberField(TEXT("count"), Items.Num());
	return FUnreelMCPResult::Success(TEXT("Animation metadata listed"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddAnimationMetadata(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ClassPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or class_path"));
	}
	UAnimationAsset* Asset = Cast<UAnimationAsset>(LoadAssetObject(AssetPath));
	UClass* MetadataClass = ResolveAnimationMetadataClass(ClassPath);
	if (!Asset)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load animation asset: %s"), *AssetPath));
	}
	if (!MetadataClass)
	{
		return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("concrete UAnimMetaData class"), ClassPath));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddAnimationMetadata", "Add Animation Metadata"));
	Asset->Modify();
	UAnimMetaData* Metadata = NewObject<UAnimMetaData>(Asset, MetadataClass, NAME_None, RF_Transactional);
	const TSharedPtr<FJsonObject>* PropertiesField = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesField) && PropertiesField && PropertiesField->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*PropertiesField)->Values)
		{
			FString Error;
			if (!ApplyJsonProperty(Metadata, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}
	Asset->AddMetaData(Metadata);
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("index"), Asset->GetMetaData().Num() - 1);
	Data->SetStringField(TEXT("class"), Metadata->GetClass()->GetPathName());
	Data->SetStringField(TEXT("path"), Metadata->GetPathName());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation metadata added"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveAnimationMetadata(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimationAsset* Asset = Cast<UAnimationAsset>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load animation asset"));
	}

	TArray<UAnimMetaData*> ToRemove;
	double IndexNumber = -1;
	FString ClassPath;
	if (Params->TryGetNumberField(TEXT("index"), IndexNumber))
	{
		const int32 Index = static_cast<int32>(IndexNumber);
		const TArray<UAnimMetaData*>& Metadata = Asset->GetMetaData();
		if (!Metadata.IsValidIndex(Index) || !Metadata[Index])
		{
			return FUnreelMCPResult::Error(TEXT("Metadata index is out of range"));
		}
		ToRemove.Add(Metadata[Index]);
	}
	else if (Params->TryGetStringField(TEXT("class_path"), ClassPath) && !ClassPath.IsEmpty())
	{
		UClass* MetadataClass = ResolveAnimationMetadataClass(ClassPath);
		if (!MetadataClass)
		{
			return FUnreelMCPResult::Error(UnreelMCPServiceUtils::LoadFailureMessage(TEXT("metadata class"), ClassPath));
		}
		for (UAnimMetaData* Entry : Asset->GetMetaData())
		{
			if (Entry && Entry->IsA(MetadataClass))
			{
				ToRemove.Add(Entry);
			}
		}
	}
	else
	{
		return FUnreelMCPResult::Error(TEXT("Provide index or class_path"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveAnimationMetadata", "Remove Animation Metadata"));
	Asset->Modify();
	Asset->RemoveMetaData(ToRemove);
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("removed"), ToRemove.Num());
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation metadata removed"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetAnimationMetadataProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	double IndexNumber = -1;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetNumberField(TEXT("index"), IndexNumber) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, index, property, or value"));
	}
	UAnimationAsset* Asset = Cast<UAnimationAsset>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load animation asset"));
	}
	const int32 Index = static_cast<int32>(IndexNumber);
	const TArray<UAnimMetaData*>& Metadata = Asset->GetMetaData();
	if (!Metadata.IsValidIndex(Index) || !Metadata[Index])
	{
		return FUnreelMCPResult::Error(TEXT("Metadata index is out of range"));
	}

	FString Error;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetAnimationMetadataProperty", "Set Animation Metadata Property"));
	Asset->Modify();
	const FString BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Metadata[Index], PropertyName);
	if (!ApplyJsonProperty(Metadata[Index], PropertyName, Value, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Metadata[Index], PropertyName);
	Asset->MarkPackageDirty();

	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation metadata property set"), MakeShared<FJsonObject>());
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(Metadata[Index]->GetPathName(), PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::ListAnimationNotifies(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load UAnimSequenceBase asset"));
	}

	TArray<TSharedPtr<FJsonValue>> Tracks;
#if WITH_EDITORONLY_DATA
	for (int32 Index = 0; Index < Asset->AnimNotifyTracks.Num(); ++Index)
	{
		const FAnimNotifyTrack& Track = Asset->AnimNotifyTracks[Index];
		TSharedPtr<FJsonObject> TrackJson = MakeShared<FJsonObject>();
		TrackJson->SetNumberField(TEXT("index"), Index);
		TrackJson->SetStringField(TEXT("name"), Track.TrackName.ToString());
		TrackJson->SetNumberField(TEXT("notify_count"), Track.Notifies.Num());
		Tracks.Add(MakeShared<FJsonValueObject>(TrackJson));
	}
#endif

	TArray<TSharedPtr<FJsonValue>> Notifies;
	for (int32 Index = 0; Index < Asset->Notifies.Num(); ++Index)
	{
		Notifies.Add(MakeShared<FJsonValueObject>(NotifySummary(Asset->Notifies[Index], Index)));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Asset->GetPathName());
	Data->SetNumberField(TEXT("length"), Asset->GetPlayLength());
	Data->SetArrayField(TEXT("tracks"), Tracks);
	Data->SetArrayField(TEXT("notifies"), Notifies);
	Data->SetNumberField(TEXT("count"), Notifies.Num());
	return FUnreelMCPResult::Success(TEXT("Animation notifies listed"), Data);
}

FUnreelMCPResult FUnreelMCPParityService::AddAnimationNotify(const TSharedRef<FJsonObject>& Params, bool bState) const
{
	FString AssetPath;
	double Time = 0.0;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetNumberField(TEXT("time"), Time))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or time"));
	}
	UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load UAnimSequenceBase asset"));
	}
	if (!IsValidNotifyTime(Asset, Time))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Notify time %.3f is outside asset range 0..%.3f"), Time, Asset->GetPlayLength()));
	}

	FString TrackName = TEXT("MCP");
	Params->TryGetStringField(TEXT("track"), TrackName);
	const int32 TrackIndex = EnsureNotifyTrack(Asset, FName(*TrackName));
	if (TrackIndex == INDEX_NONE)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve or create notify track"));
	}

	FString ClassPath;
	FString NotifyName;
	UClass* NotifyClass = nullptr;
	if (Params->TryGetStringField(TEXT("class_path"), ClassPath) && !ClassPath.IsEmpty())
	{
		NotifyClass = ResolveAnimNotifyClass(ClassPath, bState);
		if (!NotifyClass)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve concrete %s class: %s.%s"), bState ? TEXT("UAnimNotifyState") : TEXT("UAnimNotify"), *ClassPath, *UnreelMCPServiceUtils::ScriptLoadHint(ClassPath)));
		}
	}
	else if (bState)
	{
		return FUnreelMCPResult::Error(TEXT("Missing class_path for notify state"));
	}
	else if (!Params->TryGetStringField(TEXT("notify_name"), NotifyName) || NotifyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Provide class_path or notify_name"));
	}

	double Duration = 0.0;
	if (bState && (!Params->TryGetNumberField(TEXT("duration"), Duration) || Duration < 0.0 || Time + Duration > static_cast<double>(Asset->GetPlayLength())))
	{
		return FUnreelMCPResult::Error(TEXT("Notify state duration is missing or outside asset range"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddAnimationNotify", "Add Animation Notify"));
	Asset->Modify();
	FAnimNotifyEvent& NewEvent = Asset->Notifies.AddDefaulted_GetRef();
	NewEvent.Link(Asset, static_cast<float>(Time));
	NewEvent.TriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(static_cast<float>(Time)));
	NewEvent.TrackIndex = TrackIndex;
#if WITH_EDITORONLY_DATA
	NewEvent.Guid = FGuid::NewGuid();
#endif

	UObject* NotifyObject = nullptr;
	if (bState)
	{
		UAnimNotifyState* NotifyState = NewObject<UAnimNotifyState>(Asset, NotifyClass, NAME_None, RF_Transactional);
		NewEvent.NotifyStateClass = NotifyState;
		NewEvent.NotifyName = NotifyState ? FName(*NotifyState->GetNotifyName()) : NAME_None;
		NewEvent.SetDuration(static_cast<float>(Duration));
		NewEvent.EndLink.Link(Asset, NewEvent.EndLink.GetTime());
		NewEvent.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(NewEvent.EndLink.GetTime()));
		NotifyObject = NotifyState;
	}
	else if (NotifyClass)
	{
		UAnimNotify* Notify = NewObject<UAnimNotify>(Asset, NotifyClass, NAME_None, RF_Transactional);
		NewEvent.Notify = Notify;
		NewEvent.NotifyName = Notify ? FName(*Notify->GetNotifyName()) : NAME_None;
		NotifyObject = Notify;
	}
	else
	{
		NewEvent.NotifyName = FName(*NotifyName);
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (NotifyObject && Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Properties)->Values)
		{
			FString Error;
			if (!ApplyJsonProperty(NotifyObject, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}
	const TSharedPtr<FJsonObject>* EventProperties = nullptr;
	if (Params->TryGetObjectField(TEXT("event_properties"), EventProperties) && EventProperties && EventProperties->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*EventProperties)->Values)
		{
			FString Error;
			if (!ApplyJsonStructProperty(FAnimNotifyEvent::StaticStruct(), &NewEvent, Pair.Key, Pair.Value, Error))
			{
				return FUnreelMCPResult::Error(Error);
			}
		}
	}

	const FGuid Guid = NewEvent.Guid;
	const FName AddedNotifyName = NewEvent.NotifyName;
	UClass* AddedNotifyClass = NewEvent.Notify ? NewEvent.Notify->GetClass() : nullptr;
	UClass* AddedNotifyStateClass = NewEvent.NotifyStateClass ? NewEvent.NotifyStateClass->GetClass() : nullptr;
	Asset->SortNotifies();
	Asset->RefreshCacheData();
	Asset->MarkPackageDirty();
	int32 FinalIndex = FindNotifyIndexByGuid(Asset, Guid);
	if (!Asset->Notifies.IsValidIndex(FinalIndex))
	{
		FinalIndex = FindNotifyIndexBySignature(Asset, AddedNotifyName, AddedNotifyClass, AddedNotifyStateClass, Time);
	}
	if (!Asset->Notifies.IsValidIndex(FinalIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Notify was added but could not be resolved after sorting"));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("index"), FinalIndex);
	Data->SetStringField(TEXT("guid"), Guid.ToString(EGuidFormats::DigitsWithHyphens));
	Data->SetObjectField(TEXT("notify"), NotifySummary(Asset->Notifies[FinalIndex], FinalIndex));
	FUnreelMCPResult Result = FUnreelMCPResult::Success(bState ? TEXT("Animation notify state added") : TEXT("Animation notify added"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::MoveAnimationNotify(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(LoadAssetObject(AssetPath));
	int32 Index = INDEX_NONE;
	FAnimNotifyEvent* Event = ResolveNotifyEvent(Asset, Params, Index);
	if (!Asset || !Event)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve animation asset or notify"));
	}

	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "MoveAnimationNotify", "Move Animation Notify"));
	Asset->Modify();
	double Time = 0.0;
	if (Params->TryGetNumberField(TEXT("time"), Time))
	{
		if (!IsValidNotifyTime(Asset, Time))
		{
			return FUnreelMCPResult::Error(TEXT("Notify time is outside asset range"));
		}
		Event->SetTime(static_cast<float>(Time));
		Event->TriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(static_cast<float>(Time)));
	}
	double Duration = 0.0;
	if (Params->TryGetNumberField(TEXT("duration"), Duration))
	{
		if (!Event->NotifyStateClass || Duration < 0.0 || Event->GetTime() + Duration > Asset->GetPlayLength())
		{
			return FUnreelMCPResult::Error(TEXT("Duration is valid only for notify states and must fit asset range"));
		}
		Event->SetDuration(static_cast<float>(Duration));
		Event->EndLink.Link(Asset, Event->EndLink.GetTime());
		Event->EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Asset->CalculateOffsetForNotify(Event->EndLink.GetTime()));
	}
	FString TrackName;
	if (Params->TryGetStringField(TEXT("track"), TrackName) && !TrackName.IsEmpty())
	{
		const int32 TrackIndex = EnsureNotifyTrack(Asset, FName(*TrackName));
		if (TrackIndex == INDEX_NONE)
		{
			return FUnreelMCPResult::Error(TEXT("Failed to resolve or create notify track"));
		}
		Event->TrackIndex = TrackIndex;
	}

	const FGuid Guid = Event->Guid;
	const FName MovedNotifyName = Event->NotifyName;
	UClass* MovedNotifyClass = Event->Notify ? Event->Notify->GetClass() : nullptr;
	UClass* MovedNotifyStateClass = Event->NotifyStateClass ? Event->NotifyStateClass->GetClass() : nullptr;
	const double MovedTime = Event->GetTime();
	Asset->SortNotifies();
	Asset->RefreshCacheData();
	Asset->MarkPackageDirty();
	int32 FinalIndex = FindNotifyIndexByGuid(Asset, Guid);
	if (!Asset->Notifies.IsValidIndex(FinalIndex))
	{
		FinalIndex = FindNotifyIndexBySignature(Asset, MovedNotifyName, MovedNotifyClass, MovedNotifyStateClass, MovedTime);
	}
	if (!Asset->Notifies.IsValidIndex(FinalIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Notify was moved but could not be resolved after sorting"));
	}
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("index"), FinalIndex);
	Data->SetObjectField(TEXT("notify"), NotifySummary(Asset->Notifies[FinalIndex], FinalIndex));
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation notify moved"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveAnimationNotify(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(LoadAssetObject(AssetPath));
	if (!Asset)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load UAnimSequenceBase asset"));
	}

	int32 Removed = 0;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveAnimationNotify", "Remove Animation Notify"));
	Asset->Modify();
	int32 Index = INDEX_NONE;
	if (ResolveNotifyEvent(Asset, Params, Index) && Asset->Notifies.IsValidIndex(Index))
	{
		Asset->Notifies.RemoveAt(Index);
		Removed = 1;
	}
	else
	{
		FString NotifyName;
		FString ClassPath;
		if (Params->TryGetStringField(TEXT("notify_name"), NotifyName) && !NotifyName.IsEmpty())
		{
			Removed = Asset->Notifies.RemoveAll([&](const FAnimNotifyEvent& Event)
			{
				return Event.NotifyName == FName(*NotifyName);
			});
		}
		else if (Params->TryGetStringField(TEXT("class_path"), ClassPath) && !ClassPath.IsEmpty())
		{
			UClass* NotifyClass = ResolveAnimNotifyClass(ClassPath, false);
			UClass* NotifyStateClass = ResolveAnimNotifyClass(ClassPath, true);
			Removed = Asset->Notifies.RemoveAll([&](const FAnimNotifyEvent& Event)
			{
				return (NotifyClass && Event.Notify && Event.Notify->IsA(NotifyClass)) || (NotifyStateClass && Event.NotifyStateClass && Event.NotifyStateClass->IsA(NotifyStateClass));
			});
		}
		else
		{
			return FUnreelMCPResult::Error(TEXT("Provide index, guid, notify_name, or class_path"));
		}
	}

	Asset->RefreshCacheData();
	Asset->MarkPackageDirty();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("removed"), Removed);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation notify removed"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetAnimationNotifyProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path, property, or value"));
	}
	UAnimSequenceBase* Asset = Cast<UAnimSequenceBase>(LoadAssetObject(AssetPath));
	int32 Index = INDEX_NONE;
	FAnimNotifyEvent* Event = ResolveNotifyEvent(Asset, Params, Index);
	if (!Asset || !Event)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve animation asset or notify"));
	}

	FString Target = TEXT("object");
	Params->TryGetStringField(TEXT("target"), Target);
	FString Error;
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetAnimationNotifyProperty", "Set Animation Notify Property"));
	Asset->Modify();
	FString ChangeTarget;
	FString BeforeValue;
	if (Target.Equals(TEXT("event"), ESearchCase::IgnoreCase))
	{
		ChangeTarget = FString::Printf(TEXT("Notify event %d"), Index);
		BeforeValue = UnreelMCPServiceUtils::ExportStructPropertyText(FAnimNotifyEvent::StaticStruct(), Event, Asset, PropertyName);
		if (!ApplyJsonStructProperty(FAnimNotifyEvent::StaticStruct(), Event, PropertyName, Value, Error))
		{
			return FUnreelMCPResult::Error(Error);
		}
	}
	else
	{
		UObject* NotifyObject = Event->Notify ? static_cast<UObject*>(Event->Notify.Get()) : static_cast<UObject*>(Event->NotifyStateClass.Get());
		if (!NotifyObject)
		{
			return FUnreelMCPResult::Error(TEXT("Notify has no object; use target='event' for name-only notifies"));
		}
		ChangeTarget = NotifyObject->GetPathName();
		BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(NotifyObject, PropertyName);
		if (!ApplyJsonProperty(NotifyObject, PropertyName, Value, Error))
		{
			return FUnreelMCPResult::Error(Error);
		}
	}
	Asset->RefreshCacheData();
	Asset->MarkPackageDirty();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetObjectField(TEXT("notify"), NotifySummary(*Event, Index));
	const FString AfterValue = Target.Equals(TEXT("event"), ESearchCase::IgnoreCase)
		? UnreelMCPServiceUtils::ExportStructPropertyText(FAnimNotifyEvent::StaticStruct(), Event, Asset, PropertyName)
		: UnreelMCPServiceUtils::ExportObjectPropertyText(Event->Notify ? static_cast<UObject*>(Event->Notify.Get()) : static_cast<UObject*>(Event->NotifyStateClass.Get()), PropertyName);
	Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(ChangeTarget, PropertyName, BeforeValue, AfterValue));
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Animation notify property set"), Data);
	Result.ChangedAssets.Add(Asset->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::InspectMontage(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	if (!Montage)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load AnimMontage"));
	}
	return FUnreelMCPResult::Success(TEXT("Montage inspected"), MontageSummary(Montage));
}

FUnreelMCPResult FUnreelMCPParityService::AddMontageSlot(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString SlotName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("slot_name"), SlotName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or slot_name"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	if (!Montage)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load AnimMontage"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMontageSlot", "Add Montage Slot"));
	Montage->Modify();
	if (FindMontageSlotIndex(Montage, FName(*SlotName)) == INDEX_NONE)
	{
		Montage->AddSlot(FName(*SlotName));
	}
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage slot added"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveMontageSlot(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	int32 SlotIndex = INDEX_NONE;
	if (!ResolveMontageSlot(Montage, Params, SlotIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage slot"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveMontageSlot", "Remove Montage Slot"));
	Montage->Modify();
	Montage->SlotAnimTracks.RemoveAt(SlotIndex);
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage slot removed"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddMontageSection(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString SectionName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("section_name"), SectionName))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or section_name"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	if (!Montage)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to load AnimMontage"));
	}
	double Time = 0.0;
	Params->TryGetNumberField(TEXT("time"), Time);
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMontageSection", "Add Montage Section"));
	Montage->Modify();
	const int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), static_cast<float>(Time));
	if (SectionIndex == INDEX_NONE)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to add montage section; the name may already exist"));
	}
	FString NextSection;
	if (Params->TryGetStringField(TEXT("next_section"), NextSection))
	{
		Montage->CompositeSections[SectionIndex].NextSectionName = NextSection.IsEmpty() ? NAME_None : FName(*NextSection);
	}
	Montage->CompositeSections.Sort([](const FCompositeSection& A, const FCompositeSection& B)
	{
		return A.GetTime() < B.GetTime();
	});
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage section added"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetMontageSection(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	const int32 SectionIndex = ResolveMontageSectionIndex(Montage, Params);
	if (!Montage || !Montage->CompositeSections.IsValidIndex(SectionIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage section"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetMontageSection", "Set Montage Section"));
	Montage->Modify();
	FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
	FString NewSectionName;
	if (Params->TryGetStringField(TEXT("new_section_name"), NewSectionName) && !NewSectionName.IsEmpty())
	{
		const int32 ExistingIndex = Montage->GetSectionIndex(FName(*NewSectionName));
		if (ExistingIndex != INDEX_NONE && ExistingIndex != SectionIndex)
		{
			return FUnreelMCPResult::Error(TEXT("A montage section with new_section_name already exists"));
		}
		const FName OldName = Section.SectionName;
		Section.SectionName = FName(*NewSectionName);
		for (FCompositeSection& Candidate : Montage->CompositeSections)
		{
			if (Candidate.NextSectionName == OldName)
			{
				Candidate.NextSectionName = Section.SectionName;
			}
		}
	}
	double Time = 0.0;
	if (Params->TryGetNumberField(TEXT("time"), Time))
	{
		if (Time < 0.0)
		{
			return FUnreelMCPResult::Error(TEXT("Section time must be >= 0"));
		}
		Section.Link(Montage, static_cast<float>(Time));
		Section.SetTime(static_cast<float>(Time));
	}
	FString NextSection;
	if (Params->TryGetStringField(TEXT("next_section"), NextSection))
	{
		if (!NextSection.IsEmpty() && Montage->GetSectionIndex(FName(*NextSection)) == INDEX_NONE)
		{
			return FUnreelMCPResult::Error(TEXT("next_section does not exist"));
		}
		Section.NextSectionName = NextSection.IsEmpty() ? NAME_None : FName(*NextSection);
	}
	Montage->CompositeSections.Sort([](const FCompositeSection& A, const FCompositeSection& B)
	{
		return A.GetTime() < B.GetTime();
	});
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage section set"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveMontageSection(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	const int32 SectionIndex = ResolveMontageSectionIndex(Montage, Params);
	if (!Montage || !Montage->CompositeSections.IsValidIndex(SectionIndex))
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage section"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveMontageSection", "Remove Montage Section"));
	Montage->Modify();
	const FName RemovedName = Montage->CompositeSections[SectionIndex].SectionName;
	Montage->DeleteAnimCompositeSection(SectionIndex);
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.NextSectionName == RemovedName)
		{
			Section.NextSectionName = NAME_None;
		}
	}
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage section removed"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::AddMontageSegment(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString AnimationAssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || !Params->TryGetStringField(TEXT("animation_asset"), AnimationAssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path or animation_asset"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	int32 SlotIndex = INDEX_NONE;
	FSlotAnimationTrack* Slot = ResolveMontageSlot(Montage, Params, SlotIndex);
	UAnimSequenceBase* AnimationAsset = Cast<UAnimSequenceBase>(LoadAssetObject(AnimationAssetPath));
	if (!Montage || !Slot || !AnimationAsset || AnimationAsset->IsA<UAnimMontage>() || !AnimationAsset->CanBeUsedInComposition())
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage slot or composable animation asset"));
	}
	double StartPos = 0.0;
	double AnimStartTime = 0.0;
	double AnimEndTime = AnimationAsset->GetPlayLength();
	double PlayRate = 1.0;
	double LoopingCountNumber = 1;
	Params->TryGetNumberField(TEXT("start_pos"), StartPos);
	Params->TryGetNumberField(TEXT("anim_start_time"), AnimStartTime);
	Params->TryGetNumberField(TEXT("anim_end_time"), AnimEndTime);
	Params->TryGetNumberField(TEXT("play_rate"), PlayRate);
	Params->TryGetNumberField(TEXT("looping_count"), LoopingCountNumber);
	if (!IsValidMontageRange(StartPos, AnimEndTime - AnimStartTime) || AnimStartTime < 0.0 || AnimEndTime > AnimationAsset->GetPlayLength() || AnimEndTime <= AnimStartTime || FMath::IsNearlyZero(PlayRate) || LoopingCountNumber < 1)
	{
		return FUnreelMCPResult::Error(TEXT("Invalid montage segment timing, play_rate, or looping_count"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "AddMontageSegment", "Add Montage Segment"));
	Montage->Modify();
	FAnimSegment& Segment = Slot->AnimTrack.AnimSegments.AddDefaulted_GetRef();
	Segment.SetAnimReference(AnimationAsset, true);
	Segment.StartPos = static_cast<float>(StartPos);
	Segment.AnimStartTime = static_cast<float>(AnimStartTime);
	Segment.AnimEndTime = static_cast<float>(AnimEndTime);
	Segment.AnimPlayRate = static_cast<float>(PlayRate);
	Segment.LoopingCount = static_cast<int32>(LoopingCountNumber);
#if WITH_EDITOR
	Segment.UpdateCachedPlayLength();
	Slot->AnimTrack.SortAnimSegments();
	Montage->UpdateLinkableElements();
#endif
	Montage->CalculateSequenceLength();
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage segment added"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::SetMontageSegment(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	int32 SlotIndex = INDEX_NONE;
	int32 SegmentIndex = INDEX_NONE;
	FAnimSegment* Segment = ResolveMontageSegment(Montage, Params, SlotIndex, SegmentIndex);
	if (!Montage || !Segment)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage segment"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "SetMontageSegment", "Set Montage Segment"));
	Montage->Modify();
	FString AnimationAssetPath;
	if (Params->TryGetStringField(TEXT("animation_asset"), AnimationAssetPath) && !AnimationAssetPath.IsEmpty())
	{
		UAnimSequenceBase* AnimationAsset = Cast<UAnimSequenceBase>(LoadAssetObject(AnimationAssetPath));
		if (!AnimationAsset || AnimationAsset->IsA<UAnimMontage>() || !AnimationAsset->CanBeUsedInComposition())
		{
			return FUnreelMCPResult::Error(TEXT("Failed to resolve composable replacement animation asset"));
		}
		Segment->SetAnimReference(AnimationAsset, true);
		Segment->AnimEndTime = AnimationAsset->GetPlayLength();
	}
	double Number = 0.0;
	if (Params->TryGetNumberField(TEXT("start_pos"), Number))
	{
		if (Number < 0.0)
		{
			return FUnreelMCPResult::Error(TEXT("start_pos must be >= 0"));
		}
		Segment->StartPos = static_cast<float>(Number);
	}
	if (Params->TryGetNumberField(TEXT("anim_start_time"), Number))
	{
		Segment->AnimStartTime = static_cast<float>(Number);
	}
	if (Params->TryGetNumberField(TEXT("anim_end_time"), Number))
	{
		Segment->AnimEndTime = static_cast<float>(Number);
	}
	if (Params->TryGetNumberField(TEXT("play_rate"), Number))
	{
		if (FMath::IsNearlyZero(Number))
		{
			return FUnreelMCPResult::Error(TEXT("play_rate must be non-zero"));
		}
		Segment->AnimPlayRate = static_cast<float>(Number);
	}
	if (Params->TryGetNumberField(TEXT("looping_count"), Number))
	{
		if (Number < 1)
		{
			return FUnreelMCPResult::Error(TEXT("looping_count must be >= 1"));
		}
		Segment->LoopingCount = static_cast<int32>(Number);
	}
	const UAnimSequenceBase* AnimationAsset = Segment->GetAnimReference();
	if (!AnimationAsset || Segment->AnimStartTime < 0.0f || Segment->AnimEndTime <= Segment->AnimStartTime || Segment->AnimEndTime > AnimationAsset->GetPlayLength())
	{
		return FUnreelMCPResult::Error(TEXT("Invalid montage segment time range"));
	}
#if WITH_EDITOR
	Segment->UpdateCachedPlayLength();
	Montage->SlotAnimTracks[SlotIndex].AnimTrack.SortAnimSegments();
	Montage->UpdateLinkableElements(SlotIndex, SegmentIndex);
#endif
	Montage->CalculateSequenceLength();
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage segment set"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::RemoveMontageSegment(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
	{
		return FUnreelMCPResult::Error(TEXT("Missing asset_path"));
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(LoadAssetObject(AssetPath));
	int32 SlotIndex = INDEX_NONE;
	int32 SegmentIndex = INDEX_NONE;
	FAnimSegment* Segment = ResolveMontageSegment(Montage, Params, SlotIndex, SegmentIndex);
	if (!Montage || !Segment)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve montage segment"));
	}
	FUnreelMCPScopedTransaction Transaction(NSLOCTEXT("UnreelMCP", "RemoveMontageSegment", "Remove Montage Segment"));
	Montage->Modify();
	Montage->SlotAnimTracks[SlotIndex].AnimTrack.AnimSegments.RemoveAt(SegmentIndex);
#if WITH_EDITOR
	Montage->UpdateLinkableElements();
#endif
	Montage->CalculateSequenceLength();
	Montage->RefreshCacheData();
	Montage->MarkPackageDirty();
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Montage segment removed"), MontageSummary(Montage));
	Result.ChangedAssets.Add(Montage->GetPathName());
	return Result;
}

FUnreelMCPResult FUnreelMCPParityService::Unsupported(const FString& OperationId, const FString& Reason) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("operation_id"), OperationId);
	Data->SetStringField(TEXT("support"), TEXT("unsupported"));
	Data->SetStringField(TEXT("reason"), Reason);
	FUnreelMCPResult Result = FUnreelMCPResult::Error(FString::Printf(TEXT("%s is unsupported"), *OperationId));
	Result.Data = Data;
	return Result;
}

bool FUnreelMCPParityService::ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	if (!Target)
	{
		OutError = TEXT("Target object is null");
		return false;
	}
	FProperty* Property = Target->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		OutError = FString::Printf(TEXT("Property not found on %s: %s"), *Target->GetClass()->GetName(), *PropertyName);
		return false;
	}
	const FString ImportText = UnreelMCPServiceUtils::CoerceImportText(Property, Value, JsonValueToImportText(Value));
	Target->Modify();
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Target);
	if (Value->Type == EJson::String)
	{
		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			StringProperty->SetPropertyValue(ValuePtr, Value->AsString());
			return true;
		}
		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*Value->AsString()));
			return true;
		}
		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(ValuePtr, FText::FromString(Value->AsString()));
			return true;
		}
	}
	const TCHAR* Result = Property->ImportText_Direct(*ImportText, ValuePtr, Target, PPF_None);
	if (!Result)
	{
		if (Value->Type == EJson::String)
		{
			const FString QuotedImportText = FString::Printf(TEXT("\"%s\""), *Value->AsString().ReplaceCharWithEscapedChar());
			Result = Property->ImportText_Direct(*QuotedImportText, ValuePtr, Target, PPF_None);
			if (Result)
			{
				return true;
			}
		}
		OutError = FString::Printf(TEXT("Failed to import value '%s' into property %s"), *ImportText, *PropertyName);
		return false;
	}
	return true;
}

bool FUnreelMCPParityService::ApplyJsonStructProperty(UScriptStruct* Struct, void* StructData, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const
{
	if (!Struct || !StructData)
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
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(StructData);
	if (Value->Type == EJson::String)
	{
		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			StringProperty->SetPropertyValue(ValuePtr, Value->AsString());
			return true;
		}
		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
		{
			NameProperty->SetPropertyValue(ValuePtr, FName(*Value->AsString()));
			return true;
		}
		if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(ValuePtr, FText::FromString(Value->AsString()));
			return true;
		}
	}
	const FString ImportText = UnreelMCPServiceUtils::CoerceImportText(Property, Value, JsonValueToImportText(Value));
	const TCHAR* Result = Property->ImportText_Direct(*ImportText, ValuePtr, nullptr, PPF_None);
	if (!Result && Value->Type == EJson::String)
	{
		const FString QuotedImportText = FString::Printf(TEXT("\"%s\""), *Value->AsString().ReplaceCharWithEscapedChar());
		Result = Property->ImportText_Direct(*QuotedImportText, ValuePtr, nullptr, PPF_None);
	}
	if (!Result)
	{
		OutError = FString::Printf(TEXT("Failed to import value '%s' into struct property %s"), *ImportText, *PropertyName);
		return false;
	}
	return true;
}

FString FUnreelMCPParityService::JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const
{
	if (!Value.IsValid())
	{
		return TEXT("");
	}
	switch (Value->Type)
	{
	case EJson::String:
		return Value->AsString();
	case EJson::Number:
		return FString::SanitizeFloat(Value->AsNumber());
	case EJson::Boolean:
		return Value->AsBool() ? TEXT("true") : TEXT("false");
	default:
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Value, TEXT(""), Writer);
		return Output;
	}
}

TSharedPtr<FJsonObject> FUnreelMCPParityService::NotifySummary(const FAnimNotifyEvent& Event, int32 Index) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("index"), Index);
	Json->SetStringField(TEXT("guid"), Event.Guid.ToString(EGuidFormats::DigitsWithHyphens));
	Json->SetStringField(TEXT("notify_name"), Event.NotifyName.ToString());
	Json->SetNumberField(TEXT("time"), Event.GetTime());
	Json->SetNumberField(TEXT("trigger_time"), Event.GetTriggerTime());
	Json->SetNumberField(TEXT("duration"), Event.GetDuration());
	Json->SetNumberField(TEXT("track_index"), Event.TrackIndex);
	Json->SetBoolField(TEXT("is_state"), Event.NotifyStateClass != nullptr);
	Json->SetBoolField(TEXT("is_class_based"), Event.Notify != nullptr || Event.NotifyStateClass != nullptr);
	Json->SetNumberField(TEXT("trigger_chance"), Event.NotifyTriggerChance);
	Json->SetNumberField(TEXT("trigger_weight_threshold"), Event.TriggerWeightThreshold);
	if (Event.Notify)
	{
		Json->SetStringField(TEXT("class"), Event.Notify->GetClass()->GetPathName());
		Json->SetStringField(TEXT("object"), Event.Notify->GetPathName());
	}
	else if (Event.NotifyStateClass)
	{
		Json->SetStringField(TEXT("class"), Event.NotifyStateClass->GetClass()->GetPathName());
		Json->SetStringField(TEXT("object"), Event.NotifyStateClass->GetPathName());
	}
	else
	{
		Json->SetStringField(TEXT("class"), TEXT(""));
		Json->SetStringField(TEXT("object"), TEXT(""));
	}
	return Json;
}

TSharedPtr<FJsonObject> FUnreelMCPParityService::MontageSummary(const UAnimMontage* Montage) const
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (!Montage)
	{
		return Json;
	}
	Json->SetStringField(TEXT("asset"), Montage->GetPathName());
	Json->SetNumberField(TEXT("length"), Montage->GetPlayLength());
	Json->SetNumberField(TEXT("notify_count"), Montage->Notifies.Num());

	TArray<TSharedPtr<FJsonValue>> SlotValues;
	for (int32 SlotIndex = 0; SlotIndex < Montage->SlotAnimTracks.Num(); ++SlotIndex)
	{
		const FSlotAnimationTrack& Slot = Montage->SlotAnimTracks[SlotIndex];
		TSharedPtr<FJsonObject> SlotJson = MakeShared<FJsonObject>();
		SlotJson->SetNumberField(TEXT("index"), SlotIndex);
		SlotJson->SetStringField(TEXT("name"), Slot.SlotName.ToString());
		SlotJson->SetNumberField(TEXT("segment_count"), Slot.AnimTrack.AnimSegments.Num());
		SlotJson->SetNumberField(TEXT("track_length"), Slot.AnimTrack.GetLength());

		TArray<TSharedPtr<FJsonValue>> SegmentValues;
		for (int32 SegmentIndex = 0; SegmentIndex < Slot.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
		{
			const FAnimSegment& Segment = Slot.AnimTrack.AnimSegments[SegmentIndex];
			TSharedPtr<FJsonObject> SegmentJson = MakeShared<FJsonObject>();
			SegmentJson->SetNumberField(TEXT("index"), SegmentIndex);
			SegmentJson->SetNumberField(TEXT("start_pos"), Segment.StartPos);
			SegmentJson->SetNumberField(TEXT("end_pos"), Segment.GetEndPos());
			SegmentJson->SetNumberField(TEXT("anim_start_time"), Segment.AnimStartTime);
			SegmentJson->SetNumberField(TEXT("anim_end_time"), Segment.AnimEndTime);
			SegmentJson->SetNumberField(TEXT("play_rate"), Segment.AnimPlayRate);
			SegmentJson->SetNumberField(TEXT("looping_count"), Segment.LoopingCount);
			SegmentJson->SetNumberField(TEXT("length"), Segment.GetLength());
			if (const UAnimSequenceBase* AnimationAsset = Segment.GetAnimReference())
			{
				SegmentJson->SetStringField(TEXT("animation_asset"), AnimationAsset->GetPathName());
				SegmentJson->SetNumberField(TEXT("animation_length"), AnimationAsset->GetPlayLength());
			}
			else
			{
				SegmentJson->SetStringField(TEXT("animation_asset"), TEXT(""));
				SegmentJson->SetNumberField(TEXT("animation_length"), 0.0);
			}
			SegmentValues.Add(MakeShared<FJsonValueObject>(SegmentJson));
		}
		SlotJson->SetArrayField(TEXT("segments"), SegmentValues);
		SlotValues.Add(MakeShared<FJsonValueObject>(SlotJson));
	}
	Json->SetArrayField(TEXT("slots"), SlotValues);
	Json->SetNumberField(TEXT("slot_count"), SlotValues.Num());

	TArray<TSharedPtr<FJsonValue>> SectionValues;
	for (int32 SectionIndex = 0; SectionIndex < Montage->CompositeSections.Num(); ++SectionIndex)
	{
		const FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
		float StartTime = 0.0f;
		float EndTime = 0.0f;
		Montage->GetSectionStartAndEndTime(SectionIndex, StartTime, EndTime);
		TSharedPtr<FJsonObject> SectionJson = MakeShared<FJsonObject>();
		SectionJson->SetNumberField(TEXT("index"), SectionIndex);
		SectionJson->SetStringField(TEXT("name"), Section.SectionName.ToString());
		SectionJson->SetNumberField(TEXT("time"), Section.GetTime());
		SectionJson->SetNumberField(TEXT("start_time"), StartTime);
		SectionJson->SetNumberField(TEXT("end_time"), EndTime);
		SectionJson->SetNumberField(TEXT("length"), Montage->GetSectionLength(SectionIndex));
		SectionJson->SetStringField(TEXT("next_section"), Section.NextSectionName.ToString());
		SectionValues.Add(MakeShared<FJsonValueObject>(SectionJson));
	}
	Json->SetArrayField(TEXT("sections"), SectionValues);
	Json->SetNumberField(TEXT("section_count"), SectionValues.Num());
	return Json;
}
