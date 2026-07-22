#pragma once

#include "CoreMinimal.h"
#include "MCP/UnreelMCPTypes.h"
#include "Containers/Ticker.h"

class FUnreelMCPServiceRegistry;

class FUnreelMCPTaskService : public IUnreelMCPService
{
public:
	explicit FUnreelMCPTaskService(TSharedRef<FUnreelMCPServiceRegistry> InRegistry);
	virtual ~FUnreelMCPTaskService() override;

	virtual FString GetServiceName() const override;
	virtual TArray<FUnreelMCPOperation> GetOperations() const override;
	virtual FUnreelMCPResult Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params) override;

private:
	FUnreelMCPResult Submit(const TSharedRef<FJsonObject>& Params);
	FUnreelMCPResult Status(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult Result(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult Cancel(const TSharedRef<FJsonObject>& Params);
	FUnreelMCPResult List(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ScriptHistory(const TSharedRef<FJsonObject>& Params) const;
	FUnreelMCPResult ClearScriptHistory() const;
	bool Tick(float DeltaTime);
	void ProcessQueuedTasks(int32 MaxTasks) const;

	TSharedRef<FUnreelMCPServiceRegistry> Registry;
	FTSTicker::FDelegateHandle TickHandle;
};
