#include "MCP/Services/UnreelMCPTransactionService.h"

#include "ScopedTransaction.h"

namespace
{
	TUniquePtr<FScopedTransaction> GActiveTransaction;
	FString GActiveLabel;
	FDateTime GActiveStartedAt;

	FUnreelMCPParameter TransactionParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FUnreelMCPOperation TransactionOp(const FString& Id, const FString& Operation, const FString& Description, const TArray<FUnreelMCPParameter>& Params, bool bReadOnly, EUnreelMCPRisk Risk, const FString& Example)
	{
		FUnreelMCPOperation Op;
		Op.Id = Id;
		Op.Domain = TEXT("transaction");
		Op.Operation = Operation;
		Op.Description = Description;
		Op.Keywords = { TEXT("transaction"), TEXT("undo"), TEXT("editor") };
		Op.Parameters = Params;
		Op.bReadOnly = bReadOnly;
		Op.bRequiresTransaction = false;
		Op.Risk = Risk;
		Op.ExampleJson = Example;
		return Op;
	}
}

namespace UnreelMCPTransactions
{
	bool IsExplicitTransactionActive()
	{
		return GActiveTransaction.IsValid();
	}

	TSharedPtr<FJsonObject> MakeStatusData()
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetBoolField(TEXT("active"), GActiveTransaction.IsValid());
		Data->SetStringField(TEXT("label"), GActiveLabel);
		Data->SetStringField(TEXT("started_at"), GActiveStartedAt.ToIso8601());
		Data->SetStringField(TEXT("policy"), TEXT("Mutating operations create scoped transactions by default. Use explicit transactions only for short related batches; nested per-call transactions are suppressed while active."));
		return Data;
	}
}

FUnreelMCPScopedTransaction::FUnreelMCPScopedTransaction(const FText& SessionName)
{
	if (!UnreelMCPTransactions::IsExplicitTransactionActive())
	{
		Transaction = MakeUnique<FScopedTransaction>(SessionName);
	}
}

FUnreelMCPScopedTransaction::~FUnreelMCPScopedTransaction() = default;

FString FUnreelMCPTransactionService::GetServiceName() const
{
	return TEXT("transaction");
}

TArray<FUnreelMCPOperation> FUnreelMCPTransactionService::GetOperations() const
{
	return {
		TransactionOp(
			TEXT("transaction.status"),
			TEXT("status"),
			TEXT("Report whether an explicit UnreelMCP transaction is active."),
			{},
			true,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"transaction.status\",\"params\":{}}")),
		TransactionOp(
			TEXT("transaction.begin"),
			TEXT("begin"),
			TEXT("Begin an explicit Unreal Editor transaction for a short sequence of mutating calls. Do not leave it open across unrelated work."),
			{ TransactionParam(TEXT("label"), TEXT("string"), TEXT("Undo label for the transaction."), false, TEXT("UnreelMCP Transaction")) },
			false,
			EUnreelMCPRisk::Medium,
			TEXT("{\"operation_id\":\"transaction.begin\",\"params\":{\"label\":\"Create combat assets\"}}")),
		TransactionOp(
			TEXT("transaction.end"),
			TEXT("end"),
			TEXT("Commit the active explicit UnreelMCP transaction."),
			{},
			false,
			EUnreelMCPRisk::Medium,
			TEXT("{\"operation_id\":\"transaction.end\",\"params\":{}}")),
		TransactionOp(
			TEXT("transaction.cancel"),
			TEXT("cancel"),
			TEXT("Cancel the active explicit UnreelMCP transaction, removing it from the undo stack."),
			{},
			false,
			EUnreelMCPRisk::Medium,
			TEXT("{\"operation_id\":\"transaction.cancel\",\"params\":{}}"))
	};
}

FUnreelMCPResult FUnreelMCPTransactionService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("transaction.status")) return Status();
	if (OperationId == TEXT("transaction.begin")) return Begin(Params);
	if (OperationId == TEXT("transaction.end")) return End();
	if (OperationId == TEXT("transaction.cancel")) return Cancel();
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown transaction operation: %s"), *OperationId));
}

FUnreelMCPResult FUnreelMCPTransactionService::Status() const
{
	return FUnreelMCPResult::Success(GActiveTransaction.IsValid() ? TEXT("Transaction active") : TEXT("No active transaction"), UnreelMCPTransactions::MakeStatusData());
}

FUnreelMCPResult FUnreelMCPTransactionService::Begin(const TSharedRef<FJsonObject>& Params)
{
	if (GActiveTransaction.IsValid())
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("A transaction is already active: %s"), *GActiveLabel));
	}

	FString Label = TEXT("UnreelMCP Transaction");
	Params->TryGetStringField(TEXT("label"), Label);
	if (Label.IsEmpty())
	{
		Label = TEXT("UnreelMCP Transaction");
	}

	GActiveLabel = Label;
	GActiveStartedAt = FDateTime::UtcNow();
	GActiveTransaction = MakeUnique<FScopedTransaction>(FText::FromString(Label));
	return Status();
}

FUnreelMCPResult FUnreelMCPTransactionService::End()
{
	if (!GActiveTransaction.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("No active transaction to end"));
	}

	const FString Label = GActiveLabel;
	GActiveTransaction.Reset();
	GActiveLabel.Reset();
	GActiveStartedAt = FDateTime();
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Committed transaction: %s"), *Label));
}

FUnreelMCPResult FUnreelMCPTransactionService::Cancel()
{
	if (!GActiveTransaction.IsValid())
	{
		return FUnreelMCPResult::Error(TEXT("No active transaction to cancel"));
	}

	const FString Label = GActiveLabel;
	GActiveTransaction->Cancel();
	GActiveTransaction.Reset();
	GActiveLabel.Reset();
	GActiveStartedAt = FDateTime();
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Canceled transaction: %s"), *Label));
}
