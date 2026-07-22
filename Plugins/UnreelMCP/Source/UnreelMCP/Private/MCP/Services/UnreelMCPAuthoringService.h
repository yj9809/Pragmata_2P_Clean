#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class UBlueprint;
class UObject;
class USoundCue;
class USoundNode;
class UMetaSoundBuilderBase;
class UWidget;
class UWidgetBlueprint;
class UWidgetTree;
class ALandscape;
class UStateTree;
class UStateTreeEditorData;
class UStateTreeState;
struct FStateTreeEditorNode;
class UNiagaraSystem;
class UNiagaraScript;
class UNiagaraNodeFunctionCall;
class UNiagaraNodeOutput;

class FUnreelMCPAuthoringService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult CreateAssetOperation(const TSharedRef<FJsonObject>& Params, const FString& DefaultClassPath = FString(), const FString& DefaultFactoryClassPath = FString()) const;
	FUnreelMCPResult SetAssetProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateBlueprint(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddBlueprintVariable(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CompileBlueprint(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListWidgetTree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ReparentWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveWidgetTreeWidget(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetWidgetTreeWidgetProperty(const TSharedRef<FJsonObject>& Params, bool bSlotProperty) const;
	FUnreelMCPResult ListWidgetEvents(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult BindWidgetEvent(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveWidgetEventBinding(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListWidgetBindings(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddWidgetBinding(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveWidgetBinding(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListWidgetMVVM(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddWidgetMVVMViewModel(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveWidgetMVVMViewModel(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateDataTable(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMaterialInstanceParameter(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult GenerateStaticMeshUV(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateSkeletalPhysicsAsset(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectFoliage(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddFoliageInstance(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddFoliageInstances(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveFoliageInstances(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateLandscapeLayerInfo(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectLandscape(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateLandscape(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetLandscapeHeightRegion(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult PaintLandscapeLayerRegion(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectSoundCue(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddSoundCueNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectSoundCueNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult DisconnectSoundCueNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetSoundCueOutput(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetSoundCueNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveSoundCueNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CreateMetaSoundSource(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectMetaSound(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMetaSoundNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMetaSoundGraphInput(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddMetaSoundGraphOutput(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ConnectMetaSoundNodes(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult DisconnectMetaSoundInput(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetMetaSoundNodeInputDefault(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveMetaSoundNode(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectNiagaraStack(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddNiagaraScriptModule(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddNiagaraParameterModule(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveNiagaraModule(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult MoveNiagaraModule(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetNiagaraModuleEnabled(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetNiagaraModuleInputDefault(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectNiagaraScratchPad(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddNiagaraScratchPadScript(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RemoveNiagaraScratchPadScript(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult InspectStateTree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateTreeState(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateTreeTask(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateTreeEvaluator(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult AddStateTreeTransition(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SetStateTreeNodeProperty(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult CompileStateTree(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult UnsupportedBestEffort(const FString& OperationId, const FString& Reason) const;

	UObject* LoadAssetObject(const FString& AssetPath) const;
	UWidgetBlueprint* LoadWidgetBlueprint(const FString& AssetPath, FString& OutError) const;
	UWidget* FindWidgetByName(UWidgetTree* WidgetTree, const FString& WidgetName) const;
	TSharedPtr<FJsonObject> WidgetSummary(UWidget* Widget) const;
	TSharedPtr<FJsonObject> WidgetHierarchySummary(UWidget* Widget) const;
	void MarkWidgetBlueprintChanged(UWidgetBlueprint* WidgetBlueprint, bool bStructurallyModified) const;
	bool ResolveAssetPath(const TSharedRef<FJsonObject>& Params, FString& OutPackagePath, FString& OutAssetName, FString& OutError) const;
	bool ApplyJsonProperties(UObject* Target, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const;
	bool ApplyJsonProperty(UObject* Target, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	bool ApplyJsonStructProperties(const UStruct* Struct, void* StructMemory, UObject* Owner, const TSharedPtr<FJsonObject>& Properties, FString& OutError) const;
	bool ApplyJsonStructProperty(const UStruct* Struct, void* StructMemory, UObject* Owner, const FString& PropertyName, const TSharedPtr<FJsonValue>& Value, FString& OutError) const;
	FString JsonValueToImportText(const TSharedPtr<FJsonValue>& Value) const;
	TSharedPtr<FJsonObject> ObjectSummary(UObject* Object) const;
	UStateTree* LoadStateTree(const FString& AssetPath, FString& OutError) const;
	UStateTreeEditorData* LoadStateTreeEditorData(UStateTree* StateTree, FString& OutError) const;
	UStateTreeState* FindStateTreeState(UStateTreeEditorData* EditorData, const FString& Identifier) const;
	FStateTreeEditorNode* FindStateTreeNode(UStateTreeEditorData* EditorData, const FString& NodeId, UObject*& OutOuter) const;
	bool ApplyStateTreeNodeProperties(FStateTreeEditorNode& Node, UObject* Owner, const TSharedRef<FJsonObject>& Params, FString& OutError) const;
	void FinalizeStateTreeEdit(UStateTree* StateTree, UStateTreeEditorData* EditorData) const;
	TSharedPtr<FJsonObject> StateTreeSummary(UStateTree* StateTree, UStateTreeEditorData* EditorData) const;
	ALandscape* ResolveLandscape(const TSharedRef<FJsonObject>& Params, FString& OutError) const;
	TSharedPtr<FJsonObject> LandscapeSummary(ALandscape* Landscape) const;
	USoundNode* ResolveSoundCueNode(USoundCue* SoundCue, const TSharedRef<FJsonObject>& Params, int32& OutIndex) const;
	TSharedPtr<FJsonObject> SoundCueSummary(USoundCue* SoundCue) const;
	UObject* LoadMetaSoundDocumentObject(const FString& AssetPath) const;
	UMetaSoundBuilderBase* CreateMetaSoundBuilder(UObject* MetaSoundObject) const;
	bool BuildMetaSoundLiteral(const TSharedPtr<FJsonValue>& Value, const FString& DataType, struct FMetasoundFrontendLiteral& OutLiteral, FString& OutError) const;
	TSharedPtr<FJsonObject> MetaSoundSummary(UObject* MetaSoundObject) const;
	TSharedPtr<FJsonObject> NiagaraStackSummary(UNiagaraSystem* System, UNiagaraScript* Script, UNiagaraNodeOutput* OutputNode, const FString& EmitterName) const;
	TSharedPtr<FJsonObject> NiagaraScratchPadSummary(UNiagaraSystem* System) const;
};
