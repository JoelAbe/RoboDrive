#pragma once

#include "CoreMinimal.h"
#include "RoboDiffDrive.h"

/** Fixed driving patterns, used to exercise the drive path before real autonomy exists. */
namespace RoboPatterns
{
	/**
	 * Drive at a constant speed around a circle of a given radius.
	 *
	 * A body moving at v around a circle of radius R turns at omega = v / R.
	 * Positive radius curves left, matching the REP-103 sign convention.
	 * A radius of zero means drive straight, since an infinite turn rate is
	 * not a useful interpretation.
	 */
	inline RoboDiffDrive::FBodyVelocity CircleCommand(double SpeedMS, double RadiusM)
	{
		// Anything tighter than this counts as straight: an infinite turn rate
		// is not a useful interpretation of "zero radius".
		constexpr double MinRadiusM = UE_DOUBLE_KINDA_SMALL_NUMBER;
		
		if (FMath::Abs(RadiusM) < MinRadiusM)
		{
			return RoboDiffDrive::FBodyVelocity{ SpeedMS, 0.0 };
		}

		// Clamped away from zero so the divisor cannot be zero by construction,
		// rather than only by the guard above. The guard states the intent; this
		// is what makes it provable to the compiler.
		const double SafeRadiusM = RadiusM >= 0.0 ? FMath::Max(RadiusM, MinRadiusM) : FMath::Min(RadiusM, -MinRadiusM);

		return RoboDiffDrive::FBodyVelocity{ SpeedMS, SpeedMS / SafeRadiusM };
	}
}