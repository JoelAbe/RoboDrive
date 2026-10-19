#include "RoboOperatorTelemetry.h"

#include "GameFramework/PlayerController.h"
#include "RoboAutonomyComponent.h"
#include "RoboLidarComponent.h"
#include "RoboOdometryComponent.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "RoboWeather.h"

FRoboOperatorTelemetry FRoboOperatorTelemetry::Gather(const APlayerController* Player)
{
	FRoboOperatorTelemetry Out;

	if (Player == nullptr)
	{
		return Out;
	}

	if (const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(Player))
	{
		Out.bHasSim = true;
		Out.SimTimeSeconds = Sim->GetSimTimeSeconds();
		Out.RealTimeFactor = Sim->GetRealTimeFactor();
	}

	if (const URoboWeatherSubsystem* Weather = URoboWeatherSubsystem::Get(Player))
	{
		Out.bHasFog = Weather->HasFog();
		Out.VisibilityM = Weather->GetVisibilityM();
	}

	// The robot is whatever this player is controlling.
	const ARoboVehicle* Vehicle = Cast<ARoboVehicle>(Player->GetPawn());
	if (Vehicle == nullptr)
	{
		return Out;
	}

	Out.bHasRobot = true;
	Out.RobotName = Vehicle->GetActorNameOrLabel();
	Out.DrivingSource = Vehicle->GetActiveDriveSource();

	const FDriveCommand Command = Vehicle->GetActiveCommand();
	Out.CommandLinearMS = Command.Linear;
	Out.CommandAngularRadS = Command.Angular;
	Out.ActualSpeedMS = Vehicle->GetSpeedMS();

	Out.BrainName = TEXT("None");
	if (const URoboAutonomyComponent* Autonomy = Vehicle->FindComponentByClass<URoboAutonomyComponent>())
	{
		Out.BrainName = Autonomy->bEnabled
			? UEnum::GetDisplayValueAsText(Autonomy->BrainKind).ToString()
			: TEXT("Switched off");
	}

	const URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>();

	if (const URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(Player))
	{
		Out.MissionStatus = Mission->GetStatus();
		Out.bHasGoal = Mission->HasGoal();

		if (Out.bHasGoal)
		{
			const FMissionGoal Goal = Mission->GetGoal();
			Out.GoalX = Goal.X;
			Out.GoalY = Goal.Y;

			// How far the robot believes it has left, from its own odometry.
			// Whether that belief is right is what the ground-truth section shows.
			if (Odometry != nullptr)
			{
				const FPose2D Believed = Odometry->GetEstimatedPose();
				Out.bHasBelievedDistance = true;
				Out.BelievedDistanceM = FMath::Sqrt(
					FMath::Square(Goal.X - Believed.X) + FMath::Square(Goal.Y - Believed.Y));
			}
		}
	}

	if (const URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>())
	{
		Out.bHasLidar = true;
		Out.LidarBeamCount = Lidar->BeamCount;
		Out.LidarRateHz = Lidar->ScanRateHz;
		Out.LidarRangeM = Lidar->RangeMaxM;
	}

	if (Odometry != nullptr)
	{
		Out.bHasOdometry = true;
		Out.OdometryErrorM = Odometry->GetPositionErrorM();
		Out.HeadingErrorDeg = FMath::RadiansToDegrees(Odometry->GetHeadingErrorRad());
	}

	return Out;
}