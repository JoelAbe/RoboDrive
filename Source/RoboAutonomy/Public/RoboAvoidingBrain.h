#pragma once

#include "CoreMinimal.h"
#include "RoboAvoidance.h"
#include "RoboBrain.h"
#include "RoboGoToGoalBrain.h"

/**
 * Go-to-goal with lidar obstacle avoidance.
 *
 * Contains a go-to-goal brain rather than inheriting from one. Go-to-goal still
 * decides whether there is a goal and whether the robot has arrived, with its
 * hysteresis; avoidance only changes which way the robot steers while it
 * travels. Go-to-goal never needs to know avoidance exists.
 *
 * Avoids only returns that persist from scan to scan, which filters out lidar
 * clutter such as fog echoes while keeping every real obstacle.
 */
class ROBOAUTONOMY_API FAvoidingBrain : public FRoboBrain
{
public:
	FAvoidingBrain(const FGoToGoalSettings& InGoToGoalSettings, const FAvoidanceSettings& InAvoidanceSettings);

	virtual FBrainOutput Update(const FBrainInputs& Inputs) override;
	virtual void Reset() override;

private:
	FGoToGoalBrain GoToGoal;
	FGoToGoalSettings GoToGoalSettings;
	FAvoidanceSettings AvoidanceSettings;

	/** When the robot became boxed in, sim seconds. Unset while it has a way forward. */
	TOptional<double> BlockedSinceSeconds;

	/** The latest scan's returns, in the odometry frame, and when it was taken. */
	TArray<FVector2D> CurrentScanOdom;
	TOptional<double> CurrentScanTimeSeconds;

	/** The scans before it, oldest first, also in the odometry frame. */
	TArray<TArray<FVector2D>> EarlierScansOdom;
};