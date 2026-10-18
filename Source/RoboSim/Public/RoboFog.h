#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "RoboLidar.h"

/**
 * How fog affects a lidar.
 *
 * Fog does two things, and both show up in real lidar tests in fog chambers:
 *
 * - Attenuation. Fog scatters light out of the beam on the way out and on the
 *   way back, so returns from real surfaces weaken and the usable range shrinks.
 * - Clutter. Some scattered light comes straight back, so the sensor reports a
 *   return from the fog itself: an obstacle that is not there.
 *
 * Fog is described by its meteorological visibility, the distance at which a
 * dark object can no longer be told apart from the background. It is what a
 * weather report gives, and what a person can estimate by eye.
 */
namespace RoboFog
{
	/**
	 * Extinction coefficient: the fraction of light scattered out of a beam per
	 * metre of fog, 1/m.
	 *
	 * From Koschmieder's law. Contrast falls as exp(-Extinction * Distance), and
	 * visibility is defined as the distance where contrast has fallen to 2%, so
	 * Extinction = ln(1 / 0.02) / Visibility = ln(50) / Visibility.
	 */
	inline double ExtinctionPerM(double VisibilityM)
	{
		// Callers validate visibility, but zero must never reach the division:
		// treat nonsense as clear air rather than as infinitely thick fog.
		if (VisibilityM <= 0.0)
		{
			return 0.0;
		}

		return FMath::Loge(50.0) / VisibilityM;
	}

	/**
	 * Whether a return from a surface RangeM away is strong enough to detect.
	 *
	 * The sensor is specified, like a real one, by the range at which it can just
	 * detect a target in clear air: RangeMaxM. A return weakens with the square
	 * of distance, as the light spreads out, and fog removes a further fraction
	 * exp(-2 * Extinction * Range), the 2 because the light crosses the fog twice.
	 * The return is detectable when
	 *
	 *     (RangeMax / Range)^2 * exp(-2 * Extinction * Range) >= 1
	 *
	 * Taking the square root of both sides and rearranging gives the test below.
	 * In clear air it reduces to Range <= RangeMax: with no fog, the lidar
	 * behaves exactly as it did before fog existed.
	 */
	inline bool IsDetectable(double RangeM, double RangeMaxM, double ExtinctionPerM)
	{
		return RangeM * FMath::Exp(ExtinctionPerM * RangeM) <= RangeMaxM;
	}

	/**
	 * How far along the beam the fog reflects it back, m, or NoReturn if it
	 * does not.
	 *
	 * The chance per metre that the beam meets fog dense enough to send back a
	 * detectable echo is EchoFraction * Extinction. Something with a fixed chance
	 * per metre is first met at an exponentially distributed distance: short
	 * distances are most likely, and long ones increasingly rare.
	 */
	inline double SampleEchoM(FRandomStream& Stream, double ExtinctionPerM, double EchoFraction)
	{
		const double RatePerM = ExtinctionPerM * EchoFraction;

		// No fog, no echo, and no random number drawn. Clear air must leave the
		// noise sequence untouched, so runs without fog are identical to runs
		// recorded before fog existed.
		if (RatePerM <= 0.0)
		{
			return RoboLidar::NoReturn();
		}

		// Inverse transform sampling: if U is uniform on (0, 1], then
		// -ln(U) / Rate is exponentially distributed with mean 1 / Rate. Zero is
		// nudged off because ln(0) is undefined, as in RoboNoise.
		const double U = FMath::Max(Stream.GetFraction(), UE_DOUBLE_SMALL_NUMBER);
		return -FMath::Loge(U) / RatePerM;
	}
}