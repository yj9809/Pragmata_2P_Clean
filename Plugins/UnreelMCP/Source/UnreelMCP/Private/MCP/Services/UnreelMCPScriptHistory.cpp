#include "MCP/Services/UnreelMCPScriptHistory.h"

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace
{
	struct FScriptHistoryEntry
	{
		FDateTime Timestamp;
		FString Code;
		bool bAccepted = false;
		FString Message;
	};

	TArray<FScriptHistoryEntry> GScriptHistory;
	constexpr int32 MaxScriptHistory = 200;
}

namespace UnreelMCPScriptHistory
{
	void Record(const FString& Code, bool bAccepted, const FString& Message)
	{
		FScriptHistoryEntry Entry;
		Entry.Timestamp = FDateTime::UtcNow();
		Entry.Code = Code;
		Entry.bAccepted = bAccepted;
		Entry.Message = Message;
		GScriptHistory.Add(MoveTemp(Entry));
		if (GScriptHistory.Num() > MaxScriptHistory)
		{
			GScriptHistory.RemoveAt(0, GScriptHistory.Num() - MaxScriptHistory);
		}
	}

	TArray<TSharedPtr<FJsonValue>> ToJson(int32 Limit)
	{
		TArray<TSharedPtr<FJsonValue>> Entries;
		const int32 ClampedLimit = FMath::Clamp(Limit, 1, MaxScriptHistory);
		const int32 StartIndex = FMath::Max(0, GScriptHistory.Num() - ClampedLimit);
		for (int32 Index = StartIndex; Index < GScriptHistory.Num(); ++Index)
		{
			const FScriptHistoryEntry& Entry = GScriptHistory[Index];
			TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetStringField(TEXT("timestamp"), Entry.Timestamp.ToIso8601());
			Json->SetStringField(TEXT("code"), Entry.Code);
			Json->SetBoolField(TEXT("accepted"), Entry.bAccepted);
			Json->SetStringField(TEXT("message"), Entry.Message);
			Entries.Add(MakeShared<FJsonValueObject>(Json));
		}
		return Entries;
	}

	void Clear()
	{
		GScriptHistory.Reset();
	}
}
