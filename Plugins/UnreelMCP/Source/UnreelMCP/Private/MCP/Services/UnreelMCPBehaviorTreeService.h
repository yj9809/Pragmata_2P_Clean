#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class UBehaviorTree;
class UBehaviorTreeGraph;
class UBehaviorTreeGraphNode;
class UBlackboardData;
class UEdGraphNode;
class UObject;
class FProperty;

class FUnreelMCPBehaviorTreeService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult CreateBehaviorTree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectBehaviorTree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetBehaviorTreeBlackboard(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBehaviorTreeNode(const TSharedRef<FJsonObject>& Params, const FString& Kind) const;
	FUnreelMCPResult AddBehaviorTreeSubNode(const TSharedRef<FJsonObject>& Params, const FString& Kind) const;
	FUnreelMCPResult AddBehaviorTreeWait(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBehaviorTreeBlackboardDecorator(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectBehaviorTreeNodes(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult MoveBehaviorTreeChild(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetBehaviorTreeNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveBehaviorTreeSubtree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveBehaviorTreeNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult LayoutBehaviorTree(const TSharedRef<FJsonObject>& Params) const;

	FUnreelMCPResult CreateBlackboard(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectBlackboard(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlackboardKey(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveBlackboardKey(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetBlackboardKeyProperty(const TSharedRef<FJsonObject>& Params) const;

	UObject* LoadAssetObject(const FString& AssetPath) const;
	bool ReadAssetPathParam(const TSharedRef<FJsonObject>& Params, FString& OutAssetPath, FString& OutError, const FString& ExamplePath) const;
	UBehaviorTree* LoadBehaviorTree(const FString& AssetPath, FString& OutError) const;
	UBehaviorTreeGraph* EnsureBehaviorTreeGraph(UBehaviorTree* BehaviorTree, FString& OutError) const;
	UBlackboardData* LoadBlackboard(const FString& AssetPath, FString& OutError) const;
	bool ResolveAssetPath(const TSharedRef<FJsonObject>& Params, FString& OutPackagePath, FString& OutAssetName, FString& OutError) const;
	UObject* CreateAsset(const FString& AssetPath, UClass* AssetClass, UClass* FactoryClass, FString& OutError) const;
	UBehaviorTreeGraphNode* FindBehaviorTreeNode(UBehaviorTreeGraph* Graph, const FString& NodeId) const;
	UBehaviorTreeGraphNode* FindRootNode(UBehaviorTreeGraph* Graph) const;
	bool ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError, FProperty** OutProperty = nullptr) const;
	bool ApplyJsonProperties(UObject* Target, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const;
	bool MoveChildLink(UBehaviorTreeGraphNode* ParentNode, UBehaviorTreeGraphNode* ChildNode, int32 TargetIndex, FString& OutError) const;
	TSharedPtr<FJsonObject> BuildResolvedTarget(UObject* TargetObject, const FString& TargetKind, const FString& RequestedProperty, const FProperty* ResolvedProperty) const;
	FString ExportPropertyText(UObject* Target, const FString& PropertyName) const;
	FString JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const;
	void FinalizeBehaviorTreeEdit(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph) const;
	void FinalizeBlackboardEdit(UBlackboardData* Blackboard) const;
	TSharedPtr<FJsonObject> BehaviorTreeSummary(UBehaviorTree* BehaviorTree, UBehaviorTreeGraph* Graph) const;
	TSharedPtr<FJsonObject> BlackboardSummary(UBlackboardData* Blackboard) const;
	TSharedPtr<FJsonObject> BehaviorTreeNodeSummary(const UBehaviorTreeGraphNode* Node) const;
};
