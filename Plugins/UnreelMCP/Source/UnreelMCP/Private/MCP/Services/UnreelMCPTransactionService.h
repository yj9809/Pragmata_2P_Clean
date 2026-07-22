#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"

class FScopedTransaction;

namespace UnreelMCPTransactions
{
	bool IsExplicitTransactionActive();
	TSharedPtr<FJsonObject> MakeStatusData();
}

class FUnreelMCPScopedTransaction
{
public:
	explicit FUnreelMCPScopedTransaction(const FText& SessionName);
	~FUnreelMCPScopedTransaction();

private:
	TUniquePtr<FScopedTransaction> Transaction;
};

class FUnreelMCPTransactionService : public IUnreelMCPService
{
public:
	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult Status() const;
	FUnreelMCPResult Begin(const TSharedRef<FJsonObject>& Params);
	FUnreelMCPResult End();
	FUnreelMCPResult Cancel();
};
