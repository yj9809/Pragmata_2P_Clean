#include "MCP/Services/UnreelMCPCoreService.h"

#include "MCP/UnreelMCPServiceUtils.h"
#include "Dom/JsonValue.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

FString FUnreelMCPCoreService::GetServiceName() const
{
	return TEXT("core");
}

TArray<FUnreelMCPOperation> FUnreelMCPCoreService::GetOperations() const
{
	return {
		FUnreelMCPOperation{
			TEXT("core.ping"),
			TEXT("core"),
			TEXT("ping"),
			TEXT("Verify that UnreelMCP is loaded and callable inside Unreal Editor."),
			{ TEXT("status"), TEXT("health"), TEXT("connection") },
			{},
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"core.ping\",\"params\":{}}")
		},
		FUnreelMCPOperation{
			TEXT("core.context"),
			TEXT("core"),
			TEXT("context"),
			TEXT("Return concise project and plugin context for an MCP client."),
			{ TEXT("context"), TEXT("project"), TEXT("engine"), TEXT("plugin") },
			{},
			true,
			false,
			EUnreelMCPRisk::ReadOnly,
			TEXT("{\"operation_id\":\"core.context\",\"params\":{}}")
		}
	};
}

FUnreelMCPResult FUnreelMCPCoreService::Execute(const FString& OperationId, const TSharedRef<FJsonObject>& Params)
{
	if (OperationId == TEXT("core.ping"))
	{
		return ExecutePing();
	}
	if (OperationId == TEXT("core.context"))
	{
		return ExecuteContext();
	}
	return FUnreelMCPResult::Error(FString::Printf(TEXT("Unknown core operation: %s"), *OperationId));
}

FUnreelMCPResult FUnreelMCPCoreService::ExecutePing() const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("plugin"), TEXT("UnreelMCP"));
	Data->SetStringField(TEXT("project"), FApp::GetProjectName());
	Data->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	return FUnreelMCPResult::Success(TEXT("UnreelMCP is available"), Data);
}

FUnreelMCPResult FUnreelMCPCoreService::ExecuteContext() const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("summary"), TEXT("UnreelMCP is a UE 5.8 MCP tool host optimized for Codex CLI and searchable service calls."));
	Data->SetStringField(TEXT("primary_client"), TEXT("Codex CLI"));
	Data->SetStringField(TEXT("project"), FApp::GetProjectName());
	Data->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	Data->SetObjectField(TEXT("python"), UnreelMCPServiceUtils::BuildPythonStatus());
	const TArray<FString> RouteNames = {
		TEXT("GET /mcp/status"),
		TEXT("GET /mcp/tools"),
		TEXT("POST /mcp/search"),
		TEXT("POST /mcp/describe"),
		TEXT("POST /mcp/call"),
		TEXT("POST /mcp/context"),
		TEXT("POST /mcp/operation (deprecated alias for /mcp/call)")
	};
	TArray<TSharedPtr<FJsonValue>> Routes;
	for (const FString& Route : RouteNames)
	{
		Routes.Add(MakeShared<FJsonValueString>(Route));
	}
	Data->SetArrayField(TEXT("raw_routes"), Routes);
	const TArray<FString> BridgeToolNames = {
		TEXT("unreal_status"),
		TEXT("unreal_context"),
		TEXT("unreal_tool_search"),
		TEXT("unreal_tool_describe"),
		TEXT("unreal_call"),
		TEXT("unreal_transaction")
	};
	TArray<TSharedPtr<FJsonValue>> BridgeTools;
	for (const FString& Tool : BridgeToolNames)
	{
		BridgeTools.Add(MakeShared<FJsonValueString>(Tool));
	}
	Data->SetArrayField(TEXT("bridge_tools"), BridgeTools);
	Data->SetStringField(TEXT("recommended_flow"), TEXT("unreal_status -> unreal_context -> search/describe -> unreal_call"));
	return FUnreelMCPResult::Success(TEXT("Context loaded"), Data);
}
