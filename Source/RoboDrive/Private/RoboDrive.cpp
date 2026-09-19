#include "RoboDrive.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogRoboDrive);

class FRoboDriveModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogRoboDrive, Log, TEXT("RoboDrive Module Started"));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogRoboDrive, Log, TEXT("RoboDrive Module Shutdown"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE( FRoboDriveModule, RoboDrive, "RoboDrive" );
