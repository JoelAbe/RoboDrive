#include "RoboAutonomy.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogRoboAutonomy);

class FRoboAutonomyModule : public IModuleInterface
{
	public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRoboAutonomy, Display, TEXT("RoboAutonomy Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogRoboAutonomy, Display, TEXT("RoboAutonomy Module Shutdown"));
	}
};

IMPLEMENT_MODULE(FRoboAutonomyModule, RoboAutonomy);