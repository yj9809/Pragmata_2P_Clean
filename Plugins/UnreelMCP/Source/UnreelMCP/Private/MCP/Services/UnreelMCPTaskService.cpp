#include "MCP/Services/UnreelMCPTaskService.h"

#include "MCP/Services/UnreelMCPScriptHistory.h"
#include "MCP/UnreelMCPServiceRegistry.h"

namespace
{
	struct FStoredTask
	{
		FString Id;
		FString OperationId;
		FString Status;
		FDateTime SubmittedAt;
		FDateTime StartedAt;
		FDateTime CompletedAt;
		TSharedPtr<FJsonObject> Params;
		FUnreelMCPResult Result;
	};

	TMap<FString, FStoredTask> GTasks;
	TArray<FString> GTaskQueue;

	FUnreelMCPParameter TaskParam(const FString& Name, const FString& Type, const FString& Description, bool bRequired = false, const FString& DefaultValue = FString())
	{
		FUnreelMCPParameter Parameter;
		Parameter.Name = Name;
		Parameter.Type = Type;
		Parameter.Description = Description;
		Parameter.bRequired = bRequired;
		Parameter.DefaultValue = DefaultValue;
		return Parameter;
	}

	FUnreelMCPOperation TaskOp(const FString& Id, const FString& Domain, const FString& Operation, const FString& Description, const TArray<FUnreelMCPParameter>& Params, bool bReadOnly, EUnreelMCPRisk Risk, const FString& Example)
	{
		FUnreelMCPOperation Op;
		Op.Id = Id;
		Op.Domain = Domain;
		Op.Operation = Operation;
		Op.Description = Description;
		Op.Keywords = { Domain, Operation, TEXT("automation") };
		Op.Parameters = Params;
		Op.bReadOnly = bReadOnly;
		Op.Risk = Risk;
		Op.ExampleJson = Example;
		Op.SupportNotes = TEXT("Tasks are queued and drained on the editor thread; status/result/list/cancel expose stored task state.");
		return Op;
	}

	TSharedPtr<FJsonObject> ResultToJson(const FUnreelMCPResult& Result)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetBoolField(TEXT("success"), Result.bSuccess);
		Json->SetStringField(TEXT("message"), Result.Message);
		Json->SetObjectField(TEXT("data"), Result.Data.IsValid() ? Result.Data : MakeShared<FJsonObject>());

		TArray<TSharedPtr<FJsonValue>> Warnings;
		for (const FString& Warning : Result.Warnings)
		{
			Warnings.Add(MakeShared<FJsonValueString>(Warning));
		}
		Json->SetArrayField(TEXT("warnings"), Warnings);

		TArray<TSharedPtr<FJsonValue>> ChangedAssets;
		for (const FString& Asset : Result.ChangedAssets)
		{
			ChangedAssets.Add(MakeShared<FJsonValueString>(Asset));
		}
		Json->SetArrayField(TEXT("changed_assets"), ChangedAssets);
		return Json;
	}

	TSharedPtr<FJsonObject> TaskSummary(const FStoredTask& Task, bool bIncludeResult)
	{
		TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("task_id"), Task.Id);
		Json->SetStringField(TEXT("operation_id"), Task.OperationId);
		Json->SetStringField(TEXT("status"), Task.Status);
		Json->SetStringField(TEXT("submitted_at"), Task.SubmittedAt.ToIso8601());
		Json->SetStringField(TEXT("started_at"), Task.StartedAt.GetTicks() > 0 ? Task.StartedAt.ToIso8601() : FString());
		Json->SetStringField(TEXT("completed_at"), Task.CompletedAt.GetTicks() > 0 ? Task.CompletedAt.ToIso8601() : FString());
		if (bIncludeResult)
		{
			Json->SetObjectField(TEXT("result"), ResultToJson(Task.Result));
		}
		return Json;
	}
}

FUnreelMCPTaskService::FUnreelMCPTaskService(TSharedRef<FUnreelMCPServiceRegistry> InRegistry)
	: Registry(InRegistry)
{
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FUnreelMCPTaskService::Tick), 0.05f);
}

FUnreelMCPTaskService::~FUnreelMCPTaskService()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

FString FUnreelMCPTaskService::GetServiceName() const
{
	return TEXT("task");
}

TArray<FUnreelMCPOperation> FUnreelMCPTaskService::GetOperations() const
{
	return {
		TaskOp(TEXT("task.submit"), TEXT("task"), TEXT("submit"), TEXT("Queue an operation and store its result under a task id."), { TaskParam(TEXT("operation_id"), TEXT("string"), TEXT("Catalog operation id to execute."), true), TaskParam(TEXT("params"), TEXT("object"), TEXT("Operation parameters."), false, TEXT("{}")) }, false, EUnreelMCPRisk::Medium, TEXT("{\"operation_id\":\"task.submit\",\"params\":{\"operation_id\":\"core.ping\",\"params\":{}}}")),
		TaskOp(TEXT("task.status"), TEXT("task"), TEXT("status"), TEXT("Read a stored task status."), { TaskParam(TEXT("task_id"), TEXT("string"), TEXT("Task id returned by task.submit."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"task.status\",\"params\":{\"task_id\":\"task-1\"}}")),
		TaskOp(TEXT("task.result"), TEXT("task"), TEXT("result"), TEXT("Read a stored task result."), { TaskParam(TEXT("task_id"), TEXT("string"), TEXT("Task id returned by task.submit."), true) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"task.result\",\"params\":{\"task_id\":\"task-1\"}}")),
		TaskOp(TEXT("task.cancel"), TEXT("task"), TEXT("cancel"), TEXT("Cancel a task if it has not already completed."), { TaskParam(TEXT("task_id"), TEXT("string"), TEXT("Task id returned by task.submit."), true) }, false, EUnreelMCPRisk::Low, TEXT("{\"operation_id\":\"task.cancel\",\"params\":{\"task_id\":\"task-1\"}}")),
		TaskOp(TEXT("task.list"), TEXT("task"), TEXT("list"), TEXT("List recent stored tasks."), { TaskParam(TEXT("limit"), TEXT("integer"), TEXT("Maximum task count."), false, TEXT("50")) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"task.list\",\"params\":{\"limit\":20}}")),
		TaskOp(TEXT("script.history"), TEXT("script"), TEXT("history"), TEXT("Read recent python.execute attempts."), { TaskParam(TEXT("limit"), TEXT("integer"), TEXT("Maximum history count."), false, TEXT("50")) }, true, EUnreelMCPRisk::ReadOnly, TEXT("{\"operation_id\":\"script.history\",\"params\":{\"limit\":20}}")),
		TaskOp(TEXT("script.clear_history"), TEXT("script"), TEXT("clear_history"), TEXT("Clear in-memory script execution history."), {}, false, EUnreelMCPRisk::Low, TEXT("{\"operation_id\":\"script.clear_history\",\"params\":{}}"))
	};
}

FUnreelMCPResult FUnreelMCPTaskService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("task.submit")) return Submit(Params);
	if (OperationId == TEXT("task.status")) return Status(Params);
	if (OperationId == TEXT("task.result")) return Result(Params);
	if (OperationId == TEXT("task.cancel")) return Cancel(Params);
	if (OperationId == TEXT("task.list")) return List(Params);
	if (OperationId == TEXT("script.history")) return ScriptHistory(Params);
	if (OperationId == TEXT("script.clear_history")) return ClearScriptHistory();
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown task operation: %s"), *OperationId));
}

FUnreelMCPResult FUnreelMCPTaskService::Submit(const TSharedRef<FJsonObject>& Params)
{
	FString OperationId;
	if (!Params->TryGetStringField(TEXT("operation_id"), OperationId) || OperationId.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: operation_id"));
	}
	if (OperationId.StartsWith(TEXT("task.")))
	{
		return FUnreelMCPResult::Error(TEXT("task.submit cannot execute task.* operations"));
	}

	const TSharedPtr<FJsonObject>* OperationParamsField = nullptr;
	TSharedPtr<FJsonObject> OperationParams;
	if (Params->TryGetObjectField(TEXT("params"), OperationParamsField) && OperationParamsField)
	{
		OperationParams = *OperationParamsField;
	}
	if (!OperationParams.IsValid())
	{
		OperationParams = MakeShared<FJsonObject>();
	}

	FStoredTask Task;
	Task.Id = FString::Printf(TEXT("task-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Short));
	Task.OperationId = OperationId;
	Task.Status = TEXT("queued");
	Task.SubmittedAt = FDateTime::UtcNow();
	Task.Params = OperationParams;
	GTasks.Add(Task.Id, Task);
	GTaskQueue.Add(Task.Id);

	TSharedPtr<FJsonObject> Data = TaskSummary(Task, false);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Task %s queued"), *Task.Id), Data);
}

FUnreelMCPResult FUnreelMCPTaskService::Status(const TSharedRef<FJsonObject>& Params) const
{
	FString TaskId;
	if (!Params->TryGetStringField(TEXT("task_id"), TaskId) || TaskId.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: task_id"));
	}
	if (const FStoredTask* Task = GTasks.Find(TaskId))
	{
		if (Task->Status == TEXT("queued"))
		{
			ProcessQueuedTasks(1);
			Task = GTasks.Find(TaskId);
		}
		return FUnreelMCPResult::Success(TEXT("Task status loaded"), TaskSummary(*Task, false));
	}
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown task_id: %s"), *TaskId));
}

FUnreelMCPResult FUnreelMCPTaskService::Result(const TSharedRef<FJsonObject>& Params) const
{
	FString TaskId;
	if (!Params->TryGetStringField(TEXT("task_id"), TaskId) || TaskId.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: task_id"));
	}
	if (const FStoredTask* Task = GTasks.Find(TaskId))
	{
		if (Task->Status == TEXT("queued"))
		{
			ProcessQueuedTasks(1);
			Task = GTasks.Find(TaskId);
		}
		return FUnreelMCPResult::Success(TEXT("Task result loaded"), TaskSummary(*Task, true));
	}
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown task_id: %s"), *TaskId));
}

FUnreelMCPResult FUnreelMCPTaskService::Cancel(const TSharedRef<FJsonObject>& Params)
{
	FString TaskId;
	if (!Params->TryGetStringField(TEXT("task_id"), TaskId) || TaskId.IsEmpty())
	{
		return FUnreelMCPResult::Error(TEXT("Missing required parameter: task_id"));
	}
	if (FStoredTask* Task = GTasks.Find(TaskId))
	{
		if (Task->Status == TEXT("queued"))
		{
			Task->Status = TEXT("canceled");
			Task->CompletedAt = FDateTime::UtcNow();
			GTaskQueue.Remove(TaskId);
		}
		return FUnreelMCPResult::Success(TEXT("Task cancel state loaded"), TaskSummary(*Task, false));
	}
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown task_id: %s"), *TaskId));
}

FUnreelMCPResult FUnreelMCPTaskService::List(const TSharedRef<FJsonObject>& Params) const
{
	double LimitValue = 50;
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	const int32 Limit = FMath::Clamp(static_cast<int32>(LimitValue), 1, 200);
	TArray<TSharedPtr<FJsonValue>> Tasks;
	int32 Added = 0;
	for (const TPair<FString, FStoredTask>& Pair : GTasks)
	{
		if (Added++ >= Limit)
		{
			break;
		}
		Tasks.Add(MakeShared<FJsonValueObject>(TaskSummary(Pair.Value, false)));
	}
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("tasks"), Tasks);
	return FUnreelMCPResult::Success(FString::Printf(TEXT("Loaded %d task(s)"), Tasks.Num()), Data);
}

FUnreelMCPResult FUnreelMCPTaskService::ScriptHistory(const TSharedRef<FJsonObject>& Params) const
{
	double LimitValue = 50;
	Params->TryGetNumberField(TEXT("limit"), LimitValue);
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("entries"), UnreelMCPScriptHistory::ToJson(static_cast<int32>(LimitValue)));
	return FUnreelMCPResult::Success(TEXT("Script history loaded"), Data);
}

FUnreelMCPResult FUnreelMCPTaskService::ClearScriptHistory() const
{
	UnreelMCPScriptHistory::Clear();
	return FUnreelMCPResult::Success(TEXT("Script history cleared"));
}

bool FUnreelMCPTaskService::Tick(float DeltaTime)
{
	ProcessQueuedTasks(1);
	return true;
}

void FUnreelMCPTaskService::ProcessQueuedTasks(int32 MaxTasks) const
{
	for (int32 Processed = 0; Processed < MaxTasks && GTaskQueue.Num() > 0; ++Processed)
	{
		const FString TaskId = GTaskQueue[0];
		GTaskQueue.RemoveAt(0);
		FStoredTask* Task = GTasks.Find(TaskId);
		if (!Task || Task->Status != TEXT("queued"))
		{
			continue;
		}
		Task->Status = TEXT("running");
		Task->StartedAt = FDateTime::UtcNow();
		TSharedPtr<FJsonObject> OperationParams = Task->Params.IsValid() ? Task->Params : MakeShared<FJsonObject>();
		Task->Result = Registry->ExecuteOperation(Task->OperationId, OperationParams.ToSharedRef());
		Task->CompletedAt = FDateTime::UtcNow();
		Task->Status = Task->Result.bSuccess ? TEXT("completed") : TEXT("failed");
	}
}
