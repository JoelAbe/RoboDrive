#include "RoboSimSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

URoboSimSubsystem* URoboSimSubsystem::Get(const UObject* WorldContext)
{
	if (GEngine == nullptr || WorldContext == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);

	return World != nullptr ? World->GetSubsystem<URoboSimSubsystem>() : nullptr;
}

bool URoboSimSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Only while actually simulating. An editor preview world has no robot to
	// run and should not be advancing a clock.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URoboSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoboSimSubsystem, STATGROUP_Tickables);
}

void URoboSimSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const int32 StepsTaken = Clock.Advance(DeltaTime);

	// Real-time factor: simulated seconds per wall-clock second. Measured with
	// the wall clock, not DeltaTime. In fixed-time-step mode DeltaTime is
	// always exactly one step however long the frame really took, so a factor
	// built from it would read 1 at any speed.
	const double NowWallSeconds = FPlatformTime::Seconds();
	if (WindowStartWallSeconds < 0.0)
	{
		WindowStartWallSeconds = NowWallSeconds;
	}

	WindowSimSeconds += StepsTaken * Clock.GetStepSeconds();

	const double WindowWallSeconds = NowWallSeconds - WindowStartWallSeconds;
	if (WindowWallSeconds >= RealTimeFactorWindowSeconds)
	{
		RealTimeFactor = WindowSimSeconds / WindowWallSeconds;
		WindowStartWallSeconds = NowWallSeconds;
		WindowSimSeconds = 0.0;
	}
}
}