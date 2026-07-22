#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class FUnreelMCPCoreService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult ExecutePing() const;
	FUnreelMCPResult ExecuteContext() const;
};
