#include "RoboWeather.h"

#include "RoboFog.h"
#include "RoboSim.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

URoboWeatherSubsystem* URoboWeatherSubsystem::Get(const UObject* WorldContext)
{
	if (GEngine == nullptr || WorldContext == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContext, EGetWorldErrorMode::ReturnNull);

	return World != nullptr ? World->GetSubsystem<URoboWeatherSubsystem>() : nullptr;
}

bool URoboWeatherSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URoboWeatherSubsystem::SetFog(double InVisibilityM)
{
	// Zero or negative visibility has no physical meaning. Refused here as well
	// as at the API, so a Blueprint cannot slip one past.
	if (InVisibilityM <= 0.0)
	{
		UE_LOG(LogRoboSim, Warning, TEXT("Ignoring fog with visibility %.2f m: it must be positive"),
			InVisibilityM);
		return;
	}

	bHasFog = true;
	VisibilityM = InVisibilityM;

	UE_LOG(LogRoboSim, Log, TEXT("Weather: fog, visibility %.1f m"), VisibilityM);
}

void URoboWeatherSubsystem::SetClear()
{
	if (bHasFog)
	{
		UE_LOG(LogRoboSim, Log, TEXT("Weather: clear"));
	}

	bHasFog = false;
	VisibilityM = 0.0;
}

double URoboWeatherSubsystem::GetExtinctionPerM() const
{
	return bHasFog ? RoboFog::ExtinctionPerM(VisibilityM) : 0.0;
}