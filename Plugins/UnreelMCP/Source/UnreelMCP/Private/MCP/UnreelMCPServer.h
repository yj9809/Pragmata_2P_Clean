#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "IHttpRouter.h"

struct FHttpServerResponse;
struct FHttpServerRequest;
class FUnreelMCPServiceRegistry;
enum class EHttpServerResponseCodes : uint16;

class FUnreelMCPServer
{
public:
	explicit FUnreelMCPServer(TSharedRef<FUnreelMCPServiceRegistry> InRegistry);
	~FUnreelMCPServer();

	bool Start(uint32 InPort = 55570);
	void Stop();
	uint32 GetPort() const { return Port; }

private:
	void SetupRoutes();
	bool HandleStatus(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleTools(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleSearch(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleDescribe(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleCall(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleOperation(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleContext(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

	bool HandleCallInternal(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete, bool bDeprecatedOperationRoute);
	void AddPythonStatus(TSharedRef<FJsonObject> Json) const;
	void AddEndpointContext(TSharedRef<FJsonObject> Json) const;
	TSharedPtr<FJsonObject> ParseBodyOrRespond(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	TUniquePtr<FHttpServerResponse> CreateJsonResponse(const TSharedRef<FJsonObject>& Json, EHttpServerResponseCodes Code) const;
	TUniquePtr<FHttpServerResponse> CreateErrorResponse(const FString& Message, EHttpServerResponseCodes Code) const;

	TSharedRef<FUnreelMCPServiceRegistry> Registry;
	TSharedPtr<IHttpRouter> HttpRouter;
	TOptional<FHttpRouteHandle> StatusHandle;
	TOptional<FHttpRouteHandle> ToolsHandle;
	TOptional<FHttpRouteHandle> SearchHandle;
	TOptional<FHttpRouteHandle> DescribeHandle;
	TOptional<FHttpRouteHandle> CallHandle;
	TOptional<FHttpRouteHandle> OperationHandle;
	TOptional<FHttpRouteHandle> ContextHandle;
	uint32 Port = 55570;
	bool bRunning = false;
};
