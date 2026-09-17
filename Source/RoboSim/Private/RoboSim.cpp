#include "RoboSim.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogRoboSim);

class FRoboSimModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		UE_LOG(LogRoboSim, Log, TEXT("RoboSim Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogRoboSim, Log, TEXT("RoboSim Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FRoboSimModule, RoboSim)