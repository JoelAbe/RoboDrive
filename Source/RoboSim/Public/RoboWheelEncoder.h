#pragma once

#include "CoreMinimal.h"
#include "RoboNoise.h"

/**
 * A simulated incremental wheel encoder.
 *
 * Counts ticks of wheel rotation, and nothing else. Whatever odometry
 * believes about distance is derived from that count and an assumed wheel
 * radius, which is why an error in that assumed radius biases every
 * measurement the same way forever.
 *
 * Fractional rotation is retained between updates rather than discarded, as a
 * real encoder does: quantisation bounds the instantaneous error but does not
 * accumulate.
 */
class FWheelEncoder
{
public:
	explicit FWheelEncoder(int32 InTicksPerRevolution = 2048) : TicksPerRevolution(FMath::Max(InTicksPerRevolution, 1))
	{
	}

	/**
	 * Advance the encoder by the wheel's rotation over one step.
	 *
	 * @param WheelAngularSpeedRadS  How fast the wheel is turning, rad/s.
	 * @param DeltaSeconds           Step length, s.
	 * @param Stream                 Seeded source for the noise.
	 * @param NoiseStdDevRadS        Standard deviation of the speed noise.
	 * @return Ticks counted this step, positive or negative.
	 */
	int64 Update(double WheelAngularSpeedRadS, double DeltaSeconds, FRandomStream& Stream, double NoiseStdDevRadS)
	{
		if (DeltaSeconds <= 0.0)
		{
			return 0;
		}

		const double MeasuredSpeed = RoboNoise::Gaussian(Stream, WheelAngularSpeedRadS, NoiseStdDevRadS);

		PendingRadians += MeasuredSpeed * DeltaSeconds;

		// Only whole ticks are reported; the remainder waits for next time.
		const double RadiansPerTick = UE_DOUBLE_TWO_PI / TicksPerRevolution;
		const int64 NewTicks = static_cast<int64>(PendingRadians / RadiansPerTick);

		PendingRadians -= NewTicks * RadiansPerTick;
		TickCount += NewTicks;

		return NewTicks;
	}

	/** Total ticks since reset. This is the only thing a real encoder exposes. */
	int64 GetTickCount() const { return TickCount; }

	int32 GetTicksPerRevolution() const { return TicksPerRevolution; }

	void Reset()
	{
		TickCount = 0;
		PendingRadians = 0.0;
	}

	/**
	 * Convert a tick count into the distance odometry believes was travelled.
	 *
	 * AssumedRadiusM is what the software thinks the wheel is, which need not
	 * be what it is. That difference is a calibration error, and it scales
	 * every distance by the same factor rather than averaging out.
	 */
	static double TicksToDistanceM(int64 Ticks, int32 TicksPerRevolution, double AssumedRadiusM)
	{
		const double Revolutions = static_cast<double>(Ticks) / FMath::Max(TicksPerRevolution, 1);
		return Revolutions * UE_DOUBLE_TWO_PI * AssumedRadiusM;
	}

private:
	int32 TicksPerRevolution;
	int64 TickCount = 0;

	/** Rotation not yet worth a whole tick, carried to the next update. */
	double PendingRadians = 0.0;
};