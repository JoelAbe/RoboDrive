#pragma once

#include "CoreMinimal.h"

/** One row of a recorded run. All values REP-103 SI, timestamps in sim time. */
struct FRunSample
{
	double TimeSeconds = 0.0;
	int64 Step = 0;

	/** The command that won arbitration, and which source produced it. */
	double CommandLinearMS = 0.0;
	double CommandAngularRadS = 0.0;
	int32 CommandSource = 0;

	/** Where the robot actually is. */
	double TrueX = 0.0;
	double TrueY = 0.0;
	double TrueYawRad = 0.0;

	/** Where wheel odometry thinks it is. */
	double OdomX = 0.0;
	double OdomY = 0.0;
	double OdomYawRad = 0.0;

	double PositionErrorM = 0.0;
	double HeadingErrorRad = 0.0;
	double TravelledM = 0.0;

	/** Heading from integrating the gyroscope alone, and its error. */
	double GyroHeadingRad = 0.0;
	double GyroHeadingErrorRad = 0.0;

	double SpeedMS = 0.0;
};

/**
 * CSV formatting for recorded runs.
 *
 * Header and row are produced by functions next to each other so they cannot
 * drift apart. A column added to one and forgotten in the other silently
 * shifts every value afterwards, which is the classic way a data file lies.
 */
namespace RoboRunRecord
{
	inline FString Header()
	{
		return TEXT("time_s,step,cmd_v_ms,cmd_w_rads,cmd_source,")
			TEXT("true_x_m,true_y_m,true_yaw_rad,")
			TEXT("odom_x_m,odom_y_m,odom_yaw_rad,")
			TEXT("pos_error_m,heading_error_rad,travelled_m,")
			TEXT("gyro_heading_rad,gyro_heading_error_rad,speed_ms");
	}

	inline FString Format(const FRunSample& Sample)
	{
		// Enough digits that rounding in the file never dominates the errors
		// being measured.
		return FString::Printf(
			TEXT("%.6f,%lld,%.6f,%.6f,%d,")
			TEXT("%.6f,%.6f,%.6f,")
			TEXT("%.6f,%.6f,%.6f,")
			TEXT("%.6f,%.6f,%.6f,")
			TEXT("%.6f,%.6f,%.6f"),
			Sample.TimeSeconds, Sample.Step,
			Sample.CommandLinearMS, Sample.CommandAngularRadS, Sample.CommandSource,
			Sample.TrueX, Sample.TrueY, Sample.TrueYawRad,
			Sample.OdomX, Sample.OdomY, Sample.OdomYawRad,
			Sample.PositionErrorM, Sample.HeadingErrorRad, Sample.TravelledM,
			Sample.GyroHeadingRad, Sample.GyroHeadingErrorRad, Sample.SpeedMS);
	}

	/** Column count, used to check the header and rows agree. */
	inline int32 CountColumns(const FString& Line)
	{
		int32 Commas = 0;
		for (const TCHAR Character : Line)
		{
			Commas += (Character == TEXT(',')) ? 1 : 0;
		}
		return Commas + 1;
	}
}