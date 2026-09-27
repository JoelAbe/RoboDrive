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

	// Real-time factor: how much simulated time we managed per real second.
	// This is the number that will tell us what the lidar costs in 2e.
	WindowRealSeconds += DeltaTime;
	WindowSimSeconds += StepsTaken * Clock.GetStepSeconds();

	if (WindowRealSeconds >= RealTimeFactorWindowSeconds)
	{
		RealTimeFactor = WindowSimSeconds / WindowRealSeconds;
		WindowRealSeconds = 0.0;
		WindowSimSeconds = 0.0;
	}
}