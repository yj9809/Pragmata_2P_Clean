#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EUnreelMCPRisk : uint8
{
	ReadOnly,
	Low,
	Medium,
	High
};

struct FUnreelMCPParameter
{
	FString Name;
	FString Type;
	FString Description;
	bool bRequired = false;
	FString DefaultValue;
};

struct FUnreelMCPOperation
{
	FString Id;
	FString Domain;
	FString Operation;
	FString Description;
	TArray<FString> Keywords;
	TArray<FUnreelMCPParameter> Parameters;
	bool bReadOnly = true;
	bool bRequiresTransaction = false;
	EUnreelMCPRisk Risk = EUnreelMCPRisk::ReadOnly;
	FString ExampleJson;
	TArray<FString> SupportedEngines = { TEXT("5.8") };
	FString SupportNotes;
};

struct FUnreelMCPResult
{
	bool bSuccess = false;
	FString Message;
	TSharedPtr<FJsonObject> Data;
	TArray<FString> Warnings;
	TArray<FString> ChangedAssets;

	static FUnreelMCPResult Success(const FString& Message, TSharedPtr<FJsonObject> Data = nullptr)
	{
		FUnreelMCPResult Result;
		Result.bSuccess = true;
		Result.Message = Message;
		Result.Data = Data.IsValid() ? Data : MakeShared<FJsonObject>();
		return Result;
	}

	static FUnreelMCPResult Error(const FString& Message)
	{
		FUnreelMCPResult Result;
		Result.bSuccess = false;
		Result.Message = Message;
		Result.Data = MakeShared<FJsonObject>();
		return Result;
	}
};

class IUnreelMCPService
{
public:
	virtual ~IUnreelMCPService() = default;
	virtual FString GetServiceName() const = 0;
	virtual TArray<FUnreelMCPOperation> GetOperations() const = 0;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) = 0;
};
