#include "RoboMission.h"

#include "RoboSim.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

URoboMissionSubsystem* URoboMissionSubsystem::Get(const UObject* WorldContext)
{
	if (GEngine == nullptr || WorldContext == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContext, EGetWorldErrorMode::ReturnNull);

	return World != nullptr ? World->GetSubsystem<URoboMissionSubsystem>() : nullptr;
}

bool URoboMissionSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URoboMissionSubsystem::SetGoal(const FMissionGoal& NewGoal)
{
	Goal = NewGoal;
	Status = EMissionStatus::Active;

	UE_LOG(LogRoboSim, Log, TEXT("Mission goal set to (%.2f, %.2f), tolerance %.2f m"),
		Goal.X, Goal.Y, Goal.ToleranceM);
}

void URoboMissionSubsystem::ClearGoal()
{
	Goal = FMissionGoal();
	Status = EMissionStatus::None;
}

void URoboMissionSubsystem::ReportStatus(EMissionStatus NewStatus)
{
	// Progress can only be reported against a goal. Without this check, a brain
	// reporting "arrived" a moment after the goal was withdrawn would bring the
	// withdrawn mission back to life. And None is not something a brain reports:
	// only withdrawing the goal produces it.
	if (Status == EMissionStatus::None || NewStatus == EMissionStatus::None)
	{
		return;
	}

	if (NewStatus != Status)
	{
		UE_LOG(LogRoboSim, Log, TEXT("Mission status: %s"),
			*UEnum::GetDisplayValueAsText(NewStatus).ToString());
		Status = NewStatus;
	}
}