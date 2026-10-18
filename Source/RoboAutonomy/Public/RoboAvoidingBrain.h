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
};