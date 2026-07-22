#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

struct FAssetData;

class FUnreelMCPBreadthService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPOperation MakeSearchOperation(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FString>& Keywords, const FString& DefaultClasses) const;
	FUnreelMCPResult SearchByClasses(const TSharedRef<FJsonObject>& Params, const FString& DefaultClasses, const FString& Label) const;
	FUnreelMCPResult DescribeAsset(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult DescribeClass(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ProjectSettings() const;
	FUnreelMCPResult ScanConfig(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult PythonStatus() const;
	FUnreelMCPResult ExecutePython(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ExecutePythonScript(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ListMaps(const TSharedRef<FJsonObject>& Params) const;

	TArray<FAssetData> QueryAssets(const FString& Directory, int32 Limit) const;
};
