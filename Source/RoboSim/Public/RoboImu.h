#pragma once

#include "CoreMinimal.h"
#include "RoboNoise.h"
#include "RoboImu.generated.h"

/** One IMU reading, in the sensor's own frame, REP-103 units. */
USTRUCT(BlueprintType)
struct FImuSample
{
	GENERATED_BODY()

	/** Specific force, m/s^2. At rest this is +9.81 on the up axis, not zero. */
	UPROPERTY(BlueprintReadOnly, Category = "IMU")
	FVector AccelerationMS2 = FVector::ZeroVector;

	/** Angular velocity, rad/s. */
	UPROPERTY(BlueprintReadOnly, Category = "IMU")
	FVector AngularVelocityRadS = FVector::ZeroVector;

	/** Simulation time of the reading, s. */
	UPROPERTY(BlueprintReadOnly, Category = "IMU")
	double TimeSeconds = 0.0;
};

/**
 * Inertial measurement models.
 *
 * An accelerometer measures proper acceleration: the force on a test mass
 * inside it, not the rate of change of velocity. At rest that is gravity
 * pushing up through the case; in free fall it is nothing at all.
 */
namespace RoboImu
{
	/** Standard gravity in REP-103 world coordinates: down is -Z. */
	static const FVector GravityMS2 = FVector(0.0, 0.0, -9.80665);

	/**
	 * What an ideal accelerometer reads.
	 *
	 * @param WorldAccelerationMS2  True acceleration of the sensor, world frame.
	 * @param BodyRotation          Sensor orientation, world to body.
	 */
	inline FVector ProperAcceleration(const FVector& WorldAccelerationMS2, const FQuat& BodyRotation)
	{
		// Subtracting gravity is what makes a stationary sensor read +1 g and a
		// falling one read zero.
		return BodyRotation.UnrotateVector(WorldAccelerationMS2 - GravityMS2);
	}

	/** What an ideal gyroscope reads: world angular velocity seen from the body. */
	inline FVector BodyAngularVelocity(const FVector& WorldAngularVelocityRadS, const FQuat& BodyRotation)
	{
		return BodyRotation.UnrotateVector(WorldAngularVelocityRadS);
	}

	/** Apply bias then Gaussian noise to a three-axis reading. */
	inline FVector Corrupt(const FVector& Ideal, const FVector& Bias, double NoiseStdDev, FRandomStream& Stream)
	{
		return FVector(
			RoboNoise::Gaussian(Stream, Ideal.X + Bias.X, NoiseStdDev),
			RoboNoise::Gaussian(Stream, Ideal.Y + Bias.Y, NoiseStdDev),
			RoboNoise::Gaussian(Stream, Ideal.Z + Bias.Z, NoiseStdDev));
	}
}