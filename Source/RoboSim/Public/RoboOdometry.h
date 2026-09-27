#pragma once

#include "CoreMinimal.h"
#include "RoboDiffDrive.h"
#include "RoboFrames.h"
#include "RoboOdometry.generated.h"

/**
 * A planar pose in REP-103: metres, radians, +X forward, +Y left,
 * yaw positive counter-clockwise.
 */
USTRUCT(BlueprintType)
struct FPose2D
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Pose", meta = (Units = "m"))
	double X = 0.0;

	UPROPERTY(BlueprintReadWrite, Category = "Pose", meta = (Units = "m"))
	double Y = 0.0;

	UPROPERTY(BlueprintReadWrite, Category = "Pose")
	double YawRad = 0.0;
};

/**
 * Dead reckoning: estimating pose by integrating velocity.
 *
 * Nothing here consults the world, which is the point. Every error stays in
 * the estimate permanently, so the estimate drifts away from truth without
 * bound. The integration itself must not contribute to that drift: the
 * interesting error comes from the sensors, not from the arithmetic.
 */
namespace RoboOdometry
{
	/**
	 * Advance a pose by a body velocity held constant for DeltaSeconds.
	 *
	 * Follows the circular arc the robot actually travels rather than stepping
	 * straight and then turning. Straight stepping cuts every corner in the
	 * same direction, which is a systematic bias that would grow with distance
	 * and swamp the sensor errors we want to show.
	 */
	inline FPose2D Integrate(const FPose2D& Pose, const RoboDiffDrive::FBodyVelocity& Velocity, double DeltaSeconds)
	{
		// Below this the arc and a straight line are indistinguishable, and
		// the arc formula divides by the turn rate.
		constexpr double MinAngularRadS = 1e-9;

		const double Yaw = Pose.YawRad;
		const double NewYaw = Yaw + Velocity.Angular * DeltaSeconds;

		FPose2D Result;
		Result.YawRad = RoboFrames::WrapRadians(NewYaw);

		if (FMath::Abs(Velocity.Angular) < MinAngularRadS)
		{
			// Straight line: the limit of the arc as the turn rate goes to zero.
			const double DistanceM = Velocity.Linear * DeltaSeconds;
			Result.X = Pose.X + DistanceM * FMath::Cos(Yaw);
			Result.Y = Pose.Y + DistanceM * FMath::Sin(Yaw);
			return Result;
		}

		// Clamped away from zero so the divisor cannot be zero by construction,
		// not only by the branch above.
		const double SafeAngular = Velocity.Angular >= 0.0
			? FMath::Max(Velocity.Angular, MinAngularRadS)
			: FMath::Min(Velocity.Angular, -MinAngularRadS);

		const double RadiusM = Velocity.Linear / SafeAngular;
		Result.X = Pose.X + RadiusM * (FMath::Sin(NewYaw) - FMath::Sin(Yaw));
		Result.Y = Pose.Y - RadiusM * (FMath::Cos(NewYaw) - FMath::Cos(Yaw));
		return Result;
	}

	/** Straight-line distance between two poses, m. This is the drift number. */
	inline double PositionErrorM(const FPose2D& A, const FPose2D& B)
	{
		return FMath::Sqrt(FMath::Square(A.X - B.X) + FMath::Square(A.Y - B.Y));
	}

	/** Signed shortest angle from B's heading to A's, rad, wrapped to (-pi, pi]. */
	inline double HeadingErrorRad(const FPose2D& A, const FPose2D& B)
	{
		return RoboFrames::WrapRadians(A.YawRad - B.YawRad);
	}
}