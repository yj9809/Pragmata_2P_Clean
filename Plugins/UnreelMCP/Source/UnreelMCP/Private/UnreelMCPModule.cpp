#include "UnreelMCPModule.h"

#include "MCP/UnreelMCPServer.h"
#include "MCP/UnreelMCPServiceRegistry.h"
#include "MCP/Services/UnreelMCPBehaviorTreeService.h"
#include "MCP/Services/UnreelMCPCoreService.h"
#include "MCP/Services/UnreelMCPAuthoringService.h"
#include "MCP/Services/UnreelMCPBreadthService.h"
#include "MCP/Services/UnreelMCPEditorService.h"
#include "MCP/Services/UnreelMCPParityService.h"
#include "MCP/Services/UnreelMCPTaskService.h"
#include "MCP/Services/UnreelMCPTransactionService.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnreelMCP, Log, All);

void FUnreelMCPModule::StartupModule()
{
	Registry = MakeShared<FUnreelMCPServiceRegistry>();
	Registry->RegisterService(MakeShared<FUnreelMCPCoreService>());
	Registry->RegisterService(MakeShared<FUnreelMCPTransactionService>());
	Registry->RegisterService(MakeShared<FUnreelMCPEditorService>());
	Registry->RegisterService(MakeShared<FUnreelMCPBreadthService>());
	Registry->RegisterService(MakeShared<FUnreelMCPAuthoringService>());
	Registry->RegisterService(MakeShared<FUnreelMCPParityService>());
	Registry->RegisterService(MakeShared<FUnreelMCPBehaviorTreeService>());
	Registry->RegisterService(MakeShared<FUnreelMCPTaskService>(Registry.ToSharedRef()));

	Server = MakeShared<FUnreelMCPServer>(Registry.ToSharedRef());
	const bool bStarted = Server->Start();

	UE_LOG(LogUnreelMCP, Log, TEXT("UnreelMCP module started; MCP server %s on port %d"),
		bStarted ? TEXT("listening") : TEXT("failed to start"),
		Server->GetPort());
}

void FUnreelMCPModule::ShutdownModule()
{
	if (Server.IsValid())
	{
		Server->Stop();
		Server.Reset();
	}
	Registry.Reset();
	UE_LOG(LogUnreelMCP, Log, TEXT("UnreelMCP module stopped"));
}

IMPLEMENT_MODULE(FUnreelMCPModule, UnreelMCP)
