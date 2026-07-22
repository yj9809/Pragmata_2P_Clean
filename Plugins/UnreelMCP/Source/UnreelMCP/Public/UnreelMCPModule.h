#pragma once

#include "Modules/ModuleManager.h"

class FUnreelMCPServer;
class FUnreelMCPServiceRegistry;

class FUnreelMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedPtr<FUnreelMCPServiceRegistry> Registry;
	TSharedPtr<FUnreelMCPServer> Server;
};
