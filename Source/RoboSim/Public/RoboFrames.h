#pragma once

#include "CoreMinimal.h"

/**
 * Conversions between Unreal's world frame and ROS REP-103.
 *
 *   Unreal:   centimetres, X forward, Y right, Z up  (left-handed)
 *   REP-103:  metres,      X forward, Y left,  Z up  (right-handed)
 *
 * Code inside RoboSim and RoboAutonomy works in REP-103: metres, radians,
 * right-handed. Unreal units only exist on the other side of these functions.
 */

namespace RoboFrames
{
	constexpr double MetersPerUU = 0.01;
	constexpr double UUPerMeter = 100.0;
	
	/**
	 * Unreal's force unit is kg.cm/s^2, because its masses are kg and its
	 * distances cm. A newton is kg.m/s^2, so it is 100 Unreal force units.
	 */
	constexpr double UUForcePerNewton = 100.0;

	/** Positions: mirror Y, then scale cm -> m. */
	inline FVector PositionToRos(const FVector& UePosition)
	{
		return FVector(UePosition.X, -UePosition.Y, UePosition.Z) * MetersPerUU;
	}

	inline FVector PositionToUe(const FVector& RosPosition)
	{
		return FVector(RosPosition.X, -RosPosition.Y, RosPosition.Z) * UUPerMeter;
	}

	/** Linear velocity follows the same rule as position (cm/s <-> m/s). */
	inline FVector LinearVelocityToRos(const FVector& UeVelocity)
	{
		return PositionToRos(UeVelocity);
	}

	inline FVector LinearVelocityToUe(const FVector& RosVelocity)
	{
		return PositionToUe(RosVelocity);
	}

	/**
	 * Yaw (heading).
	 *   Unreal:  degrees, positive = clockwise seen from above (turning right).
	 *   REP-103: radians, positive = counter-clockwise (turning left).
	 */
	inline double YawToRos(double UeYawDegrees)
	{
		return -FMath::DegreesToRadians(UeYawDegrees);
	}

	inline double YawToUe(double RosYawRadians)
	{
		return -FMath::RadiansToDegrees(RosYawRadians);
	}

	/**
	 * Angular velocity. Unreal reports degrees per second; REP-103 wants
	 * radians per second.
	 *
	 * The handedness flip mirrors Y, and a mirror reverses the sense of
	 * rotation as well as the axis. For rotation about X or Z the axis
	 * survives the mirror but the spin reverses, so those components flip.
	 * For rotation about Y both reverse, and the two cancel. That is the
	 * opposite pattern to positions, where only Y flips.
	 */
	inline FVector AngularVelocityToRos(const FVector& UeAngularVelocityDegS)
	{
		return FVector(
			-FMath::DegreesToRadians(UeAngularVelocityDegS.X),
			 FMath::DegreesToRadians(UeAngularVelocityDegS.Y),
			-FMath::DegreesToRadians(UeAngularVelocityDegS.Z));
	}

	inline FVector AngularVelocityToUe(const FVector& RosAngularVelocityRadS)
	{
		return FVector(
			-FMath::RadiansToDegrees(RosAngularVelocityRadS.X),
			 FMath::RadiansToDegrees(RosAngularVelocityRadS.Y),
			-FMath::RadiansToDegrees(RosAngularVelocityRadS.Z));
	}

	/**
	 * Orientation. The quaternion's vector part is the rotation axis scaled by
	 * sin(angle/2), so it follows the angular rule above. The scalar part is
	 * cos(angle/2), a plain number with no direction, so it is unchanged.
	 *
	 * A mirror is its own inverse, which is why the two directions are the
	 * same operation.
	 */
	inline FQuat RotationToRos(const FQuat& UeRotation)
	{
		return FQuat(-UeRotation.X, UeRotation.Y, -UeRotation.Z, UeRotation.W);
	}

	inline FQuat RotationToUe(const FQuat& RosRotation)
	{
		return FQuat(-RosRotation.X, RosRotation.Y, -RosRotation.Z, RosRotation.W);
	}
	
	/**
	 * Wrap an angle to (-pi, pi], in full double precision.
	 *
	 * FMath::UnwindRadians is templated on the type but subtracts the float
	 * constant UE_TWO_PI, which injects roughly 3e-8 rad per wrap. That is
	 * invisible in gameplay and ruinous in odometry, where a heading error is
	 * multiplied by distance travelled and never corrects itself.
	 */
	inline double WrapRadians(double Radians)
	{
		const double Shifted = FMath::Fmod(Radians + UE_DOUBLE_PI, UE_DOUBLE_TWO_PI);
		const double Positive = Shifted <= 0.0 ? Shifted + UE_DOUBLE_TWO_PI : Shifted;
		return Positive - UE_DOUBLE_PI;
	}
}
