#pragma once

#include "CoreMinimal.h"
#include "RoboBrain.h"
#include "RoboGoToGoalBrain.generated.h"

/**
 * Tuning for the go-to-goal brain. A USTRUCT so a component can expose it for
 * editing during play.
 */
USTRUCT(BlueprintType)
struct FGoToGoalSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal",
		meta = (ClampMin = "0.0", Units = "m/s"))
	double MaxSpeedMS = 0.5;

	/** Fastest turn rate, rad/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal", meta = (ClampMin = "0.0"))
	double MaxTurnRadS = 1.0;

	/** Turn rate per radian of heading error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal", meta = (ClampMin = "0.0"))
	double HeadingGain = 1.5;

	/** Speed per metre of remaining distance, so the robot slows as it closes in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal", meta = (ClampMin = "0.0"))
	double SpeedGain = 0.5;

	/** Become arrived once within this fraction of the goal's tolerance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal",
		meta = (ClampMin = "0.05", ClampMax = "1.0"))
	double ArriveFraction = 0.5;

	/**
	 * Once arrived, stay arrived until beyond this fraction of the tolerance.
	 * The gap between the two fractions is what stops the robot chattering at
	 * the boundary.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Go To Goal",
		meta = (ClampMin = "0.05", ClampMax = "2.0"))
	double LeaveFraction = 1.0;
};

/**
 * Drives straight for the goal using odometry alone: turn to face it, drive
 * toward it, slow down on approach. Knows nothing about obstacles.
 */
class ROBOAUTONOMY_API FGoToGoalBrain : public FRoboBrain
{
public:
	explicit FGoToGoalBrain(const FGoToGoalSettings& InSettings = FGoToGoalSettings());

	virtual FBrainOutput Update(const FBrainInputs& Inputs) override;
	virtual void Reset() override;

	void SetSettings(const FGoToGoalSettings& InSettings) { Settings = InSettings; }
	
	/**
	 * Turn toward a heading and drive, slowing near the goal and when facing
	 * away from it. Shared with brains that choose a heading other than
	 * straight at the goal.
	 *
	 * @param HeadingErrorRad  Desired heading relative to the robot's own, rad.
	 * @param DistanceM        Distance remaining to the goal, m.
	 */
	static RoboDiffDrive::FBodyVelocity SteerTowards(double HeadingErrorRad, double DistanceM,
		const FGoToGoalSettings& Settings);

private:
	FGoToGoalSettings Settings;

	/** Whether the robot has reached the current goal. The one bit of memory hysteresis needs. */
	bool bArrived = false;

	/** The goal last seen, so a new goal clears the arrived flag. */
	FMissionGoal LastGoal;
	bool bHasLastGoal = false;
};