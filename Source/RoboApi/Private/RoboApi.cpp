#include "RoboApi.h"

DEFINE_LOG_CATEGORY(LogRoboApi);

void FRoboApiModule::StartupModule()
{
	UE_LOG(LogRoboApi, Log, TEXT("RoboApi module started"));
}

void FRoboApiModule::ShutdownModule()
{
	UE_LOG(LogRoboApi, Log, TEXT("RoboApi module shut down"));
}
    
IMPLEMENT_MODULE(FRoboApiModule, RoboApi)