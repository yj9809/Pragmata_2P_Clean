#include "MCP/Services/UnreelMCPBehaviorTreeService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "AIGraphNode.h"
#include "AIGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Blackboard/BlackboardKeyEnums.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_NativeEnum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Rotator.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTreeFactory.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeGraphNode_SubtreeTask.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BlackboardDataFactory.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Factories/Factory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MCP/Services/UnreelMCPTransactionService.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/PropertyPortFlags.h"

namespace
{
	FUnreelMCPParameter BTParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FUnreelMCPOperation BTOp(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FString>& Keywords, const TArray<FUnreelMCPParameter>& Params, bool bReadOnly, EUnreelMCPRisk Risk, const FString& Example)
	{
		FUnreelMCPOperation Op;
		Op.Id = Id;
		Op.Domain = Domain;
		Op.Operation = Operation;
		Op.Description = Description;
		Op.Keywords = Keywords;
		Op.Parameters = Params;
		Op.bReadOnly = bReadOnly;
		Op.bRequiresTransaction = !bReadOnly;
		Op.Risk = Risk;
		Op.ExampleJson = Example;
		Op.SupportedEngines = { TEXT("5.8") };
		return Op;
	}

	FString GuidString(const FGuid& Guid)
	{
		return Guid.ToString(EGuidFormats::DigitsWithHyphens);
	}

	TSharedPtr<FJsonObject> ObjectRef(UObject* Object)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		if (Object)
		{
			Data->SetStringField(TEXT("name"), Object->GetName());
			Data->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
			Data->SetStringField(TEXT("path"), Object->GetPathName());
		}
		return Data;
	}

	UClass* ResolveClass(const FString& ClassPath, UClass* RequiredBase)
	{
		if (ClassPath.IsEmpty())
		{
			return nullptr;
		}

		UClass* Class = StaticLoadClass(RequiredBase, nullptr, *ClassPath);
		if (!Class)
		{
			UObject* Object = StaticLoadObject(UObject::StaticClass(), nullptr, *ClassPath);
			Class = Cast<UClass>(Object);
			if (!Class)
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(Object))
				{
					Class = Blueprint->GeneratedClass;
				}
			}
		}

		return Class && Class->IsChildOf(RequiredBase) ? Class : nullptr;
	}

	FString ScriptClassLoadHint(const FString& ClassPath)
	{
		if (!ClassPath.StartsWith(TEXT("/Script/")))
		{
			return FString();
		}
		return TEXT(" Project class paths under /Script may require rebuilding the project module, clearing stale editor DLLs, and restarting Unreal Editor before reflection can load the class.");
	}

	UClass* ResolveBlackboardKeyType(const FString& Type, const FString& ClassPath)
	{
		if (!ClassPath.IsEmpty())
		{
			return ResolveClass(ClassPath, UBlackboardKeyType::StaticClass());
		}

		const FString Lower = Type.ToLower();
		if (Lower == TEXT("bool") || Lower == TEXT("boolean")) return UBlackboardKeyType_Bool::StaticClass();
		if (Lower == TEXT("int") || Lower == TEXT("integer")) return UBlackboardKeyType_Int::StaticClass();
		if (Lower == TEXT("float") || Lower == TEXT("number")) return UBlackboardKeyType_Float::StaticClass();
		if (Lower == TEXT("string")) return UBlackboardKeyType_String::StaticClass();
		if (Lower == TEXT("name")) return UBlackboardKeyType_Name::StaticClass();
		if (Lower == TEXT("vector")) return UBlackboardKeyType_Vector::StaticClass();
		if (Lower == TEXT("rotator")) return UBlackboardKeyType_Rotator::StaticClass();
		if (Lower == TEXT("object")) return UBlackboardKeyType_Object::StaticClass();
		if (Lower == TEXT("class")) return UBlackboardKeyType_Class::StaticClass();
		if (Lower == TEXT("enum")) return UBlackboardKeyType_Enum::StaticClass();
		if (Lower == TEXT("native_enum")) return UBlackboardKeyType_NativeEnum::StaticClass();
		return nullptr;
	}

	const FBlackboardEntry* FindBlackboardEntry(const UBehaviorTree* BehaviorTree, const FString& KeyName)
	{
		const UBlackboardData* Blackboard = BehaviorTree ? BehaviorTree->BlackboardAsset.Get() : nullptr;
		if (!Blackboard)
		{
			return nullptr;
		}

		const FName EntryName(*KeyName);
		return Blackboard->Keys.FindByPredicate([EntryName](const FBlackboardEntry& Entry)
		{
			return Entry.EntryName == EntryName;
		});
	}

	UEnum* ResolveBlackboardKeyEnum(const UBlackboardKeyType* KeyType)
	{
		if (const UBlackboardKeyType_Enum* EnumKey = Cast<UBlackboardKeyType_Enum>(KeyType))
		{
			if (EnumKey->EnumType)
			{
				return EnumKey->EnumType.Get();
			}
			if (!EnumKey->EnumName.IsEmpty())
			{
				return UClass::TryFindTypeSlow<UEnum>(EnumKey->EnumName, EFindFirstObjectOptions::ExactClass);
			}
		}
		if (const UBlackboardKeyType_NativeEnum* NativeEnumKey = Cast<UBlackboardKeyType_NativeEnum>(KeyType))
		{
			if (NativeEnumKey->EnumType)
			{
				return NativeEnumKey->EnumType.Get();
			}
			if (!NativeEnumKey->EnumName.IsEmpty())
			{
				return UClass::TryFindTypeSlow<UEnum>(NativeEnumKey->EnumName, EFindFirstObjectOptions::ExactClass);
			}
		}
		return nullptr;
	}

	bool ResolveBlackboardEnumValue(const UBehaviorTree* BehaviorTree, const FString& KeyName, const TSharedPtr<FJsonValue>& Value, int32& OutValue, FString& OutName, FString& OutError)
	{
		OutValue = 0;
		OutName.Reset();
		OutError.Reset();

		const FBlackboardEntry* Entry = FindBlackboardEntry(BehaviorTree, KeyName);
		UEnum* EnumType = Entry ? ResolveBlackboardKeyEnum(Entry->KeyType) : nullptr;
		if (!EnumType)
		{
			return false;
		}
		if (!Value.IsValid() || Value->IsNull())
		{
			OutError = FString::Printf(TEXT("Blackboard enum key '%s' requires a comparison value"), *KeyName);
			return false;
		}

		double NumberValue = 0.0;
		if (Value->TryGetNumber(NumberValue))
		{
			const int32 EnumValue = static_cast<int32>(NumberValue);
			const int32 EnumIndex = EnumType->GetIndexByValue(EnumValue);
			if (EnumIndex == INDEX_NONE)
			{
				OutError = FString::Printf(TEXT("Value %d is not valid for enum Blackboard key '%s'"), EnumValue, *KeyName);
				return false;
			}
			OutValue = EnumValue;
			OutName = EnumType->GetNameStringByValue(EnumValue);
			return true;
		}

		FString StringValue;
		if (!Value->TryGetString(StringValue) || StringValue.TrimStartAndEnd().IsEmpty())
		{
			OutError = FString::Printf(TEXT("Blackboard enum key '%s' value must be a number or enum name string"), *KeyName);
			return false;
		}

		StringValue = StringValue.TrimStartAndEnd();
		const int64 EnumValue = EnumType->GetValueByNameString(StringValue);
		if (EnumValue == INDEX_NONE || EnumType->GetIndexByValue(EnumValue) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Value '%s' is not valid for enum Blackboard key '%s'"), *StringValue, *KeyName);
			return false;
		}
		OutValue = static_cast<int32>(EnumValue);
		OutName = EnumType->GetNameStringByValue(EnumValue);
		return true;
	}

	UEdGraphPin* FirstPin(UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UClass* GraphNodeClassFor(const FString& Kind, UClass* NodeClass)
	{
		if (Kind == TEXT("composite"))
		{
			return NodeClass == UBTComposite_SimpleParallel::StaticClass()
				? UBehaviorTreeGraphNode_SimpleParallel::StaticClass()
				: UBehaviorTreeGraphNode_Composite::StaticClass();
		}

		if (Kind == TEXT("task"))
		{
			return NodeClass == UBTTask_RunBehavior::StaticClass()
				? UBehaviorTreeGraphNode_SubtreeTask::StaticClass()
				: UBehaviorTreeGraphNode_Task::StaticClass();
		}

		if (Kind == TEXT("decorator")) return UBehaviorTreeGraphNode_Decorator::StaticClass();
		if (Kind == TEXT("service")) return UBehaviorTreeGraphNode_Service::StaticClass();
		return nullptr;
	}
}

FString FUnreelMCPBehaviorTreeService::GetServiceName() const
{
	return TEXT("behavior_tree");
}

TArray<FUnreelMCPOperation> FUnreelMCPBehaviorTreeService::GetOperations() const
{
	const TArray<FUnreelMCPParameter> AssetPath = { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Asset path."), true) };
	const TArray<FUnreelMCPParameter> BTNodeParams = {
		BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true),
		BTParam(TEXT("class_path"), TEXT("string"), TEXT("UBTCompositeNode or UBTTaskNode class path."), true),
		BTParam(TEXT("parent_node_id"), TEXT("string"), TEXT("Optional parent node GUID; if set, the new node is connected to this parent.")),
		BTParam(TEXT("insert_index"), TEXT("integer"), TEXT("Optional child insertion index under the parent node.")),
		BTParam(TEXT("x"), TEXT("number"), TEXT("Graph X position."), false, TEXT("0")),
		BTParam(TEXT("y"), TEXT("number"), TEXT("Graph Y position."), false, TEXT("0")),
		BTParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the node instance."))
	};
	const TArray<FUnreelMCPParameter> BTSubNodeParams = {
		BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true),
		BTParam(TEXT("parent_node_id"), TEXT("string"), TEXT("Parent composite or task node GUID."), true),
		BTParam(TEXT("class_path"), TEXT("string"), TEXT("UBTDecorator or UBTService class path."), true),
		BTParam(TEXT("index"), TEXT("integer"), TEXT("Optional insertion index.")),
		BTParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties to set on the node instance."))
	};

	return {
		BTOp(TEXT("behavior_tree.create"), TEXT("behavior_tree"), TEXT("create"), TEXT("Create a Behavior Tree asset and initialize its editable graph."), { TEXT("behavior"), TEXT("tree"), TEXT("bt"), TEXT("ai"), TEXT("create") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination asset path."), true), BTParam(TEXT("blackboard"), TEXT("string"), TEXT("Optional Blackboard asset path to assign.")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.create\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"blackboard\":\"/Game/AI/BB_Boss\"}}")),
		BTOp(TEXT("behavior_tree.inspect"), TEXT("behavior_tree"), TEXT("inspect"), TEXT("Inspect Behavior Tree graph nodes, runtime node classes, subnodes, pins, connections, and Blackboard assignment."), { TEXT("behavior"), TEXT("tree"), TEXT("bt"), TEXT("inspect") }, AssetPath, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"behavior_tree.inspect\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\"}}")),
		BTOp(TEXT("behavior_tree.set_blackboard"), TEXT("behavior_tree"), TEXT("set_blackboard"), TEXT("Assign a Blackboard asset to a Behavior Tree."), { TEXT("behavior"), TEXT("tree"), TEXT("blackboard"), TEXT("assign") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("blackboard"), TEXT("string"), TEXT("Blackboard asset path. Empty clears it.")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.set_blackboard\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"blackboard\":\"/Game/AI/BB_Boss\"}}")),
		BTOp(TEXT("behavior_tree.add_composite"), TEXT("behavior_tree"), TEXT("add_composite"), TEXT("Add a composite node such as Selector or Sequence and optionally connect it to a parent."), { TEXT("behavior"), TEXT("tree"), TEXT("composite"), TEXT("selector"), TEXT("sequence") }, BTNodeParams, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_composite\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"class_path\":\"/Script/AIModule.BTComposite_Sequence\",\"parent_node_id\":\"root\"}}")),
		BTOp(TEXT("behavior_tree.add_task"), TEXT("behavior_tree"), TEXT("add_task"), TEXT("Add a task node such as a project UBTTaskNode subclass and optionally connect it to a parent."), { TEXT("behavior"), TEXT("tree"), TEXT("task"), TEXT("bt task"), TEXT("ai") }, BTNodeParams, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_task\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"class_path\":\"/Script/LyraGame.DSBTTask_MoveToIdealRange\",\"parent_node_id\":\"...\"}}")),
		BTOp(TEXT("behavior_tree.add_wait"), TEXT("behavior_tree"), TEXT("add_wait"), TEXT("Add a BTTask_Wait node with plain numeric wait_time and random_deviation values."), { TEXT("behavior"), TEXT("tree"), TEXT("wait"), TEXT("task"), TEXT("delay") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("parent_node_id"), TEXT("string"), TEXT("Optional parent node GUID.")), BTParam(TEXT("wait_time"), TEXT("number"), TEXT("Wait time in seconds."), false, TEXT("1.0")), BTParam(TEXT("random_deviation"), TEXT("number"), TEXT("Optional random deviation in seconds."), false, TEXT("0.0")), BTParam(TEXT("insert_index"), TEXT("integer"), TEXT("Optional child insertion index.")), BTParam(TEXT("x"), TEXT("number"), TEXT("Graph X position."), false, TEXT("0")), BTParam(TEXT("y"), TEXT("number"), TEXT("Graph Y position."), false, TEXT("0")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_wait\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"wait_time\":0.5,\"random_deviation\":0.1}}")),
		BTOp(TEXT("behavior_tree.add_decorator"), TEXT("behavior_tree"), TEXT("add_decorator"), TEXT("Attach a decorator subnode to a Behavior Tree graph node."), { TEXT("behavior"), TEXT("tree"), TEXT("decorator"), TEXT("condition") }, BTSubNodeParams, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_decorator\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"class_path\":\"/Script/LyraGame.DSBTDecorator_TargetInFront\"}}")),
		BTOp(TEXT("behavior_tree.add_blackboard_decorator"), TEXT("behavior_tree"), TEXT("add_blackboard_decorator"), TEXT("Attach a Blackboard decorator with key, operation, value, abort mode, and insertion index fields."), { TEXT("behavior"), TEXT("tree"), TEXT("decorator"), TEXT("blackboard"), TEXT("condition"), TEXT("enum") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("parent_node_id"), TEXT("string"), TEXT("Parent composite or task node GUID."), true), BTParam(TEXT("key"), TEXT("string"), TEXT("Blackboard key name."), true), BTParam(TEXT("operation"), TEXT("string"), TEXT("set, not_set, equal, not_equal, less, less_or_equal, greater, greater_or_equal, contains, not_contains."), false, TEXT("equal")), BTParam(TEXT("value"), TEXT("any"), TEXT("Comparison value for arithmetic or text operations.")), BTParam(TEXT("abort_mode"), TEXT("string"), TEXT("None, Self, LowerPriority, or Both."), false, TEXT("None")), BTParam(TEXT("notify_observer"), TEXT("string"), TEXT("ValueChange or ResultChange."), false, TEXT("ResultChange")), BTParam(TEXT("index"), TEXT("integer"), TEXT("Optional decorator insertion index.")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_blackboard_decorator\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"key\":\"BossAction\",\"operation\":\"equal\",\"value\":1,\"abort_mode\":\"Both\"}}")),
		BTOp(TEXT("behavior_tree.add_service"), TEXT("behavior_tree"), TEXT("add_service"), TEXT("Attach a service subnode to a Behavior Tree graph node."), { TEXT("behavior"), TEXT("tree"), TEXT("service"), TEXT("tick") }, BTSubNodeParams, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.add_service\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"class_path\":\"/Script/LyraGame.DSBTService_BossCombatDirector\"}}")),
		BTOp(TEXT("behavior_tree.connect"), TEXT("behavior_tree"), TEXT("connect"), TEXT("Connect one Behavior Tree graph node to another using schema validation."), { TEXT("behavior"), TEXT("tree"), TEXT("connect"), TEXT("wire") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("from_node_id"), TEXT("string"), TEXT("Parent/source node GUID or 'root'."), true), BTParam(TEXT("to_node_id"), TEXT("string"), TEXT("Child/target node GUID."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.connect\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"from_node_id\":\"root\",\"to_node_id\":\"...\"}}")),
		BTOp(TEXT("behavior_tree.move_child"), TEXT("behavior_tree"), TEXT("move_child"), TEXT("Move a child node to a specific child index under a parent composite/root node."), { TEXT("behavior"), TEXT("tree"), TEXT("move"), TEXT("order"), TEXT("child"), TEXT("reorder") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("parent_node_id"), TEXT("string"), TEXT("Parent/source node GUID or 'root'."), true), BTParam(TEXT("child_node_id"), TEXT("string"), TEXT("Child node GUID."), true), BTParam(TEXT("target_index"), TEXT("integer"), TEXT("Target child index."), true) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.move_child\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"child_node_id\":\"...\",\"target_index\":0}}")),
		BTOp(TEXT("behavior_tree.set_node_property"), TEXT("behavior_tree"), TEXT("set_node_property"), TEXT("Set a reflected property on a graph node or its UBTNode instance."), { TEXT("behavior"), TEXT("tree"), TEXT("property"), TEXT("edit") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("node_id"), TEXT("string"), TEXT("Graph node GUID."), true), BTParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), BTParam(TEXT("value"), TEXT("any"), TEXT("JSON value or import-text string."), true), BTParam(TEXT("target"), TEXT("string"), TEXT("instance or graph_node."), false, TEXT("instance")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"behavior_tree.set_node_property\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\",\"property\":\"NodeName\",\"value\":\"Move To Ideal Range\"}}")),
		BTOp(TEXT("behavior_tree.remove_node"), TEXT("behavior_tree"), TEXT("remove_node"), TEXT("Remove a Behavior Tree graph node or subnode by GUID."), { TEXT("behavior"), TEXT("tree"), TEXT("remove"), TEXT("delete") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("node_id"), TEXT("string"), TEXT("Graph node or subnode GUID."), true), BTParam(TEXT("remove_children"), TEXT("boolean"), TEXT("If true, remove the node and all reachable descendants."), false, TEXT("false")) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"behavior_tree.remove_node\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\",\"remove_children\":true}}")),
		BTOp(TEXT("behavior_tree.remove_subtree"), TEXT("behavior_tree"), TEXT("remove_subtree"), TEXT("Remove a non-root Behavior Tree node and all descendants reachable from its output pin."), { TEXT("behavior"), TEXT("tree"), TEXT("remove"), TEXT("delete"), TEXT("subtree"), TEXT("children") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Behavior Tree asset path."), true), BTParam(TEXT("node_id"), TEXT("string"), TEXT("Subtree root graph node GUID."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"behavior_tree.remove_subtree\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\"}}")),
		BTOp(TEXT("behavior_tree.layout"), TEXT("behavior_tree"), TEXT("layout"), TEXT("Auto-arrange a Behavior Tree graph and update the runtime tree asset."), { TEXT("behavior"), TEXT("tree"), TEXT("layout"), TEXT("arrange") }, AssetPath, false, EUnreelMCPRisk::Low, TEXT("{\"operation_id\":\"behavior_tree.layout\",\"params\":{\"asset_path\":\"/Game/AI/BT_Boss\"}}")),
		BTOp(TEXT("blackboard.create"), TEXT("blackboard"), TEXT("create"), TEXT("Create a Blackboard asset."), { TEXT("blackboard"), TEXT("bb"), TEXT("ai"), TEXT("create") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Destination asset path."), true), BTParam(TEXT("parent"), TEXT("string"), TEXT("Optional parent Blackboard asset path.")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blackboard.create\",\"params\":{\"asset_path\":\"/Game/AI/BB_Boss\"}}")),
		BTOp(TEXT("blackboard.inspect"), TEXT("blackboard"), TEXT("inspect"), TEXT("Inspect Blackboard keys and parent data."), { TEXT("blackboard"), TEXT("bb"), TEXT("inspect") }, AssetPath, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"blackboard.inspect\",\"params\":{\"asset_path\":\"/Game/AI/BB_Boss\"}}")),
		BTOp(TEXT("blackboard.add_key"), TEXT("blackboard"), TEXT("add_key"), TEXT("Add or replace a local Blackboard key with a key type."), { TEXT("blackboard"), TEXT("bb"), TEXT("key"), TEXT("add") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Blackboard asset path."), true), BTParam(TEXT("name"), TEXT("string"), TEXT("Key name."), true), BTParam(TEXT("type"), TEXT("string"), TEXT("Shortcut type: bool, int, float, string, name, vector, rotator, object, class, enum, native_enum."), false, TEXT("object")), BTParam(TEXT("key_type_class_path"), TEXT("string"), TEXT("Optional UBlackboardKeyType class path.")), BTParam(TEXT("description"), TEXT("string"), TEXT("Editor description.")), BTParam(TEXT("category"), TEXT("string"), TEXT("Editor category.")), BTParam(TEXT("instance_synced"), TEXT("boolean"), TEXT("Synchronize this key across instances.")), BTParam(TEXT("properties"), TEXT("object"), TEXT("Optional reflected properties for the key type object.")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blackboard.add_key\",\"params\":{\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\",\"type\":\"object\",\"properties\":{\"BaseClass\":\"/Script/Engine.Actor\"}}}")),
		BTOp(TEXT("blackboard.remove_key"), TEXT("blackboard"), TEXT("remove_key"), TEXT("Remove a local Blackboard key by name."), { TEXT("blackboard"), TEXT("bb"), TEXT("key"), TEXT("remove") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Blackboard asset path."), true), BTParam(TEXT("name"), TEXT("string"), TEXT("Key name."), true) }, false, EUnreelMCPRisk::High, TEXT("{\"operation_id\":\"blackboard.remove_key\",\"params\":{\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\"}}")),
		BTOp(TEXT("blackboard.set_key_property"), TEXT("blackboard"), TEXT("set_key_property"), TEXT("Set a reflected property on a Blackboard entry or its key type object."), { TEXT("blackboard"), TEXT("bb"), TEXT("key"), TEXT("property") }, { BTParam(TEXT("asset_path"), TEXT("string"), TEXT("Blackboard asset path."), true), BTParam(TEXT("name"), TEXT("string"), TEXT("Key name."), true), BTParam(TEXT("property"), TEXT("string"), TEXT("Property name."), true), BTParam(TEXT("value"), TEXT("any"), TEXT("JSON value or import-text string."), true), BTParam(TEXT("target"), TEXT("string"), TEXT("entry or key_type."), false, TEXT("key_type")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"blackboard.set_key_property\",\"params\":{\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\",\"target\":\"key_type\",\"property\":\"BaseClass\",\"value\":\"/Script/Engine.Actor\"}}"))
	};
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("behavior_tree.create")) return CreateBehaviorTree(Params);
	if (OperationId == TEXT("behavior_tree.inspect")) return InspectBehaviorTree(Params);
	if (OperationId == TEXT("behavior_tree.set_blackboard")) return SetBehaviorTreeBlackboard(Params);
	if (OperationId == TEXT("behavior_tree.add_composite")) return AddBehaviorTreeNode(Params, TEXT("composite"));
	if (OperationId == TEXT("behavior_tree.add_task")) return AddBehaviorTreeNode(Params, TEXT("task"));
	if (OperationId == TEXT("behavior_tree.add_wait")) return AddBehaviorTreeWait(Params);
	if (OperationId == TEXT("behavior_tree.add_decorator")) return AddBehaviorTreeSubNode(Params, TEXT("decorator"));
	if (OperationId == TEXT("behavior_tree.add_blackboard_decorator")) return AddBehaviorTreeBlackboardDecorator(Params);
	if (OperationId == TEXT("behavior_tree.add_service")) return AddBehaviorTreeSubNode(Params, TEXT("service"));
	if (OperationId == TEXT("behavior_tree.connect")) return ConnectBehaviorTreeNodes(Params);
	if (OperationId == TEXT("behavior_tree.move_child")) return MoveBehaviorTreeChild(Params);
	if (OperationId == TEXT("behavior_tree.set_node_property")) return SetBehaviorTreeNodeProperty(Params);
	if (OperationId == TEXT("behavior_tree.remove_subtree")) return RemoveBehaviorTreeSubtree(Params);
	if (OperationId == TEXT("behavior_tree.remove_node")) return RemoveBehaviorTreeNode(Params);
	if (OperationId == TEXT("behavior_tree.layout")) return LayoutBehaviorTree(Params);
	if (OperationId == TEXT("blackboard.create")) return CreateBlackboard(Params);
	if (OperationId == TEXT("blackboard.inspect")) return InspectBlackboard(Params);
	if (OperationId == TEXT("blackboard.add_key")) return AddBlackboardKey(Params);
	if (OperationId == TEXT("blackboard.remove_key")) return RemoveBlackboardKey(Params);
	if (OperationId == TEXT("blackboard.set_key_property")) return SetBlackboardKeyProperty(Params);
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown Behavior Tree operation: %s"), *OperationId));
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::CreateBehaviorTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}

	FString Error;
	UObject* Created = CreateAsset(AssetPath, UBehaviorTree::StaticClass(), UBehaviorTreeFactory::StaticClass(), Error);
	UBehaviorTree* BehaviorTree = Cast<UBehaviorTree>(Created);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error.IsEmpty() ? TEXT("Failed to create Behavior Tree") : Error);
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Create Behavior Tree")));
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	FString BlackboardPath;
	if (Params->TryGetStringField(TEXT("blackboard"), BlackboardPath) && !BlackboardPath.IsEmpty())
	{
		UBlackboardData* Blackboard = LoadBlackboard(BlackboardPath, Error);
		if (!Blackboard)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Behavior Tree created but Blackboard assignment failed: %s"), *Error));
		}
		BehaviorTree->Modify();
		BehaviorTree->BlackboardAsset = Blackboard;
	}

	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree created"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::InspectBehaviorTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}
	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}
	return FUnreelMCPResult::Success(TEXT("Behavior Tree inspected"), BehaviorTreeSummary(BehaviorTree, Graph));
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::SetBehaviorTreeBlackboard(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}
	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	FString BlackboardPath;
	Params->TryGetStringField(TEXT("blackboard"), BlackboardPath);
	UBlackboardData* Blackboard = nullptr;
	if (!BlackboardPath.IsEmpty())
	{
		Blackboard = LoadBlackboard(BlackboardPath, Error);
		if (!Blackboard)
		{
			return FUnreelMCPResult::Error(Error);
		}
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Set Behavior Tree Blackboard")));
	BehaviorTree->Modify();
	BehaviorTree->BlackboardAsset = Blackboard;
	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree Blackboard updated"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::AddBehaviorTreeNode(const TSharedRef<FJsonObject>& Params, const FString& Kind) const
{
	FString AssetPath;
	FString ClassPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameter: class_path. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"class_path\":\"/Script/AIModule.BTTask_Wait\"}") : ParamError);
	}

	UClass* RequiredBase = Kind == TEXT("composite") ? UBTCompositeNode::StaticClass() : UBTTaskNode::StaticClass();
	UClass* NodeClass = ResolveClass(ClassPath, RequiredBase);
	if (!NodeClass)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load %s class: %s.%s"), *Kind, *ClassPath, *ScriptClassLoadHint(ClassPath)));
	}
	if (NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Cannot instantiate abstract class: %s"), *ClassPath));
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	UClass* GraphNodeClass = GraphNodeClassFor(Kind, NodeClass);
	if (!GraphNodeClass)
	{
		return FUnreelMCPResult::Error(TEXT("Failed to resolve graph node class"));
	}

	double X = 0;
	double Y = 0;
	Params->TryGetNumberField(TEXT("x"), X);
	Params->TryGetNumberField(TEXT("y"), Y);

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add Behavior Tree Node")));
	Graph->Modify();
	UBehaviorTreeGraphNode* Template = NewObject<UBehaviorTreeGraphNode>(Graph, GraphNodeClass);
	Template->ClassData = FGraphNodeClassData(NodeClass, FString());
	FAISchemaAction_NewNode Action;
	Action.NodeTemplate = Template;
	UBehaviorTreeGraphNode* NewNode = Cast<UBehaviorTreeGraphNode>(Action.PerformAction(Graph, nullptr, FVector2f(static_cast<float>(X), static_cast<float>(Y)), false));
	if (!NewNode)
	{
		return FUnreelMCPResult::Error(TEXT("Behavior Tree graph node creation failed"));
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid() && NewNode->NodeInstance)
	{
		if (!ApplyJsonProperties(NewNode->NodeInstance, *Properties, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Node created but property assignment failed: %s"), *Error));
		}
	}

	FString ParentNodeId;
	if (Params->TryGetStringField(TEXT("parent_node_id"), ParentNodeId) && !ParentNodeId.IsEmpty())
	{
		UBehaviorTreeGraphNode* ParentNode = ParentNodeId.Equals(TEXT("root"), ESearchCase::IgnoreCase) ? FindRootNode(Graph) : FindBehaviorTreeNode(Graph, ParentNodeId);
		if (!ParentNode)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Node created but parent was not found: %s"), *ParentNodeId));
		}
		UEdGraphPin* FromPin = FirstPin(ParentNode, EGPD_Output);
		UEdGraphPin* ToPin = FirstPin(NewNode, EGPD_Input);
		if (!FromPin || !ToPin)
		{
			return FUnreelMCPResult::Error(TEXT("Node created but parent or child pin was not available"));
		}
		const UEdGraphSchema* Schema = Graph->GetSchema();
		const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
		if (!Response.CanSafeConnect())
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Node created but connection was rejected: %s"), *Response.Message.ToString()));
		}
		Schema->TryCreateConnection(FromPin, ToPin);
		int32 InsertIndex = INDEX_NONE;
		if (Params->TryGetNumberField(TEXT("insert_index"), InsertIndex) && InsertIndex >= 0)
		{
			if (!MoveChildLink(ParentNode, NewNode, InsertIndex, Error))
			{
				return FUnreelMCPResult::Error(FString::Printf(TEXT("Node created but insert_index reorder failed: %s"), *Error));
			}
		}
	}

	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree node added"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.Data->SetObjectField(TEXT("node"), BehaviorTreeNodeSummary(NewNode));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::AddBehaviorTreeSubNode(const TSharedRef<FJsonObject>& Params, const FString& Kind) const
{
	FString AssetPath;
	FString ParentNodeId;
	FString ClassPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("parent_node_id"), ParentNodeId) || !Params->TryGetStringField(TEXT("class_path"), ClassPath))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: parent_node_id and class_path. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"class_path\":\"/Script/AIModule.BTDecorator_ForceSuccess\"}") : ParamError);
	}

	UClass* RequiredBase = Kind == TEXT("decorator") ? UBTDecorator::StaticClass() : UBTService::StaticClass();
	UClass* NodeClass = ResolveClass(ClassPath, RequiredBase);
	if (!NodeClass)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to load %s class: %s.%s"), *Kind, *ClassPath, *ScriptClassLoadHint(ClassPath)));
	}
	if (NodeClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Cannot instantiate abstract class: %s"), *ClassPath));
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraphNode* ParentNode = FindBehaviorTreeNode(Graph, ParentNodeId);
	if (!ParentNode)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Parent node not found: %s"), *ParentNodeId));
	}

	UClass* GraphNodeClass = GraphNodeClassFor(Kind, NodeClass);
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add Behavior Tree Subnode")));
	Graph->Modify();
	ParentNode->Modify();
	UBehaviorTreeGraphNode* SubNode = NewObject<UBehaviorTreeGraphNode>(Graph, GraphNodeClass);
	SubNode->ClassData = FGraphNodeClassData(NodeClass, FString());
	ParentNode->AddSubNode(SubNode, Graph);

	int32 Index = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("index"), Index) && Index >= 0)
	{
		ParentNode->RemoveSubNode(SubNode);
		ParentNode->InsertSubNodeAt(SubNode, Index);
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid() && SubNode->NodeInstance)
	{
		if (!ApplyJsonProperties(SubNode->NodeInstance, *Properties, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Subnode created but property assignment failed: %s"), *Error));
		}
	}

	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree subnode added"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.Data->SetObjectField(TEXT("node"), BehaviorTreeNodeSummary(SubNode));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::AddBehaviorTreeWait(const TSharedRef<FJsonObject>& Params) const
{
	double WaitTime = 1.0;
	double RandomDeviation = 0.0;
	Params->TryGetNumberField(TEXT("wait_time"), WaitTime);
	Params->TryGetNumberField(TEXT("random_deviation"), RandomDeviation);

	TSharedRef<FJsonObject> WaitParams = MakeShared<FJsonObject>();
	FString AssetPath;
	FString AssetParamError;
	if (!ReadAssetPathParam(Params, AssetPath, AssetParamError, TEXT("/Game/AI/BT_Boss")))
	{
		return FUnreelMCPResult::Error(AssetParamError);
	}
	WaitParams->SetStringField(TEXT("asset_path"), AssetPath);
	WaitParams->SetStringField(TEXT("class_path"), TEXT("/Script/AIModule.BTTask_Wait"));

	FString ParentNodeId;
	if (Params->TryGetStringField(TEXT("parent_node_id"), ParentNodeId))
	{
		WaitParams->SetStringField(TEXT("parent_node_id"), ParentNodeId);
	}
	double X = 0;
	double Y = 0;
	if (Params->TryGetNumberField(TEXT("x"), X))
	{
		WaitParams->SetNumberField(TEXT("x"), X);
	}
	if (Params->TryGetNumberField(TEXT("y"), Y))
	{
		WaitParams->SetNumberField(TEXT("y"), Y);
	}
	int32 InsertIndex = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("insert_index"), InsertIndex))
	{
		WaitParams->SetNumberField(TEXT("insert_index"), InsertIndex);
	}

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	Properties->SetStringField(TEXT("WaitTime"), FString::Printf(TEXT("(DefaultValue=%f,Key=\"\")"), WaitTime));
	Properties->SetStringField(TEXT("RandomDeviation"), FString::Printf(TEXT("(DefaultValue=%f,Key=\"\")"), RandomDeviation));
	WaitParams->SetObjectField(TEXT("properties"), Properties);
	return AddBehaviorTreeNode(WaitParams, TEXT("task"));
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::AddBehaviorTreeBlackboardDecorator(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParentNodeId;
	FString KeyName;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("parent_node_id"), ParentNodeId) || !Params->TryGetStringField(TEXT("key"), KeyName) || ParentNodeId.IsEmpty() || KeyName.IsEmpty())
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: parent_node_id and key. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"key\":\"BossAction\",\"operation\":\"equal\",\"value\":1}") : ParamError);
	}

	FString Operation = TEXT("equal");
	FString AbortMode = TEXT("None");
	FString NotifyObserver = TEXT("ResultChange");
	Params->TryGetStringField(TEXT("operation"), Operation);
	Params->TryGetStringField(TEXT("abort_mode"), AbortMode);
	Params->TryGetStringField(TEXT("notify_observer"), NotifyObserver);
	Operation = Operation.ToLower();

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}

	TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
	Properties->SetStringField(TEXT("BlackboardKey"), FString::Printf(TEXT("(SelectedKeyName=\"%s\")"), *KeyName));
	Properties->SetStringField(TEXT("FlowAbortMode"), AbortMode);
	Properties->SetStringField(TEXT("NotifyObserver"), NotifyObserver);

	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	if (Operation == TEXT("set") || Operation == TEXT("is_set"))
	{
		Properties->SetStringField(TEXT("BasicOperation"), TEXT("Set"));
		Properties->SetNumberField(TEXT("OperationType"), static_cast<int32>(EBasicKeyOperation::Set));
	}
	else if (Operation == TEXT("not_set") || Operation == TEXT("is_not_set"))
	{
		Properties->SetStringField(TEXT("BasicOperation"), TEXT("NotSet"));
		Properties->SetNumberField(TEXT("OperationType"), static_cast<int32>(EBasicKeyOperation::NotSet));
	}
	else if (Operation == TEXT("contains") || Operation == TEXT("contain") || Operation == TEXT("not_contains") || Operation == TEXT("not_contain"))
	{
		const bool bNot = Operation.StartsWith(TEXT("not_"));
		Properties->SetStringField(TEXT("TextOperation"), bNot ? TEXT("NotContain") : TEXT("Contain"));
		Properties->SetNumberField(TEXT("OperationType"), bNot ? static_cast<int32>(ETextKeyOperation::NotContain) : static_cast<int32>(ETextKeyOperation::Contain));
		Properties->SetStringField(TEXT("StringValue"), Value.IsValid() ? JsonValueToImportText(Value) : FString());
	}
	else
	{
		FString ArithmeticOp = TEXT("Equal");
		int32 OperationValue = static_cast<int32>(EArithmeticKeyOperation::Equal);
		if (Operation == TEXT("not_equal") || Operation == TEXT("not equal"))
		{
			ArithmeticOp = TEXT("NotEqual");
			OperationValue = static_cast<int32>(EArithmeticKeyOperation::NotEqual);
		}
		else if (Operation == TEXT("less"))
		{
			ArithmeticOp = TEXT("Less");
			OperationValue = static_cast<int32>(EArithmeticKeyOperation::Less);
		}
		else if (Operation == TEXT("less_or_equal") || Operation == TEXT("less_equal"))
		{
			ArithmeticOp = TEXT("LessOrEqual");
			OperationValue = static_cast<int32>(EArithmeticKeyOperation::LessOrEqual);
		}
		else if (Operation == TEXT("greater"))
		{
			ArithmeticOp = TEXT("Greater");
			OperationValue = static_cast<int32>(EArithmeticKeyOperation::Greater);
		}
		else if (Operation == TEXT("greater_or_equal") || Operation == TEXT("greater_equal"))
		{
			ArithmeticOp = TEXT("GreaterOrEqual");
			OperationValue = static_cast<int32>(EArithmeticKeyOperation::GreaterOrEqual);
		}
		Properties->SetStringField(TEXT("ArithmeticOperation"), ArithmeticOp);
		Properties->SetNumberField(TEXT("OperationType"), OperationValue);

		int32 EnumValue = 0;
		FString EnumName;
		FString EnumError;
		const bool bResolvedEnum = ResolveBlackboardEnumValue(BehaviorTree, KeyName, Value, EnumValue, EnumName, EnumError);
		if (!EnumError.IsEmpty())
		{
			return FUnreelMCPResult::Error(EnumError);
		}
		if (bResolvedEnum)
		{
			Properties->SetNumberField(TEXT("IntValue"), EnumValue);
			Properties->SetNumberField(TEXT("FloatValue"), static_cast<double>(EnumValue));
			Properties->SetStringField(TEXT("StringValue"), EnumName);
		}
		else
		{
			double NumberValue = 0.0;
			if (Value.IsValid() && Value->TryGetNumber(NumberValue))
			{
				Properties->SetNumberField(TEXT("IntValue"), static_cast<int32>(NumberValue));
				Properties->SetNumberField(TEXT("FloatValue"), NumberValue);
			}
			else if (Value.IsValid())
			{
				Properties->SetStringField(TEXT("StringValue"), JsonValueToImportText(Value));
			}
		}
	}

	TSharedRef<FJsonObject> DecoratorParams = MakeShared<FJsonObject>();
	DecoratorParams->SetStringField(TEXT("asset_path"), AssetPath);
	DecoratorParams->SetStringField(TEXT("parent_node_id"), ParentNodeId);
	DecoratorParams->SetStringField(TEXT("class_path"), TEXT("/Script/AIModule.BTDecorator_Blackboard"));
	DecoratorParams->SetObjectField(TEXT("properties"), Properties);
	int32 Index = INDEX_NONE;
	if (Params->TryGetNumberField(TEXT("index"), Index))
	{
		DecoratorParams->SetNumberField(TEXT("index"), Index);
	}
	FUnreelMCPResult Result = AddBehaviorTreeSubNode(DecoratorParams, TEXT("decorator"));
	if (Result.bSuccess && Result.Data.IsValid())
	{
		Result.Data->SetStringField(TEXT("blackboard_key"), KeyName);
		Result.Data->SetStringField(TEXT("blackboard_operation"), Operation);
	}
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::ConnectBehaviorTreeNodes(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString FromNodeId;
	FString ToNodeId;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("from_node_id"), FromNodeId) || !Params->TryGetStringField(TEXT("to_node_id"), ToNodeId))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: from_node_id and to_node_id. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"from_node_id\":\"root\",\"to_node_id\":\"...\"}") : ParamError);
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	UBehaviorTreeGraphNode* FromNode = FromNodeId.Equals(TEXT("root"), ESearchCase::IgnoreCase) ? FindRootNode(Graph) : FindBehaviorTreeNode(Graph, FromNodeId);
	UBehaviorTreeGraphNode* ToNode = FindBehaviorTreeNode(Graph, ToNodeId);
	if (!FromNode || !ToNode)
	{
		return FUnreelMCPResult::Error(TEXT("Source or target node not found"));
	}

	UEdGraphPin* FromPin = FirstPin(FromNode, EGPD_Output);
	UEdGraphPin* ToPin = FirstPin(ToNode, EGPD_Input);
	if (!FromPin || !ToPin)
	{
		return FUnreelMCPResult::Error(TEXT("Source or target node pin not found"));
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
	if (!Response.CanSafeConnect())
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Connection rejected: %s"), *Response.Message.ToString()));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Connect Behavior Tree Nodes")));
	FromNode->Modify();
	ToNode->Modify();
	Schema->TryCreateConnection(FromPin, ToPin);
	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree nodes connected"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::MoveBehaviorTreeChild(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParentNodeId;
	FString ChildNodeId;
	int32 TargetIndex = INDEX_NONE;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("parent_node_id"), ParentNodeId) || !Params->TryGetStringField(TEXT("child_node_id"), ChildNodeId) || !Params->TryGetNumberField(TEXT("target_index"), TargetIndex))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: parent_node_id, child_node_id, and target_index. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"parent_node_id\":\"...\",\"child_node_id\":\"...\",\"target_index\":0}") : ParamError);
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	UBehaviorTreeGraphNode* ParentNode = ParentNodeId.Equals(TEXT("root"), ESearchCase::IgnoreCase) ? FindRootNode(Graph) : FindBehaviorTreeNode(Graph, ParentNodeId);
	UBehaviorTreeGraphNode* ChildNode = FindBehaviorTreeNode(Graph, ChildNodeId);
	if (!ParentNode || !ChildNode)
	{
		return FUnreelMCPResult::Error(TEXT("Parent or child node not found"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Move Behavior Tree Child")));
	if (!MoveChildLink(ParentNode, ChildNode, TargetIndex, Error))
	{
		return FUnreelMCPResult::Error(Error);
	}
	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree child moved"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.Data->SetStringField(TEXT("parent_node_id"), ParentNodeId);
	Result.Data->SetStringField(TEXT("child_node_id"), ChildNodeId);
	Result.Data->SetNumberField(TEXT("target_index"), TargetIndex);
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::SetBehaviorTreeNodeProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString NodeId;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("node_id"), NodeId) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: node_id, property, and value. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\",\"property\":\"NodeName\",\"value\":\"Move\"}") : ParamError);
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraphNode* Node = FindBehaviorTreeNode(Graph, NodeId);
	if (!Node)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	FString Target = TEXT("instance");
	Params->TryGetStringField(TEXT("target"), Target);
	UObject* TargetObject = Target.Equals(TEXT("graph_node"), ESearchCase::IgnoreCase) ? Cast<UObject>(Node) : Node->NodeInstance.Get();
	if (!TargetObject)
	{
		return FUnreelMCPResult::Error(TEXT("Target object not available"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Set Behavior Tree Node Property")));
	FProperty* ResolvedProperty = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!ResolvedProperty)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetObjectField(TEXT("resolved_target"), BuildResolvedTarget(TargetObject, Target, PropertyName, nullptr));
		TArray<TSharedPtr<FJsonValue>> Suggestions;
		for (const FString& Name : UnreelMCPServiceUtils::SimilarPropertyNames(TargetObject->GetClass(), PropertyName))
		{
			Suggestions.Add(MakeShared<FJsonValueString>(Name));
		}
		Data->SetArrayField(TEXT("available_properties"), Suggestions);
		FUnreelMCPResult Result = FUnreelMCPResult::Error(FString::Printf(TEXT("Property not found on %s: %s"), *TargetObject->GetClass()->GetName(), *PropertyName));
		Result.Data = Data;
		return Result;
	}
	const FString BeforeValue = ExportPropertyText(TargetObject, PropertyName);
	if (!ApplyJsonProperty(TargetObject, PropertyName, Value, Error, &ResolvedProperty))
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FString AfterValue = ExportPropertyText(TargetObject, PropertyName);

	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree node property set"), BehaviorTreeSummary(BehaviorTree, Graph));
	TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
	Change->SetStringField(TEXT("target"), Target);
	Change->SetStringField(TEXT("target_path"), TargetObject->GetPathName());
	Change->SetStringField(TEXT("property"), PropertyName);
	Change->SetStringField(TEXT("before"), BeforeValue);
	Change->SetStringField(TEXT("after"), AfterValue);
	Result.Data->SetObjectField(TEXT("property_change"), Change);
	Result.Data->SetObjectField(TEXT("resolved_target"), BuildResolvedTarget(TargetObject, Target, PropertyName, ResolvedProperty));
	Result.Data->SetObjectField(TEXT("node"), BehaviorTreeNodeSummary(Node));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::RemoveBehaviorTreeNode(const TSharedRef<FJsonObject>& Params) const
{
	bool bRemoveChildren = false;
	if (Params->TryGetBoolField(TEXT("remove_children"), bRemoveChildren) && bRemoveChildren)
	{
		return RemoveBehaviorTreeSubtree(Params);
	}

	FString AssetPath;
	FString NodeId;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("node_id"), NodeId))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameter: node_id. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\"}") : ParamError);
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	UBehaviorTreeGraphNode* Node = FindBehaviorTreeNode(Graph, NodeId);
	if (!Node)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}
	if (Node->IsA<UBehaviorTreeGraphNode_Root>())
	{
		return FUnreelMCPResult::Error(TEXT("Cannot remove the root node"));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Remove Behavior Tree Node")));
	if (Node->IsSubNode())
	{
		if (UAIGraphNode* ParentNode = Node->ParentNode)
		{
			ParentNode->RemoveSubNode(Node);
		}
	}
	else
	{
		Node->DestroyNode();
	}
	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree node removed"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::RemoveBehaviorTreeSubtree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString NodeId;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")) || !Params->TryGetStringField(TEXT("node_id"), NodeId))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameter: node_id. Example: {\"asset_path\":\"/Game/AI/BT_Boss\",\"node_id\":\"...\"}") : ParamError);
	}

	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}

	UBehaviorTreeGraphNode* RootToRemove = FindBehaviorTreeNode(Graph, NodeId);
	if (!RootToRemove)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}
	if (RootToRemove->IsA<UBehaviorTreeGraphNode_Root>() || RootToRemove->IsSubNode())
	{
		return FUnreelMCPResult::Error(TEXT("remove_subtree requires a non-root graph node"));
	}

	TArray<UBehaviorTreeGraphNode*> RemovalOrder;
	TSet<UBehaviorTreeGraphNode*> Visited;
	TFunction<void(UBehaviorTreeGraphNode*)> Visit = [&](UBehaviorTreeGraphNode* Current)
	{
		if (!Current || Visited.Contains(Current))
		{
			return;
		}
		Visited.Add(Current);
		if (UEdGraphPin* OutputPin = FirstPin(Current, EGPD_Output))
		{
			TArray<UEdGraphPin*> Links = OutputPin->LinkedTo;
			for (UEdGraphPin* Link : Links)
			{
				if (Link)
				{
					Visit(Cast<UBehaviorTreeGraphNode>(Link->GetOwningNode()));
				}
			}
		}
		RemovalOrder.Add(Current);
	};
	Visit(RootToRemove);

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Remove Behavior Tree Subtree")));
	TArray<TSharedPtr<FJsonValue>> RemovedIds;
	for (UBehaviorTreeGraphNode* Node : RemovalOrder)
	{
		if (!Node || Node->IsA<UBehaviorTreeGraphNode_Root>())
		{
			continue;
		}
		RemovedIds.Add(MakeShared<FJsonValueString>(GuidString(Node->NodeGuid)));
		Node->DestroyNode();
	}

	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree subtree removed"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.Data->SetArrayField(TEXT("removed_node_ids"), RemovedIds);
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::LayoutBehaviorTree(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BT_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}
	FString Error;
	UBehaviorTree* BehaviorTree = LoadBehaviorTree(AssetPath, Error);
	if (!BehaviorTree)
	{
		return FUnreelMCPResult::Error(Error);
	}
	UBehaviorTreeGraph* Graph = EnsureBehaviorTreeGraph(BehaviorTree, Error);
	if (!Graph)
	{
		return FUnreelMCPResult::Error(Error);
	}
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Layout Behavior Tree")));
	Graph->Modify();
	int32 Row = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
		{
			continue;
		}
		Node->Modify();
		if (Node->IsA<UBehaviorTreeGraphNode_Root>())
		{
			Node->NodePosX = 0;
			Node->NodePosY = 0;
			continue;
		}
		Node->NodePosX = 300;
		Node->NodePosY = Row * 180;
		++Row;
	}
	FinalizeBehaviorTreeEdit(BehaviorTree, Graph);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Behavior Tree layout updated"), BehaviorTreeSummary(BehaviorTree, Graph));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::CreateBlackboard(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BB_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}

	FString Error;
	UObject* Created = CreateAsset(AssetPath, UBlackboardData::StaticClass(), UBlackboardDataFactory::StaticClass(), Error);
	UBlackboardData* Blackboard = Cast<UBlackboardData>(Created);
	if (!Blackboard)
	{
		return FUnreelMCPResult::Error(Error.IsEmpty() ? TEXT("Failed to create Blackboard") : Error);
	}

	FString ParentPath;
	if (Params->TryGetStringField(TEXT("parent"), ParentPath) && !ParentPath.IsEmpty())
	{
		UBlackboardData* Parent = LoadBlackboard(ParentPath, Error);
		if (!Parent)
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Blackboard created but parent assignment failed: %s"), *Error));
		}
		Blackboard->Modify();
		Blackboard->Parent = Parent;
	}

	FinalizeBlackboardEdit(Blackboard);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blackboard created"), BlackboardSummary(Blackboard));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::InspectBlackboard(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BB_Boss")))
	{
		return FUnreelMCPResult::Error(ParamError);
	}
	FString Error;
	UBlackboardData* Blackboard = LoadBlackboard(AssetPath, Error);
	if (!Blackboard)
	{
		return FUnreelMCPResult::Error(Error);
	}
	return FUnreelMCPResult::Success(TEXT("Blackboard inspected"), BlackboardSummary(Blackboard));
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::AddBlackboardKey(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BB_Boss")) || !Params->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameter: name. Example: {\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\",\"type\":\"object\"}") : ParamError);
	}

	FString Error;
	UBlackboardData* Blackboard = LoadBlackboard(AssetPath, Error);
	if (!Blackboard)
	{
		return FUnreelMCPResult::Error(Error);
	}

	FString Type = TEXT("object");
	FString ClassPath;
	Params->TryGetStringField(TEXT("type"), Type);
	Params->TryGetStringField(TEXT("key_type_class_path"), ClassPath);
	UClass* KeyTypeClass = ResolveBlackboardKeyType(Type, ClassPath);
	if (!KeyTypeClass)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Failed to resolve Blackboard key type: type=%s class=%s"), *Type, *ClassPath));
	}

	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Add Blackboard Key")));
	Blackboard->Modify();
	const FName KeyName(*Name);
	const int32 ExistingIndex = Blackboard->Keys.IndexOfByPredicate([KeyName](const FBlackboardEntry& Entry)
	{
		return Entry.EntryName == KeyName;
	});

	FBlackboardEntry Entry;
	if (ExistingIndex != INDEX_NONE)
	{
		Entry = Blackboard->Keys[ExistingIndex];
	}
	Entry.EntryName = KeyName;
	Entry.KeyType = NewObject<UBlackboardKeyType>(Blackboard, KeyTypeClass, NAME_None, RF_Transactional);

	FString Description;
	if (Params->TryGetStringField(TEXT("description"), Description))
	{
		Entry.EntryDescription = Description;
	}
	FString Category;
	if (Params->TryGetStringField(TEXT("category"), Category))
	{
		Entry.EntryCategory = FName(*Category);
	}
	bool bInstanceSynced = false;
	if (Params->TryGetBoolField(TEXT("instance_synced"), bInstanceSynced))
	{
		Entry.bInstanceSynced = bInstanceSynced;
	}

	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (Params->TryGetObjectField(TEXT("properties"), Properties) && Properties && Properties->IsValid())
	{
		if (!ApplyJsonProperties(Entry.KeyType, *Properties, Error))
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Key created but key type property assignment failed: %s"), *Error));
		}
	}

	if (ExistingIndex != INDEX_NONE)
	{
		Blackboard->Keys[ExistingIndex] = Entry;
	}
	else
	{
		Blackboard->Keys.Add(Entry);
	}

	FinalizeBlackboardEdit(Blackboard);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blackboard key added"), BlackboardSummary(Blackboard));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::RemoveBlackboardKey(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BB_Boss")) || !Params->TryGetStringField(TEXT("name"), Name))
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameter: name. Example: {\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\"}") : ParamError);
	}
	FString Error;
	UBlackboardData* Blackboard = LoadBlackboard(AssetPath, Error);
	if (!Blackboard)
	{
		return FUnreelMCPResult::Error(Error);
	}

	const FName KeyName(*Name);
	const int32 Removed = Blackboard->Keys.RemoveAll([KeyName](const FBlackboardEntry& Entry)
	{
		return Entry.EntryName == KeyName;
	});
	if (Removed == 0)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Blackboard key not found: %s"), *Name));
	}

	FinalizeBlackboardEdit(Blackboard);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blackboard key removed"), BlackboardSummary(Blackboard));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

FUnreelMCPResult FUnreelMCPBehaviorTreeService::SetBlackboardKeyProperty(const TSharedRef<FJsonObject>& Params) const
{
	FString AssetPath;
	FString Name;
	FString PropertyName;
	TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
	FString ParamError;
	if (!ReadAssetPathParam(Params, AssetPath, ParamError, TEXT("/Game/AI/BB_Boss")) || !Params->TryGetStringField(TEXT("name"), Name) || !Params->TryGetStringField(TEXT("property"), PropertyName) || !Value.IsValid())
	{
		return FUnreelMCPResult::Error(ParamError.IsEmpty() ? TEXT("Missing required parameters: name, property, and value. Example: {\"asset_path\":\"/Game/AI/BB_Boss\",\"name\":\"TargetActor\",\"property\":\"BaseClass\",\"value\":\"/Script/Engine.Actor\"}") : ParamError);
	}
	FString Error;
	UBlackboardData* Blackboard = LoadBlackboard(AssetPath, Error);
	if (!Blackboard)
	{
		return FUnreelMCPResult::Error(Error);
	}
	FBlackboardEntry* Entry = Blackboard->Keys.FindByPredicate([Name](const FBlackboardEntry& Candidate)
	{
		return Candidate.EntryName.ToString() == Name;
	});
	if (!Entry)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Blackboard key not found: %s"), *Name));
	}

	FString Target = TEXT("key_type");
	Params->TryGetStringField(TEXT("target"), Target);
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(TEXT("UnreelMCP Set Blackboard Key Property")));
	Blackboard->Modify();
	FString ChangeTarget = FString::Printf(TEXT("Blackboard key %s"), *Name);
	FString BeforeValue;
	if (Target.Equals(TEXT("entry"), ESearchCase::IgnoreCase))
	{
		if (PropertyName == TEXT("EntryName"))
		{
			BeforeValue = Entry->EntryName.ToString();
		}
		else if (PropertyName == TEXT("EntryDescription"))
		{
			BeforeValue = Entry->EntryDescription;
		}
		else if (PropertyName == TEXT("EntryCategory"))
		{
			BeforeValue = Entry->EntryCategory.ToString();
		}
		else if (PropertyName == TEXT("bInstanceSynced"))
		{
			BeforeValue = Entry->bInstanceSynced ? TEXT("true") : TEXT("false");
		}
		if (PropertyName == TEXT("EntryName"))
		{
			Entry->EntryName = FName(*JsonValueToImportText(Value));
		}
		else if (PropertyName == TEXT("EntryDescription"))
		{
			Entry->EntryDescription = JsonValueToImportText(Value);
		}
		else if (PropertyName == TEXT("EntryCategory"))
		{
			Entry->EntryCategory = FName(*JsonValueToImportText(Value));
		}
		else if (PropertyName == TEXT("bInstanceSynced"))
		{
			bool bValue = false;
			if (Value->TryGetBool(bValue))
			{
				Entry->bInstanceSynced = bValue;
			}
			else
			{
				return FUnreelMCPResult::Error(TEXT("bInstanceSynced requires a boolean value"));
			}
		}
		else
		{
			return FUnreelMCPResult::Error(FString::Printf(TEXT("Unsupported Blackboard entry property: %s"), *PropertyName));
		}
	}
	else
	{
		if (!Entry->KeyType)
		{
			return FUnreelMCPResult::Error(TEXT("Blackboard key has no key type object"));
		}
		ChangeTarget = Entry->KeyType->GetPathName();
		BeforeValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Entry->KeyType, PropertyName);
		if (!ApplyJsonProperty(Entry->KeyType, PropertyName, Value, Error))
		{
			return FUnreelMCPResult::Error(Error);
		}
	}

	FinalizeBlackboardEdit(Blackboard);
	FUnreelMCPResult Result = FUnreelMCPResult::Success(TEXT("Blackboard key property set"), BlackboardSummary(Blackboard));
	FString AfterValue;
	if (Target.Equals(TEXT("entry"), ESearchCase::IgnoreCase))
	{
		if (PropertyName == TEXT("EntryName"))
		{
			AfterValue = Entry->EntryName.ToString();
		}
		else if (PropertyName == TEXT("EntryDescription"))
		{
			AfterValue = Entry->EntryDescription;
		}
		else if (PropertyName == TEXT("EntryCategory"))
		{
			AfterValue = Entry->EntryCategory.ToString();
		}
		else if (PropertyName == TEXT("bInstanceSynced"))
		{
			AfterValue = Entry->bInstanceSynced ? TEXT("true") : TEXT("false");
		}
	}
	else
	{
		AfterValue = UnreelMCPServiceUtils::ExportObjectPropertyText(Entry->KeyType, PropertyName);
	}
	Result.Data->SetObjectField(TEXT("property_change"), UnreelMCPServiceUtils::BuildPropertyChange(ChangeTarget, PropertyName, BeforeValue, AfterValue));
	Result.ChangedAssets.Add(AssetPath);
	return Result;
}

UObject* FUnreelMCPBehaviorTreeService::LoadAssetObject(const FString& AssetPath) const
{
	return StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
}

bool FUnreelMCPBehaviorTreeService::ReadAssetPathParam(const TSharedRef<FJsonObject>& Params, FString& OutAssetPath, FString& OutError, const FString& ExamplePath) const
{
	OutAssetPath.Reset();
	OutError.Reset();
	if (!Params->TryGetStringField(TEXT("asset_path"), OutAssetPath) || OutAssetPath.IsEmpty())
	{
		Params->TryGetStringField(TEXT("asset"), OutAssetPath);
	}
	if (OutAssetPath.IsEmpty())
	{
		OutError = UnreelMCPServiceUtils::MissingAssetPathMessage(ExamplePath);
		return false;
	}
	return true;
}

UBehaviorTree* FUnreelMCPBehaviorTreeService::LoadBehaviorTree(const FString& AssetPath, FString& OutError) const
{
	UBehaviorTree* BehaviorTree = Cast<UBehaviorTree>(LoadAssetObject(AssetPath));
	if (!BehaviorTree)
	{
		OutError = FString::Printf(TEXT("Failed to load Behavior Tree: %s"), *AssetPath);
	}
	return BehaviorTree;
}

UBehaviorTreeGraph* FUnreelMCPBehaviorTreeService::EnsureBehaviorTreeGraph(UBehaviorTree* BehaviorTree, FString& OutError) const
{
	if (!BehaviorTree)
	{
		OutError = TEXT("Behavior Tree is null");
		return nullptr;
	}

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
	if (!Graph)
	{
		BehaviorTree->Modify();
		const TSubclassOf<UEdGraphSchema> SchemaClass = GetDefault<UBehaviorTreeGraph>()->Schema;
		BehaviorTree->BTGraph = FBlueprintEditorUtils::CreateNewGraph(BehaviorTree, TEXT("Behavior Tree"), UBehaviorTreeGraph::StaticClass(), SchemaClass);
		Graph = Cast<UBehaviorTreeGraph>(BehaviorTree->BTGraph);
		if (!Graph)
		{
			OutError = TEXT("Failed to create Behavior Tree graph");
			return nullptr;
		}
		const UEdGraphSchema* Schema = Graph->GetSchema();
		if (Schema)
		{
			Schema->CreateDefaultNodesForGraph(*Graph);
		}
		Graph->OnCreated();
	}
	else
	{
		Graph->OnLoaded();
	}

	Graph->Initialize();
	return Graph;
}

UBlackboardData* FUnreelMCPBehaviorTreeService::LoadBlackboard(const FString& AssetPath, FString& OutError) const
{
	UBlackboardData* Blackboard = Cast<UBlackboardData>(LoadAssetObject(AssetPath));
	if (!Blackboard)
	{
		OutError = FString::Printf(TEXT("Failed to load Blackboard: %s"), *AssetPath);
	}
	return Blackboard;
}

bool FUnreelMCPBehaviorTreeService::ResolveAssetPath(const TSharedRef<FJsonObject>& Params, FString& OutPackagePath, FString& OutAssetName, FString& OutError) const
{
	FString AssetPath;
	Params->TryGetStringField(TEXT("asset_path"), AssetPath);
	Params->TryGetStringField(TEXT("package_path"), OutPackagePath);
	Params->TryGetStringField(TEXT("asset_name"), OutAssetName);

	if (!AssetPath.IsEmpty())
	{
		if (!AssetPath.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(AssetPath, false))
		{
			OutError = FString::Printf(TEXT("Invalid /Game asset_path: %s"), *AssetPath);
			return false;
		}
		OutPackagePath = FPackageName::GetLongPackagePath(AssetPath);
		OutAssetName = FPackageName::GetLongPackageAssetName(AssetPath);
	}

	if (OutPackagePath.IsEmpty() || OutAssetName.IsEmpty())
	{
		OutError = TEXT("Provide asset_path or both package_path and asset_name");
		return false;
	}
	if (!OutPackagePath.StartsWith(TEXT("/Game")))
	{
		OutError = TEXT("Only /Game package paths are allowed");
		return false;
	}
	return true;
}

UObject* FUnreelMCPBehaviorTreeService::CreateAsset(const FString& AssetPath, UClass* AssetClass, UClass* FactoryClass, FString& OutError) const
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("asset_path"), AssetPath);
	FString PackagePath;
	FString AssetName;
	if (!ResolveAssetPath(Params, PackagePath, AssetName, OutError))
	{
		return nullptr;
	}

	if (StaticLoadObject(UObject::StaticClass(), nullptr, *(PackagePath / AssetName)))
	{
		OutError = FString::Printf(TEXT("Asset already exists: %s"), *(PackagePath / AssetName));
		return nullptr;
	}

	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	const FUnreelMCPScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("UnreelMCP Create %s"), *AssetName)));
	UObject* Asset = AssetTools.CreateAsset(AssetName, PackagePath, AssetClass, Factory);
	if (!Asset)
	{
		OutError = FString::Printf(TEXT("AssetTools failed to create: %s"), *(PackagePath / AssetName));
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
	return Asset;
}

UBehaviorTreeGraphNode* FUnreelMCPBehaviorTreeService::FindBehaviorTreeNode(UBehaviorTreeGraph* Graph, const FString& NodeId) const
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
		if (!BTNode)
		{
			continue;
		}
		if (GuidString(BTNode->NodeGuid).Equals(NodeId, ESearchCase::IgnoreCase) || BTNode->GetName().Equals(NodeId, ESearchCase::IgnoreCase))
		{
			return BTNode;
		}
		for (UAIGraphNode* SubNode : BTNode->SubNodes)
		{
			UBehaviorTreeGraphNode* BTSubNode = Cast<UBehaviorTreeGraphNode>(SubNode);
			if (BTSubNode && (GuidString(BTSubNode->NodeGuid).Equals(NodeId, ESearchCase::IgnoreCase) || BTSubNode->GetName().Equals(NodeId, ESearchCase::IgnoreCase)))
			{
				return BTSubNode;
			}
		}
	}
	return nullptr;
}

UBehaviorTreeGraphNode* FUnreelMCPBehaviorTreeService::FindRootNode(UBehaviorTreeGraph* Graph) const
{
	if (!Graph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UBehaviorTreeGraphNode_Root* Root = Cast<UBehaviorTreeGraphNode_Root>(Node))
		{
			return Root;
		}
	}
	return nullptr;
}

bool FUnreelMCPBehaviorTreeService::ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError, FProperty** OutProperty) const
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
	if (OutProperty)
	{
		*OutProperty = Property;
	}
	if (!Value.IsValid())
	{
		OutError = FString::Printf(TEXT("Missing value for property: %s"), *PropertyName);
		return false;
	}

	const FString ImportText = UnreelMCPServiceUtils::CoerceImportText(Property, Value, JsonValueToImportText(Value));
	Target->Modify();
	if (Property->ImportText_Direct(*ImportText, Property->ContainerPtrToValuePtr<void>(Target), Target, PPF_None) == nullptr)
	{
		OutError = FString::Printf(TEXT("Failed to import value for %s from '%s'"), *PropertyName, *ImportText);
		return false;
	}
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	Target->PostEditChangeProperty(PropertyChangedEvent);
	if (UBTNode* BTNode = Cast<UBTNode>(Target))
	{
		if (UBehaviorTree* Tree = Cast<UBehaviorTree>(BTNode->GetOuter()))
		{
			BTNode->InitializeFromAsset(*Tree);
		}
	}
	Target->MarkPackageDirty();
	return true;
}

bool FUnreelMCPBehaviorTreeService::ApplyJsonProperties(UObject* Target, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const
{
	if (!Properties.IsValid())
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

TSharedPtr<FJsonObject> FUnreelMCPBehaviorTreeService::BuildResolvedTarget(UObject* TargetObject, const FString& TargetKind, const FString& RequestedProperty, const FProperty* ResolvedProperty) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("target"), TargetKind);
	Data->SetStringField(TEXT("object_path"), TargetObject ? TargetObject->GetPathName() : FString());
	Data->SetStringField(TEXT("class"), TargetObject && TargetObject->GetClass() ? TargetObject->GetClass()->GetPathName() : FString());
	Data->SetStringField(TEXT("requested_property"), RequestedProperty);
	Data->SetStringField(TEXT("resolved_property"), ResolvedProperty ? ResolvedProperty->GetName() : FString());
	return Data;
}

bool FUnreelMCPBehaviorTreeService::MoveChildLink(UBehaviorTreeGraphNode* ParentNode, UBehaviorTreeGraphNode* ChildNode, int32 TargetIndex, FString& OutError) const
{
	if (!ParentNode || !ChildNode)
	{
		OutError = TEXT("Parent or child node is null");
		return false;
	}
	UEdGraphPin* FromPin = FirstPin(ParentNode, EGPD_Output);
	UEdGraphPin* ToPin = FirstPin(ChildNode, EGPD_Input);
	if (!FromPin || !ToPin)
	{
		OutError = TEXT("Parent output pin or child input pin not found");
		return false;
	}
	const int32 ExistingIndex = FromPin->LinkedTo.IndexOfByKey(ToPin);
	if (ExistingIndex == INDEX_NONE)
	{
		OutError = TEXT("Child is not connected to the specified parent");
		return false;
	}
	ParentNode->Modify();
	ChildNode->Modify();
	FromPin->Modify();
	FromPin->LinkedTo.RemoveAt(ExistingIndex);
	const int32 ClampedIndex = FMath::Clamp(TargetIndex, 0, FromPin->LinkedTo.Num());
	FromPin->LinkedTo.Insert(ToPin, ClampedIndex);
	return true;
}

FString FUnreelMCPBehaviorTreeService::ExportPropertyText(UObject* Target, const FString& PropertyName) const
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

FString FUnreelMCPBehaviorTreeService::JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const
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
	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Value, TEXT(""), Writer);
	return Json;
}

void FUnreelMCPBehaviorTreeService::FinalizeBehaviorTreeEdit(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph) const
{
	if (BehaviorTree && Graph)
	{
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UBehaviorTreeGraphNode* BTGraphNode = Cast<UBehaviorTreeGraphNode>(GraphNode);
			if (!BTGraphNode)
			{
				continue;
			}
			if (UBTNode* NodeInstance = Cast<UBTNode>(BTGraphNode->NodeInstance.Get()))
			{
				NodeInstance->InitializeFromAsset(*BehaviorTree);
			}
			for (UAIGraphNode* SubNode : BTGraphNode->SubNodes)
			{
				if (UBehaviorTreeGraphNode* BTSubNode = Cast<UBehaviorTreeGraphNode>(SubNode))
				{
					if (UBTNode* SubNodeInstance = Cast<UBTNode>(BTSubNode->NodeInstance.Get()))
					{
						SubNodeInstance->InitializeFromAsset(*BehaviorTree);
					}
				}
			}
		}
	}
	if (Graph)
	{
		Graph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags);
		Graph->RebuildExecutionOrder();
		Graph->MarkVersion();
	}
	if (BehaviorTree)
	{
		BehaviorTree->MarkPackageDirty();
	}
}

void FUnreelMCPBehaviorTreeService::FinalizeBlackboardEdit(UBlackboardData* Blackboard) const
{
	if (!Blackboard)
	{
		return;
	}
	Blackboard->UpdateParentKeys();
	Blackboard->UpdateKeyIDs();
	Blackboard->UpdateIfHasSynchronizedKeys();
	Blackboard->PropagateKeyChangesToDerivedBlackboardAssets();
	Blackboard->MarkPackageDirty();
}

TSharedPtr<FJsonObject> FUnreelMCPBehaviorTreeService::BehaviorTreeSummary(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph) const
{
	TSharedPtr<FJsonObject> Data = ObjectRef(BehaviorTree);
	Data->SetObjectField(TEXT("blackboard"), ObjectRef(BehaviorTree ? BehaviorTree->BlackboardAsset.Get() : nullptr));
	Data->SetObjectField(TEXT("root_node"), ObjectRef(BehaviorTree ? BehaviorTree->RootNode.Get() : nullptr));
	TArray<TSharedPtr<FJsonValue>> Nodes;
	if (Graph)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (const UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node))
			{
				Nodes.Add(MakeShared<FJsonValueObject>(BehaviorTreeNodeSummary(BTNode)));
			}
		}
	}
	Data->SetArrayField(TEXT("nodes"), Nodes);
	Data->SetNumberField(TEXT("node_count"), Nodes.Num());
	return Data;
}

TSharedPtr<FJsonObject> FUnreelMCPBehaviorTreeService::BlackboardSummary(UBlackboardData* Blackboard) const
{
	TSharedPtr<FJsonObject> Data = ObjectRef(Blackboard);
	Data->SetObjectField(TEXT("parent"), ObjectRef(Blackboard ? Blackboard->Parent.Get() : nullptr));
	TArray<TSharedPtr<FJsonValue>> Keys;
	if (Blackboard)
	{
		for (int32 Index = 0; Index < Blackboard->Keys.Num(); ++Index)
		{
			const FBlackboardEntry& Entry = Blackboard->Keys[Index];
			TSharedPtr<FJsonObject> Key = MakeShared<FJsonObject>();
			Key->SetNumberField(TEXT("index"), Index);
			Key->SetStringField(TEXT("name"), Entry.EntryName.ToString());
			Key->SetStringField(TEXT("description"), Entry.EntryDescription);
			Key->SetStringField(TEXT("category"), Entry.EntryCategory.ToString());
			Key->SetBoolField(TEXT("instance_synced"), Entry.bInstanceSynced != 0);
			Key->SetObjectField(TEXT("key_type"), ObjectRef(Entry.KeyType));
			Keys.Add(MakeShared<FJsonValueObject>(Key));
		}
	}
	Data->SetArrayField(TEXT("keys"), Keys);
	Data->SetNumberField(TEXT("key_count"), Keys.Num());
	return Data;
}

TSharedPtr<FJsonObject> FUnreelMCPBehaviorTreeService::BehaviorTreeNodeSummary(const UBehaviorTreeGraphNode* Node) const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	if (!Node)
	{
		return Data;
	}
	Data->SetStringField(TEXT("id"), GuidString(Node->NodeGuid));
	Data->SetStringField(TEXT("name"), Node->GetName());
	Data->SetStringField(TEXT("graph_class"), Node->GetClass()->GetPathName());
	Data->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
	Data->SetStringField(TEXT("description"), Node->GetDescription().ToString());
	Data->SetBoolField(TEXT("is_root"), Node->IsA<UBehaviorTreeGraphNode_Root>());
	Data->SetBoolField(TEXT("is_subnode"), Node->IsSubNode());
	Data->SetNumberField(TEXT("x"), Node->NodePosX);
	Data->SetNumberField(TEXT("y"), Node->NodePosY);
	Data->SetObjectField(TEXT("instance"), ObjectRef(Node->NodeInstance));
	if (Node->ParentNode)
	{
		Data->SetStringField(TEXT("parent_node_id"), GuidString(Node->ParentNode->NodeGuid));
	}

	TArray<TSharedPtr<FJsonValue>> Pins;
	for (const UEdGraphPin* Pin : Node->Pins)
	{
		TSharedPtr<FJsonObject> PinData = MakeShared<FJsonObject>();
		PinData->SetStringField(TEXT("name"), Pin->PinName.ToString());
		PinData->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
		TArray<TSharedPtr<FJsonValue>> Links;
		for (const UEdGraphPin* Link : Pin->LinkedTo)
		{
			if (Link && Link->GetOwningNode())
			{
				if (const UBehaviorTreeGraphNode* LinkedBTNode = Cast<UBehaviorTreeGraphNode>(Link->GetOwningNode()))
				{
					Links.Add(MakeShared<FJsonValueString>(GuidString(LinkedBTNode->NodeGuid)));
				}
			}
		}
		PinData->SetArrayField(TEXT("links"), Links);
		Pins.Add(MakeShared<FJsonValueObject>(PinData));
	}
	Data->SetArrayField(TEXT("pins"), Pins);

	TArray<TSharedPtr<FJsonValue>> SubNodes;
	for (const UAIGraphNode* SubNode : Node->SubNodes)
	{
		if (const UBehaviorTreeGraphNode* BTSubNode = Cast<UBehaviorTreeGraphNode>(SubNode))
		{
			SubNodes.Add(MakeShared<FJsonValueObject>(BehaviorTreeNodeSummary(BTSubNode)));
		}
	}
	Data->SetArrayField(TEXT("subnodes"), SubNodes);
	return Data;
}
