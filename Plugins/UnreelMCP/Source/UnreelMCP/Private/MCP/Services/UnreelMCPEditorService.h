#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class UWorld;

class FUnreelMCPEditorService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	UWorld* GetEditorWorld(FString& OutError) const;
	FVector ReadVector(const TSharedRef<FJsonObject>& Params, const FString& FieldName, const FVector& DefaultValue) const;

	FUnreelMCPResult ListActors(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SpawnActor(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SearchAssets(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult GetAssetInfo(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult SaveAsset(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult TailLog(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult RequestScreenshot(const TSharedRef<FJsonObject>& Params) const;
};
