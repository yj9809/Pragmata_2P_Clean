#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UAnimationStateMachineGraph;
class UAnimStateNodeBase;
class UAnimMontage;
class UMaterial;
class UMaterialExpression;
class UObject;
class UEdGraphPin;
struct FAnimNotifyEvent;

class FUnreelMCPParityService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult ListBlueprintGraphs(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateBlueprintFunction(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlueprintEvent(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlueprintFunctionCall(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlueprintVariableNode(const TSharedRef<FJsonObject>& Params, bool bSetter) const;
	FUnreelMCPResult AddBlueprintNodeByClass(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlueprintCommonNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetBlueprintNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetBlueprintPinDefault(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectBlueprintPins(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult LayoutBlueprintGraph(const TSharedRef<FJsonObject>& Params) const;

	FUnreelMCPResult ListMaterialGraph(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMaterialExpression(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMaterialParameter(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMaterialTexture(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMaterialFunctionCall(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMaterialExpressionProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectMaterialExpressions(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMaterialOutput(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult LayoutMaterialGraph(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CompileMaterial(const TSharedRef<FJsonObject>& Params) const;

	FUnreelMCPResult ListInputMappings(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddInputMapping(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveInputMapping(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetInputActionValueType(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetInputMappingObjects(const TSharedRef<FJsonObject>& Params, bool bTriggers) const;
	FUnreelMCPResult SetInputMappingObjectProperty(const TSharedRef<FJsonObject>& Params, bool bTriggers) const;

	FUnreelMCPResult InspectAnimBlueprint(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectAnimGraph(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddAnimGraphNodeByClass(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetAnimGraphNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetAnimGraphNodeAnimationAsset(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectAnimGraphPosePins(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateMachine(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectStateMachine(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateMachineState(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateMachineAlias(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetStateMachineAlias(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectStateMachineStates(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetStateMachineNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListAnimationMetadata(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddAnimationMetadata(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveAnimationMetadata(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetAnimationMetadataProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListAnimationNotifies(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddAnimationNotify(const TSharedRef<FJsonObject>& Params, bool bState) const;
	FUnreelMCPResult MoveAnimationNotify(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveAnimationNotify(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetAnimationNotifyProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectMontage(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMontageSlot(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveMontageSlot(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMontageSection(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMontageSection(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveMontageSection(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMontageSegment(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMontageSegment(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveMontageSegment(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult Unsupported(const FString& OperationId, const FString& Reason) const;

	UObject* LoadAssetObject(const FString& AssetPath) const;
	UClass* ResolveAnimationMetadataClass(const FString& ClassPath) const;
	UClass* ResolveAnimNotifyClass(const FString& ClassPath, bool bState) const;
	UBlueprint* LoadBlueprint(const TSharedRef<FJsonObject>& Params) const;
	UEdGraph* FindBlueprintGraph(UBlueprint* Blueprint, const FString& GraphName) const;
	UEdGraph* FindAnimGraph(UBlueprint* Blueprint, const FString& GraphName) const;
	UAnimationStateMachineGraph* FindStateMachineGraph(UBlueprint* Blueprint, const FString& StateMachine) const;
	UAnimStateNodeBase* FindStateMachineStateNode(UAnimationStateMachineGraph* Graph, const FString& Key) const;
	UEdGraphNode* FindNode(UEdGraph* Graph, const FString& NodeId) const;
	UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, const FString& Direction) const;
	UMaterial* LoadMaterial(const TSharedRef<FJsonObject>& Params) const;
	UMaterialExpression* GetMaterialExpression(UMaterial* Material, int32 Index) const;
	bool ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	bool ApplyJsonStructProperty(UScriptStruct* Struct, void* StructData, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	FString JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const;
	TSharedPtr<FJsonObject> NodeSummary(const UEdGraphNode* Node) const;
	TSharedPtr<FJsonObject> ExpressionSummary(const UMaterialExpression* Expression, int32 Index) const;
	TSharedPtr<FJsonObject> NotifySummary(const FAnimNotifyEvent& Event, int32 Index) const;
	TSharedPtr<FJsonObject> MontageSummary(const UAnimMontage* Montage) const;
};
