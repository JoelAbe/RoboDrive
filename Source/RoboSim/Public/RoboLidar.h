#pragma once

#include "CoreMinimal.h"
#include "RoboLidar.generated.h"

/**
 * One sweep, mirroring the ROS sensor_msgs/LaserScan layout so a Phase 6
 * bridge is a copy rather than a translation.
 */
USTRUCT(BlueprintType)
struct FLidarScan
{
	GENERATED_BODY()

	/** Range per beam, m. Infinite means no return, which is not the same as far away. */
	TArray<double> RangesM;

	/** Angle of the first beam, rad, counter-clockwise from forward. */
	UPROPERTY(BlueprintReadOnly, Category = "Lidar")
	double AngleMinRad = 0.0;

	/** Angle between beams, rad. */
	UPROPERTY(BlueprintReadOnly, Category = "Lidar")
	double AngleIncrementRad = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Lidar")
	double RangeMinM = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Lidar")
	double RangeMaxM = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Lidar")
	double TimeSeconds = 0.0;
};

namespace RoboLidar
{
	/** Reported when a beam finds nothing. Maps to inf in a ROS LaserScan. */
	inline double NoReturn()
	{
		return TNumericLimits<double>::Max();
	}

	inline bool HasReturn(double RangeM)
	{
		return RangeM < NoReturn();
	}

	/**
	 * Angle of beam Index, rad, counter-clockwise from forward.
	 *
	 * A sweep of N beams across a span is divided into N-1 gaps so the first
	 * and last beams land exactly on the edges of the span.
	 */
	inline double BeamAngleRad(int32 Index, int32 BeamCount, double AngularSpanRad)
	{
		if (BeamCount <= 1)
		{
			return 0.0;
		}

		const double Increment = AngularSpanRad / (BeamCount - 1);
		return -0.5 * AngularSpanRad + Index * Increment;
	}

	/** Direction of a beam in the sensor's frame, REP-103 (+Y left). */
	inline FVector BeamDirection(double AngleRad)
	{
		return FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0);
	}
}