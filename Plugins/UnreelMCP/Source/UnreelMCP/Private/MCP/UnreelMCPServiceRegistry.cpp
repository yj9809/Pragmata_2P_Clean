#include "MCP/UnreelMCPServiceRegistry.h"

#include "MCP/UnreelMCPServiceUtils.h"

namespace
{
	void TokenizeSearchText(FString Text, TArray<FString>& OutTokens)
	{
		Text = Text.ToLower();
		Text.ReplaceInline(TEXT("."), TEXT(" "));
		Text.ReplaceInline(TEXT("_"), TEXT(" "));
		Text.ReplaceInline(TEXT("-"), TEXT(" "));
		Text.ParseIntoArrayWS(OutTokens);
	}

	FString BuildSearchText(const FUnreelMCPOperation& Operation)
	{
		FString SearchText = Operation.Id.ToLower() + TEXT(" ") + Operation.Operation.ToLower() + TEXT(" ") + Operation.Domain.ToLower() + TEXT(" ") + Operation.Description.ToLower();
		for (const FString& Keyword : Operation.Keywords)
		{
			SearchText += TEXT(" ");
			SearchText += Keyword.ToLower();
		}
		return SearchText;
	}

	int32 ScoreOperation(const FUnreelMCPOperation& Operation, const FString& QueryLower, const TArray<FString>& QueryTerms)
	{
		if (QueryLower.IsEmpty())
		{
			return 1;
		}

		const FString IdLower = Operation.Id.ToLower();
		const FString OperationLower = Operation.Operation.ToLower();
		const FString DomainLower = Operation.Domain.ToLower();
		const FString SearchText = BuildSearchText(Operation);
		TArray<FString> OperationTokens;
		TokenizeSearchText(SearchText, OperationTokens);

		int32 Score = 0;
		if (IdLower == QueryLower)
		{
			Score += 10000;
		}
		else if (IdLower.Contains(QueryLower))
		{
			Score += 2500;
		}
		else if (SearchText.Contains(QueryLower))
		{
			Score += 500;
		}

		for (const FString& Term : QueryTerms)
		{
			if (Term.IsEmpty())
			{
				continue;
			}
			if (IdLower == Term || OperationLower == Term)
			{
				Score += 300;
			}
			else if (IdLower.Contains(Term) || OperationLower.Contains(Term))
			{
				Score += 160;
			}
			if (DomainLower == Term || DomainLower.Contains(Term))
			{
				Score += 120;
			}
			if (OperationTokens.Contains(Term))
			{
				Score += 80;
			}
			else if (SearchText.Contains(Term))
			{
				Score += 20;
			}
		}
		return Score;
	}
}

void FUnreelMCPServiceRegistry::RegisterService(TSharedRef<IUnreelMCPService> Service)
{
	ServicesByName.Add(Service->GetServiceName(), Service);

	for (const FUnreelMCPOperation& Operation : Service->GetOperations())
	{
		OperationsById.Add(Operation.Id, Operation);
		ServicesByOperationId.Add(Operation.Id, Service);
	}
}

TArray<FUnreelMCPOperation> FUnreelMCPServiceRegistry::GetAllOperations() const
{
	TArray<FUnreelMCPOperation> Operations;
	OperationsById.GenerateValueArray(Operations);
	Operations.Sort([](const FUnreelMCPOperation& A, const FUnreelMCPOperation& B)
	{
		return A.Id < B.Id;
	});
	return Operations;
}

TOptional<FUnreelMCPOperation> FUnreelMCPServiceRegistry::FindOperation(const FString& OperationId) const
{
	if (const FUnreelMCPOperation* Operation = OperationsById.Find(OperationId))
	{
		return *Operation;
	}
	return TOptional<FUnreelMCPOperation>();
}

TArray<FUnreelMCPOperation> FUnreelMCPServiceRegistry::SearchOperations(const FString& Query, const FString& Domain, int32 Limit) const
{
	const FString QueryLower = Query.ToLower();
	const FString DomainLower = Domain.ToLower();
	TArray<FString> QueryTerms;
	TokenizeSearchText(QueryLower, QueryTerms);
	struct FScoredOperation
	{
		FUnreelMCPOperation Operation;
		int32 Score = 0;
	};
	TArray<FScoredOperation> ScoredMatches;

	for (const TPair<FString, FUnreelMCPOperation>& Pair : OperationsById)
	{
		const FUnreelMCPOperation& Operation = Pair.Value;
		if (!DomainLower.IsEmpty() && Operation.Domain.ToLower() != DomainLower)
		{
			continue;
		}

		const int32 Score = ScoreOperation(Operation, QueryLower, QueryTerms);
		if (Score > 0)
		{
			FScoredOperation Match;
			Match.Operation = Operation;
			Match.Score = Score;
			ScoredMatches.Add(Match);
		}
	}

	ScoredMatches.Sort([](const FScoredOperation& A, const FScoredOperation& B)
	{
		if (A.Score != B.Score)
		{
			return A.Score > B.Score;
		}
		return A.Operation.Id < B.Operation.Id;
	});

	TArray<FUnreelMCPOperation> Matches;
	const int32 ClampedLimit = Limit <= 0 ? 20 : Limit;
	for (const FScoredOperation& Match : ScoredMatches)
	{
		Matches.Add(Match.Operation);
		if (Matches.Num() >= ClampedLimit)
		{
			break;
		}
	}
	return Matches;
}

FUnreelMCPResult FUnreelMCPServiceRegistry::ExecuteOperation(const FString& OperationId, const TSharedRef<FJsonObject>& Params) const
{
	const TSharedRef<IUnreelMCPService>* Service = ServicesByOperationId.Find(OperationId);
	if (!Service)
	{
		return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown operation: %s"), *OperationId));
	}
	UnreelMCPServiceUtils::NormalizeAssetAlias(OperationId, Params);
	FUnreelMCPResult Result = (*Service)->Execute(OperationId, Params);
	FString SuppliedAssetPath;
	const bool bHasSuppliedAssetPath = Params->TryGetStringField(TEXT("asset_path"), SuppliedAssetPath) && !SuppliedAssetPath.TrimStartAndEnd().IsEmpty();
	if (!Result.bSuccess && !bHasSuppliedAssetPath && !UnreelMCPServiceUtils::IsCreationOperation(OperationId) && Result.Message.Contains(TEXT("asset_path")) && !Result.Message.Contains(TEXT("alias: asset")))
	{
		Result.Message += TEXT(". ");
		Result.Message += UnreelMCPServiceUtils::MissingAssetPathMessage();
	}
	if (!Result.bSuccess && Result.Message.Contains(TEXT("/Script/")) && !Result.Message.Contains(TEXT("stale editor DLLs")))
	{
		const int32 ScriptStart = Result.Message.Find(TEXT("/Script/"));
		if (ScriptStart != INDEX_NONE)
		{
			FString ScriptPath = Result.Message.Mid(ScriptStart);
			int32 EndIndex = INDEX_NONE;
			if (ScriptPath.FindChar(TEXT(' '), EndIndex) || ScriptPath.FindChar(TEXT('.'), EndIndex))
			{
				ScriptPath = ScriptPath.Left(EndIndex);
			}
			Result.Message += UnreelMCPServiceUtils::ScriptLoadHint(ScriptPath);
		}
	}
	return Result;
}
