#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/**
 * Noise models for simulated sensors.
 *
 * Every draw comes from a caller-owned FRandomStream rather than global
 * randomness, so a run with the same seed produces the same noise. Sensor
 * noise that cannot be reproduced makes a simulation impossible to debug and
 * a recorded run impossible to replay.
 */
namespace RoboNoise
{
	/**
	 * One sample from a normal distribution, by the Box-Muller transform:
	 * two uniform samples become one normally distributed one.
	 *
	 * Gaussian noise is the usual model for sensor error because most real
	 * sensors sum many small independent disturbances, and such sums tend
	 * toward a normal distribution.
	 */
	inline double Gaussian(FRandomStream& Stream, double Mean, double StdDev)
	{
		if (StdDev <= 0.0)
		{
			return Mean;
		}

		// Log of zero is undefined, so nudge the first sample off zero.
		const double U1 = FMath::Max(Stream.GetFraction(), UE_DOUBLE_SMALL_NUMBER);
		const double U2 = Stream.GetFraction();

		const double Magnitude = FMath::Sqrt(-2.0 * FMath::Loge(U1));
		const double Normal = Magnitude * FMath::Cos(UE_DOUBLE_TWO_PI * U2);

		return Mean + StdDev * Normal;
	}
}