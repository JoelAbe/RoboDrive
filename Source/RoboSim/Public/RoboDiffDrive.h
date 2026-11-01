#pragma once

#include "CoreMinimal.h"

/**
 * Differential drive kinematics.
 *
 * Two independently driven wheels on a common axle. Steering comes from the
 * speed difference between them, which is also what makes spin-in-place
 * possible and what makes wheel odometry drift when a wheel slips.
 *
 * All values are REP-103 SI: metres, seconds, radians. Positive v is forward,
 * positive omega turns left. See RoboFrames.h.
 *
 * These functions assume WheelSeparation and WheelRadius are positive.
 * Validating them belongs where they are configured, not on every call.
 */
namespace RoboDiffDrive
{
	/** How the robot is asked to move, or is measured to be moving. */
	struct FBodyVelocity
	{
		/** Forward speed, m/s. */
		double Linear = 0.0;

		/** Yaw rate, rad/s. Positive turns left. */
		double Angular = 0.0;
	};

	/** Linear speed of each wheel at its contact patch, m/s. */
	struct FWheelSpeeds
	{
		double Left = 0.0;
		double Right = 0.0;
	};

	/**
	 * Inverse kinematics: what the wheels must do to achieve a body velocity.
	 *
	 * Each wheel is offset sideways by half the wheel separation, so rotation
	 * adds speed at one wheel and removes it at the other.
	 */
	inline FWheelSpeeds ToWheelSpeeds(const FBodyVelocity& Body, double WheelSeparation)
	{
		const double HalfTrack = 0.5 * WheelSeparation;
		return FWheelSpeeds{
			Body.Linear - Body.Angular * HalfTrack,
			Body.Linear + Body.Angular * HalfTrack
		};
	}

	/** Forward kinematics: what the body is doing, given the wheel speeds. */
	inline FBodyVelocity ToBodyVelocity(const FWheelSpeeds& Wheels, double WheelSeparation)
	{
		return FBodyVelocity{
			0.5 * (Wheels.Right + Wheels.Left),
			(Wheels.Right - Wheels.Left) / WheelSeparation
		};
	}

	/** Contact-patch speed (m/s) to wheel rotation rate (rad/s), rolling without slip. */
	inline double ToWheelAngularSpeed(double WheelLinearSpeed, double WheelRadius)
	{
		return WheelLinearSpeed / WheelRadius;
	}

	/** Wheel rotation rate (rad/s) back to contact-patch speed (m/s). */
	inline double ToWheelLinearSpeed(double WheelAngularSpeed, double WheelRadius)
	{
		return WheelAngularSpeed * WheelRadius;
	}

	/**
	 * Move a command towards a target by no more than the given accelerations
	 * allow in DeltaSeconds. A command that jumps asks the wheels for more grip
	 * than they have: they slip, and slip is what fools wheel odometry.
	 */
	inline FBodyVelocity LimitAcceleration(const FBodyVelocity& Current, const FBodyVelocity& Target,
		double MaxLinearAccelMS2, double MaxAngularAccelRadS2, double DeltaSeconds)
	{
		const double LinearStep = MaxLinearAccelMS2 * DeltaSeconds;
		const double AngularStep = MaxAngularAccelRadS2 * DeltaSeconds;

		FBodyVelocity Limited;
		Limited.Linear = Current.Linear + FMath::Clamp(Target.Linear - Current.Linear, -LinearStep, LinearStep);
		Limited.Angular = Current.Angular + FMath::Clamp(Target.Angular - Current.Angular, -AngularStep, AngularStep);
		return Limited;
	}
}