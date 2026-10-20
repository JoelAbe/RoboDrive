#include "RoboAutonomyComponent.h"

#include "RoboAutonomy.h"
#include "RoboAvoidingBrain.h"
#include "RoboFrames.h"
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
	RebuildBrain();

	// The vehicle announces resets rather than calling the brain itself,
	// because RoboSim cannot know RoboAutonomy exists.
	RobotResetHandle = Vehicle->OnRobotReset().AddUObject(this, &URoboAutonomyComponent::HandleRobotReset);

	UE_LOG(LogRoboAutonomy, Log, TEXT("Autonomy running on %s at %.0f Hz"),
		*Vehicle->GetName(), UpdateRateHz);
}

void URoboAutonomyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Vehicle != nullptr)
	{
		Vehicle->OnRobotReset().Remove(RobotResetHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void URoboAutonomyComponent::HandleRobotReset()
{
	// Without this, memory from the old situation carries over: a boxed-in
	// timer that started before a restart would count against the new start.
	if (Brain.IsValid())
	{
		Brain->Reset();
	}
	LastOutput = FBrainOutput();
}

TUniquePtr<FRoboBrain> URoboAutonomyComponent::MakeBrain() const
{
	switch (BrainKind)
	{
	case EBrainKind::GoToGoal:
		return MakeUnique<FGoToGoalBrain>(GoToGoal);

	case EBrainKind::Avoiding:
	default:
		return MakeUnique<FAvoidingBrain>(GoToGoal, Avoidance);
	}
}

void URoboAutonomyComponent::RebuildBrain()
{
	Brain = MakeBrain();
	BuiltKind = BrainKind;
	BuiltGoToGoal = GoToGoal;
	BuiltAvoidance = Avoidance;
}

bool URoboAutonomyComponent::IsBrainStale() const
{
	// Reflection compares every field of each settings struct, so a setting
	// added later is covered without touching this function.
	return BrainKind != BuiltKind
		|| !FGoToGoalSettings::StaticStruct()->CompareScriptStruct(&GoToGoal, &BuiltGoToGoal, PPF_None)
		|| !FAvoidanceSettings::StaticStruct()->CompareScriptStruct(&Avoidance, &BuiltAvoidance, PPF_None);
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
		// Any setting may change during play, from the Details panel, a
		// Blueprint or code. Checked on every decision so all three are caught.
		if (IsBrainStale())
		{
			RebuildBrain();
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

	if (bDrawChosenHeading && LastOutput.ChosenHeadingRad.IsSet())
	{
		// The heading is relative to the robot, in REP-103. Convert it into
		// the robot's local Unreal frame, then rotate it into the world by the
		// robot's transform. Using the true transform here is fine: this is
		// drawing for a person watching, never something the brain sees.
		const double HeadingRad = LastOutput.ChosenHeadingRad.GetValue();
		const FVector LocalDirection = RoboFrames::PositionToUe(
			FVector(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad), 0.0)).GetSafeNormal();
		const FVector WorldDirection = Vehicle->GetActorQuat().RotateVector(LocalDirection);
		const FVector Start = Vehicle->GetActorLocation() + FVector(0.0, 0.0, 30.0);

		DrawDebugDirectionalArrow(GetWorld(), Start, Start + WorldDirection * 150.0,
			25.0f, FColor::Purple, false, 0.0f, 0, 3.0f);
	}
}