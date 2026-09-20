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
}
