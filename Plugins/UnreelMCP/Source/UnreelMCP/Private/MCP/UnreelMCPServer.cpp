#include "MCP/UnreelMCPServer.h"

#include "MCP/UnreelMCPJson.h"
#include "MCP/UnreelMCPServiceRegistry.h"
#include "MCP/UnreelMCPServiceUtils.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

FUnreelMCPServer::FUnreelMCPServer(TSharedRef<FUnreelMCPServiceRegistry> InRegistry)
	: Registry(InRegistry)
{
}

FUnreelMCPServer::~FUnreelMCPServer()
{
	Stop();
}

bool FUnreelMCPServer::Start(uint32 InPort)
{
	if (bRunning)
	{
		return true;
	}

	Port = InPort;
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(Port);
	if (!HttpRouter.IsValid())
	{
		return false;
	}

	SetupRoutes();
	HttpServerModule.StartAllListeners();
	bRunning = true;
	return true;
}

void FUnreelMCPServer::Stop()
{
	if (!bRunning || !HttpRouter.IsValid())
	{
		bRunning = false;
		return;
	}

	if (StatusHandle.IsSet()) HttpRouter->UnbindRoute(StatusHandle.GetValue());
	if (ToolsHandle.IsSet()) HttpRouter->UnbindRoute(ToolsHandle.GetValue());
	if (SearchHandle.IsSet()) HttpRouter->UnbindRoute(SearchHandle.GetValue());
	if (DescribeHandle.IsSet()) HttpRouter->UnbindRoute(DescribeHandle.GetValue());
	if (CallHandle.IsSet()) HttpRouter->UnbindRoute(CallHandle.GetValue());
	if (OperationHandle.IsSet()) HttpRouter->UnbindRoute(OperationHandle.GetValue());
	if (ContextHandle.IsSet()) HttpRouter->UnbindRoute(ContextHandle.GetValue());

	bRunning = false;
}

void FUnreelMCPServer::SetupRoutes()
{
	StatusHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/status")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleStatus));
	ToolsHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/tools")), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleTools));
	SearchHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/search")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleSearch));
	DescribeHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/describe")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleDescribe));
	CallHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/call")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleCall));
	OperationHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/operation")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleOperation));
	ContextHandle = HttpRouter->BindRoute(FHttpPath(TEXT("/mcp/context")), EHttpServerRequestVerbs::VERB_POST,
		FHttpRequestHandler::CreateRaw(this, &FUnreelMCPServer::HandleContext));
}

bool FUnreelMCPServer::HandleStatus(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("connected"), true);
	Json->SetStringField(TEXT("project"), FApp::GetProjectName());
	Json->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
	Json->SetNumberField(TEXT("port"), Port);
	Json->SetNumberField(TEXT("operation_count"), Registry->GetAllOperations().Num());
	Json->SetStringField(TEXT("plugin"), TEXT("UnreelMCP"));
	AddPythonStatus(Json);
	OnComplete(CreateJsonResponse(Json, EHttpServerResponseCodes::Ok));
	return true;
}

bool FUnreelMCPServer::HandleTools(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ToolValues;
	for (const FUnreelMCPOperation& Operation : Registry->GetAllOperations())
	{
		ToolValues.Add(MakeShared<FJsonValueObject>(UnreelMCPJson::OperationToJson(Operation)));
	}
	Json->SetArrayField(TEXT("tools"), ToolValues);
	OnComplete(CreateJsonResponse(Json, EHttpServerResponseCodes::Ok));
	return true;
}

bool FUnreelMCPServer::HandleSearch(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<FJsonObject> Body = ParseBodyOrRespond(Request, OnComplete);
	if (!Body.IsValid()) return true;

	FString Query;
	FString Domain;
	int32 Limit = 20;
	Body->TryGetStringField(TEXT("query"), Query);
	Body->TryGetStringField(TEXT("domain"), Domain);
	double LimitValue = 20.0;
	if (Body->TryGetNumberField(TEXT("limit"), LimitValue))
	{
		Limit = FMath::Max(1, FMath::RoundToInt(LimitValue));
	}

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ToolValues;
	for (const FUnreelMCPOperation& Operation : Registry->SearchOperations(Query, Domain, Limit))
	{
		ToolValues.Add(MakeShared<FJsonValueObject>(UnreelMCPJson::OperationToJson(Operation)));
	}
	Json->SetArrayField(TEXT("tools"), ToolValues);
	OnComplete(CreateJsonResponse(Json, EHttpServerResponseCodes::Ok));
	return true;
}

bool FUnreelMCPServer::HandleDescribe(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedPtr<FJsonObject> Body = ParseBodyOrRespond(Request, OnComplete);
	if (!Body.IsValid()) return true;

	FString OperationId;
	if (!Body->TryGetStringField(TEXT("operation_id"), OperationId) || OperationId.IsEmpty())
	{
		OnComplete(CreateErrorResponse(TEXT("Missing required field: operation_id"), EHttpServerResponseCodes::BadRequest));
		return true;
	}

	TOptional<FUnreelMCPOperation> Operation = Registry->FindOperation(OperationId);
	if (!Operation.IsSet())
	{
		OnComplete(CreateErrorResponse(FString::Printf(TEXT("Unknown operation: %s"), *OperationId), EHttpServerResponseCodes::NotFound));
		return true;
	}

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetObjectField(TEXT("tool"), UnreelMCPJson::OperationToJson(Operation.GetValue()));
	OnComplete(CreateJsonResponse(Json, EHttpServerResponseCodes::Ok));
	return true;
}

bool FUnreelMCPServer::HandleCall(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	return HandleCallInternal(Request, OnComplete, false);
}

bool FUnreelMCPServer::HandleOperation(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	return HandleCallInternal(Request, OnComplete, true);
}

bool FUnreelMCPServer::HandleCallInternal(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, bool bDeprecatedOperationRoute)
{
	TSharedPtr<FJsonObject> Body = ParseBodyOrRespond(Request, OnComplete);
	if (!Body.IsValid()) return true;

	FString OperationId;
	if (!Body->TryGetStringField(TEXT("operation_id"), OperationId) || OperationId.IsEmpty())
	{
		OnComplete(CreateErrorResponse(TEXT("Missing required field: operation_id"), EHttpServerResponseCodes::BadRequest));
		return true;
	}

	const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	if (Body->TryGetObjectField(TEXT("params"), ParamsObject) && ParamsObject && ParamsObject->IsValid())
	{
		Params = (*ParamsObject).ToSharedRef();
	}

	const FUnreelMCPResult Result = Registry->ExecuteOperation(OperationId, Params);
	const EHttpServerResponseCodes Code = Result.bSuccess ? EHttpServerResponseCodes::Ok : EHttpServerResponseCodes::BadRequest;
	TSharedRef<FJsonObject> Json = UnreelMCPJson::ResultToJson(Result).ToSharedRef();
	if (bDeprecatedOperationRoute && Result.bSuccess)
	{
		TSharedPtr<FJsonObject> DeprecatedRoute = MakeShared<FJsonObject>();
		DeprecatedRoute->SetStringField(TEXT("route"), TEXT("/mcp/operation"));
		DeprecatedRoute->SetStringField(TEXT("use"), TEXT("/mcp/call"));
		DeprecatedRoute->SetStringField(TEXT("message"), TEXT("/mcp/operation is a compatibility alias. Prefer POST /mcp/call."));
		Json->SetObjectField(TEXT("deprecated_route"), DeprecatedRoute);
	}
	OnComplete(CreateJsonResponse(Json, Code));
	return true;
}

bool FUnreelMCPServer::HandleContext(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("summary"), TEXT("UnreelMCP exposes Unreal Editor services through a searchable MCP catalog. Use search, describe, then call."));
	Json->SetStringField(TEXT("primary_client"), TEXT("Codex CLI"));
	Json->SetStringField(TEXT("engine_target"), TEXT("Unreal Engine 5.8 only"));
	AddEndpointContext(Json);
	AddPythonStatus(Json);
	OnComplete(CreateJsonResponse(Json, EHttpServerResponseCodes::Ok));
	return true;
}

void FUnreelMCPServer::AddPythonStatus(TSharedRef<FJsonObject> Json) const
{
	Json->SetObjectField(TEXT("python"), UnreelMCPServiceUtils::BuildPythonStatus());
}

void FUnreelMCPServer::AddEndpointContext(TSharedRef<FJsonObject> Json) const
{
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
	Json->SetArrayField(TEXT("raw_routes"), Routes);

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
	Json->SetArrayField(TEXT("bridge_tools"), BridgeTools);
	Json->SetStringField(TEXT("recommended_flow"), TEXT("unreal_status -> unreal_context -> unreal_tool_search or POST /mcp/search -> unreal_tool_describe or POST /mcp/describe -> unreal_call or POST /mcp/call"));
}

TSharedPtr<FJsonObject> FUnreelMCPServer::ParseBodyOrRespond(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString Error;
	TSharedPtr<FJsonObject> Body = UnreelMCPJson::ParseRequestBody(Request.Body, Error);
	if (!Body.IsValid())
	{
		OnComplete(CreateErrorResponse(Error, EHttpServerResponseCodes::BadRequest));
	}
	return Body;
}

TUniquePtr<FHttpServerResponse> FUnreelMCPServer::CreateJsonResponse(const TSharedRef<FJsonObject>& Json, EHttpServerResponseCodes Code) const
{
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(UnreelMCPJson::Stringify(Json), TEXT("application/json"));
	Response->Code = Code;
	Response->Headers.Add(TEXT("Access-Control-Allow-Origin"), { TEXT("http://localhost") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Methods"), { TEXT("GET, POST") });
	Response->Headers.Add(TEXT("Access-Control-Allow-Headers"), { TEXT("Content-Type") });
	return Response;
}

TUniquePtr<FHttpServerResponse> FUnreelMCPServer::CreateErrorResponse(const FString& Message, EHttpServerResponseCodes Code) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("success"), false);
	Json->SetStringField(TEXT("message"), Message);
	return CreateJsonResponse(Json, Code);
}
