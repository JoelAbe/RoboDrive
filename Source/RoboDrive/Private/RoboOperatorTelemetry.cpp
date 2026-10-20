#include "RoboOperatorTelemetry.h"

#include "GameFramework/PlayerController.h"
#include "RoboAutonomyComponent.h"
#include "RoboLidarComponent.h"
#include "RoboOdometryComponent.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "RoboWeather.h"
#include "RoboScenario.h"
#include "RoboAvoidance.h"
#include "RoboFrames.h"

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

	if (const URoboScenarioSubsystem* Scenarios = URoboScenarioSubsystem::Get(Player))
	{
		Out.bHasScenario = Scenarios->HasScenario();
		Out.ScenarioName = Scenarios->GetScenarioName();
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
	
	const FVector ForwardRos = Vehicle->GetRotationRos().RotateVector(FVector::ForwardVector);
	Out.ActualLinearMS = FVector::DotProduct(Vehicle->GetLinearVelocityMS(), ForwardRos);
	Out.ActualYawRateRadS = Vehicle->GetAngularVelocityRadS().Z;

	Out.DriveMode = Vehicle->GetDriveMode();
	Out.bStopped = Vehicle->IsEmergencyStopped();

	Out.BrainName = TEXT("None");
	if (const URoboAutonomyComponent* Autonomy = Vehicle->FindComponentByClass<URoboAutonomyComponent>())
	{
		Out.BrainName = Autonomy->bEnabled
			? UEnum::GetDisplayValueAsText(Autonomy->BrainKind).ToString()
			: TEXT("Switched off");

		if (Autonomy->bEnabled)
		{
			const FBrainOutput& Last = Autonomy->GetLastOutput();
			Out.bHasBrainActivity = true;
			Out.BrainActivity = Last.Activity;
			Out.DeviationDeg = FMath::RadiansToDegrees(Last.DeviationRad);
			Out.BlockedForSeconds = Last.BlockedForSeconds;

			// The heading chosen, and the corridor checked along it. Only the
			// avoiding brain chooses a heading; go-to-goal leaves it unset.
			Out.ChosenHeadingRad = Last.ChosenHeadingRad;
			Out.CorridorLengthM = Autonomy->Avoidance.LookaheadM;
			Out.CorridorHalfWidthM = Autonomy->Avoidance.RobotHalfWidthM + Autonomy->Avoidance.MarginM;
		}
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
				
				// The goal's direction relative to where the robot believes it
				// is facing: the same bearing the brain steers by.
				Out.bHasGoalBearing = true;
				Out.GoalBearingRad = RoboFrames::WrapRadians(
					FMath::Atan2(Goal.Y - Believed.Y, Goal.X - Believed.X) - Believed.YawRad);
			}
		}
	}

	if (const URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>())
	{
		Out.bHasLidar = true;
		Out.LidarBeamCount = Lidar->BeamCount;
		Out.LidarRateHz = Lidar->ScanRateHz;
		Out.LidarRangeM = Lidar->RangeMaxM;

		// The scan turned into points with exactly the function avoidance
		// uses, so the view shows what the brain sees and nothing more.
		const FLidarScan& Scan = Lidar->GetLatestScan();
		Out.bHasScan = Scan.RangesM.Num() > 0;
		Out.ScanPointsM = RoboAvoidance::ScanToPoints(Scan);
		Out.ScanAngleMinRad = Scan.AngleMinRad;
		Out.ScanAngleMaxRad = Scan.AngleMinRad + Scan.AngleIncrementRad * FMath::Max(Scan.RangesM.Num() - 1, 0);
	}

	if (Odometry != nullptr)
	{
		Out.bHasOdometry = true;
		Out.OdometryErrorM = Odometry->GetPositionErrorM();
		Out.HeadingErrorDeg = FMath::RadiansToDegrees(Odometry->GetHeadingErrorRad());
	}

	return Out;
}