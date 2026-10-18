#pragma once

#include "CoreMinimal.h"
#include "RoboDiffDrive.h"
#include "RoboLidar.h"
#include "RoboMission.h"
#include "RoboOdometry.h"

/**
 * Everything a brain is allowed to know, captured at one moment.
 *
 * A brain is handed this snapshot and never the robot itself. With no pointer
 * to the vehicle it has no way to read ground truth, so it cannot cheat, even
 * by accident. This is the in-engine version of the split between get_sensors
 * and get_ground_truth in the external API.
 *
 * Held by value, so the data cannot change underneath a brain while it is
 * deciding.
 */
struct FBrainInputs
{
	double SimTimeSeconds = 0.0;

	/** Where the robot believes it is, from wheel odometry. */
	FPose2D Odometry;

	/** The latest lidar sweep, if there has been one. */
	FLidarScan Scan;
	bool bHasScan = false;

	/** Where the robot has been asked to go, if anywhere. */
	FMissionGoal Goal;
	bool bHasGoal = false;
};

/** What a brain decided. */
struct FBrainOutput
{
	/** Zero by default, which is a stop. */
	RoboDiffDrive::FBodyVelocity Command;

	/** How the brain thinks the mission is going, reported to the mission subsystem. */
	EMissionStatus Status = EMissionStatus::None;

	/**
	 * The direction the brain chose to steer, relative to the robot, rad. For
	 * drawing only: it lets a person watching see what the brain decided,
	 * rather than inferring it from how the wheels moved.
	 */
	TOptional<double> ChosenHeadingRad;
};

/**
 * Anything that can drive the robot from its sensors: go-to-goal now, obstacle
 * avoidance next.
 *
 * Plain C++ rather than a UObject, because a brain is pure decision logic.
 * Tests can construct one, hand it a made-up snapshot and check what it decides,
 * with no world, actor or engine running.
 */
class FRoboBrain
{
public:
	virtual ~FRoboBrain() = default;

	/** Decide what to do now. Called at a fixed rate on the simulation clock. */
	virtual FBrainOutput Update(const FBrainInputs& Inputs) = 0;

	/** Forget anything remembered from earlier updates, as when the robot is reset. */
	virtual void Reset() {}
};