#include "RoboAutonomyComponent.h"

#include "RoboAutonomy.h"
#include "RoboLidarComponent.h"
#include "RoboMission.h"
#include "RoboOdometryComponent.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

URoboAutonomyComponent::URoboAutonomyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboAutonomyComponent::BeginPlay()
{
	Super::BeginPlay();

	Vehicle = Cast<ARoboVehicle>(GetOwner());
	if (Vehicle == nullptr)
	{
		UE_LOG(LogRoboAutonomy, Warning,
			TEXT("%s is attached to %s, which is not a RoboVehicle. It will not drive anything."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>();
	Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>();
	Brain = MakeBrain();
	BrainSettings = GoToGoal;

	UE_LOG(LogRoboAutonomy, Log, TEXT("Autonomy running on %s at %.0f Hz"),
		*Vehicle->GetName(), UpdateRateHz);
}

TUniquePtr<FRoboBrain> URoboAutonomyComponent::MakeBrain() const
{
	return MakeUnique<FGoToGoalBrain>(GoToGoal);
}

FBrainInputs URoboAutonomyComponent::BuildInputs(double NowSeconds) const
{
	FBrainInputs Inputs;
	Inputs.SimTimeSeconds = NowSeconds;

	// The estimate, never the truth. This line is the one ADR 0009 depends on.
	if (Odometry != nullptr)
	{
		Inputs.Odometry = Odometry->GetEstimatedPose();
	}

	if (Lidar != nullptr)
	{
		Inputs.Scan = Lidar->GetLatestScan();
		Inputs.bHasScan = Inputs.Scan.RangesM.Num() > 0;
	}

	if (const URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this))
	{
		Inputs.bHasGoal = Mission->HasGoal();
		if (Inputs.bHasGoal)
		{
			Inputs.Goal = Mission->GetGoal();
		}
	}

	return Inputs;
}

void URoboAutonomyComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (!bEnabled || Vehicle == nullptr || Sim == nullptr || !Brain.IsValid())
	{
		// Sending nothing is deliberate: the last command expires and the
		// watchdog stops the robot.
		return;
	}

	const double NowSeconds = Sim->GetSimTimeSeconds();

	if (NowSeconds >= NextUpdateTimeSeconds)
	{
		// Settings can change during play from the Details panel, a Blueprint
		// or code. Comparing on every decision catches all three, where
		// PostEditChangeProperty would only see the first. The comparison uses
		// reflection, so it covers every UPROPERTY in the struct: a setting added
		// later cannot be silently missed.
		if (!FGoToGoalSettings::StaticStruct()->CompareScriptStruct(&GoToGoal, &BrainSettings, PPF_None))
		{
			Brain = MakeBrain();
			BrainSettings = GoToGoal;
		}
		
		NextUpdateTimeSeconds = NowSeconds + 1.0 / UpdateRateHz;

		LastOutput = Brain->Update(BuildInputs(NowSeconds));

		// A command every decision, including a stop when there is no goal: an
		// idle robot should stay put, and Autonomy is the lowest priority, so
		// holding still never blocks the keyboard or an external client.
		FDriveCommand Command;
		Command.Linear = LastOutput.Command.Linear;
		Command.Angular = LastOutput.Command.Angular;
		Command.TimeSeconds = NowSeconds;
		Command.Source = EDriveSource::Autonomy;
		Vehicle->SubmitDriveCommand(Command);

		// Safe to call unconditionally: the mission ignores a report of None,
		// which is what the brain returns when there is no goal.
		if (URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this))
		{
			Mission->ReportStatus(LastOutput.Status);
		}
	}

	if (bDrawReadout)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 145.0),
			FString::Printf(TEXT("brain %s  v=%.2f  w=%.2f"),
				*UEnum::GetDisplayValueAsText(LastOutput.Status).ToString(),
				LastOutput.Command.Linear, LastOutput.Command.Angular),
			Vehicle, FColor::Green, 0.0f, true);
	}
}