#pragma once

#include "CoreMinimal.h"
#include "RoboLidar.h"
#include "RoboAvoidance.generated.h"

/** Tuning for lidar obstacle avoidance. A USTRUCT so a component can expose it. */
USTRUCT(BlueprintType)
struct FAvoidanceSettings
{
	GENERATED_BODY()

	/**
	 * How far ahead a heading must be clear, m. Longer sees obstacles sooner
	 * but blocks more headings in tight spaces.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance",
		meta = (ClampMin = "0.1", Units = "m"))
	double LookaheadM = 1.5;

	/** Half the robot's width, m. It is 0.6 m across the wheels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance",
		meta = (ClampMin = "0.0", Units = "m"))
	double RobotHalfWidthM = 0.3;

	/** Extra clearance either side of the robot, m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance",
		meta = (ClampMin = "0.0", Units = "m"))
	double MarginM = 0.15;

	/** Spacing of the headings tried, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance",
		meta = (ClampMin = "0.5", ClampMax = "45.0"))
	double CandidateStepDeg = 5.0;

	/** How long to stay boxed in before giving up and reporting the mission failed, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Avoidance",
		meta = (ClampMin = "0.5", Units = "s"))
	double BlockedTimeoutS = 5.0;
};

/**
 * Choosing a direction the robot can actually drive in, from a lidar scan.
 *
 * A simplified Vector Field Histogram: each candidate heading is tested by
 * laying a corridor the width of the robot along it and checking whether any
 * scan point falls inside. The clear heading nearest the goal wins.
 *
 * Everything here is in the lidar's frame. The lidar is mounted at the robot's
 * centre facing forward, so that is also the robot's frame. A robot with an
 * offset sensor would transform the points first, which is the job tf2 does
 * in ROS.
 */
namespace RoboAvoidance
{
	/** Points the scan hit, in metres. Beams with no return produce no point. */
	ROBOAUTONOMY_API TArray<FVector2D> ScanToPoints(const FLidarScan& Scan);

	/**
	 * Whether the robot could drive straight along a heading for the lookahead
	 * distance without any point coming within its half-width plus margin.
	 */
	ROBOAUTONOMY_API bool IsHeadingClear(const TArray<FVector2D>& Points, double HeadingRad,
		const FAvoidanceSettings& Settings);

	/**
	 * The clear heading closest to the goal's bearing, searching outward from
	 * it. Unset if every heading the scan can see is blocked.
	 */
	ROBOAUTONOMY_API TOptional<double> ChooseHeading(const FLidarScan& Scan, double GoalBearingRad,
		const FAvoidanceSettings& Settings);
}