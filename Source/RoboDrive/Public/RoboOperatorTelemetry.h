#pragma once

#include "CoreMinimal.h"
#include "RoboBrain.h"
#include "RoboDriveCommand.h"
#include "RoboMission.h"

class APlayerController;

/**
 * Everything the operator view shows, gathered at one moment.
 *
 * Kept apart from the UI, so the question "what does the operator need to
 * know?" is answered in one place, in plain values, and the UI only decides
 * how they look. Each group has a flag, because a missing value (no robot, no
 * goal, no lidar) must show as missing rather than as a misleading zero.
 */
struct FRoboOperatorTelemetry
{
	// Simulation
	bool bHasSim = false;
	double SimTimeSeconds = 0.0;
	double RealTimeFactor = 0.0;

	// The scenario running, if any
	bool bHasScenario = false;
	FString ScenarioName;
	
	// Robot and driving
	bool bHasRobot = false;
	FString RobotName;
	EDriveSource DrivingSource = EDriveSource::None;
	double CommandLinearMS = 0.0;
	double CommandAngularRadS = 0.0;
	double ActualLinearMS = 0.0;
	double ActualYawRateRadS = 0.0;
	FString BrainName;
	
	// What the brain is doing, from its last decision
	bool bHasBrainActivity = false;
	EBrainActivity BrainActivity = EBrainActivity::Idle;
	double DeviationDeg = 0.0;
	double BlockedForSeconds = 0.0;

	// What the lidar sees and what the brain made of it, in the robot's own
	// frame (REP-103 metres and radians), for the lidar view
	bool bHasScan = false;
	TArray<FVector2D> ScanPointsM;
	double ScanAngleMinRad = 0.0;
	double ScanAngleMaxRad = 0.0;
	bool bHasGoalBearing = false;
	double GoalBearingRad = 0.0;
	TOptional<double> ChosenHeadingRad;
	double CorridorLengthM = 0.0;
	double CorridorHalfWidthM = 0.0;
	
	// The operator's choices
	EDriveMode DriveMode = EDriveMode::Auto;
	bool bStopped = false;

	// The mission, as the robot believes it is going
	EMissionStatus MissionStatus = EMissionStatus::None;
	bool bHasGoal = false;
	double GoalX = 0.0;
	double GoalY = 0.0;
	bool bHasBelievedDistance = false;
	double BelievedDistanceM = 0.0;

	// Sensing
	bool bHasLidar = false;
	int32 LidarBeamCount = 0;
	double LidarRateHz = 0.0;
	double LidarRangeM = 0.0;
	bool bHasFog = false;
	double VisibilityM = 0.0;

	// Ground truth: for the operator, never for the robot
	bool bHasOdometry = false;
	double OdometryErrorM = 0.0;
	double HeadingErrorDeg = 0.0;

	/** Read the current state of the robot a player controls. Safe with a null player. */
	static FRoboOperatorTelemetry Gather(const APlayerController* Player);
};