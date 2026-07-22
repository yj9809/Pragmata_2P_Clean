#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class FUnreelMCPServiceRegistry
{
public:
	void RegisterService(TSharedRef<IUnreelMCPService> Service);
	TArray<FUnreelMCPOperation> GetAllOperations() const;
	TOptional<FUnreelMCPOperation> FindOperation(const FString& OperationId) const;
	TArray<FUnreelMCPOperation> SearchOperations(const FString& Query, const FString& Domain, int32 Limit = 20) const;
	FUnreelMCPResult ExecuteOperation(const FString& OperationId, const TSharedRef<FJsonObject>& Params) const;

private:
	TMap<FString, TSharedRef<IUnreelMCPService>> ServicesByName;
	TMap<FString, TSharedRef<IUnreelMCPService>> ServicesByOperationId;
	TMap<FString, FUnreelMCPOperation> OperationsById;
};
