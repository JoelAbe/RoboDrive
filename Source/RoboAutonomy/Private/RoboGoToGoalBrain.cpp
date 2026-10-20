#include "RoboGoToGoalBrain.h"

#include "RoboFrames.h"

namespace
{
	/** Goals come from configuration, not computation, so exact comparison is correct. */
	bool GoToGoalIsSameGoal(const FMissionGoal& A, const FMissionGoal& B)
	{
		return A.X == B.X && A.Y == B.Y && A.ToleranceM == B.ToleranceM;
	}
}

FGoToGoalBrain::FGoToGoalBrain(const FGoToGoalSettings& InSettings)
	: Settings(InSettings)
{
}

void FGoToGoalBrain::Reset()
{
	bArrived = false;
	bHasLastGoal = false;
}

RoboDiffDrive::FBodyVelocity FGoToGoalBrain::SteerTowards(double HeadingErrorRad, double DistanceM,
	const FGoToGoalSettings& Settings)
{
	RoboDiffDrive::FBodyVelocity Command;

	Command.Angular = FMath::Clamp(
		Settings.HeadingGain * HeadingErrorRad, -Settings.MaxTurnRadS, Settings.MaxTurnRadS);

	// Drive forward only when roughly facing the heading: cos is 1 when facing
	// it, 0 when it is to the side, and negative when it is behind, which is
	// clamped to zero so the robot turns on the spot rather than reversing.
	Command.Linear = FMath::Min(Settings.MaxSpeedMS, Settings.SpeedGain * DistanceM)
		* FMath::Max(0.0, FMath::Cos(HeadingErrorRad));

	return Command;
}

FBrainOutput FGoToGoalBrain::Update(const FBrainInputs& Inputs)
{
	// A default output is a zero command, which is a stop.
	FBrainOutput Output;

	if (!Inputs.bHasGoal)
	{
		Reset();
		return Output;
	}

	// A new goal is a new journey: having arrived at the last one says nothing
	// about this one.
	if (!bHasLastGoal || !GoToGoalIsSameGoal(Inputs.Goal, LastGoal))
	{
		bArrived = false;
		LastGoal = Inputs.Goal;
		bHasLastGoal = true;
	}

	const double DX = Inputs.Goal.X - Inputs.Odometry.X;
	const double DY = Inputs.Goal.Y - Inputs.Odometry.Y;
	const double Distance = FMath::Sqrt(DX * DX + DY * DY);

	// Hysteresis: arrive inside the inner radius, but once arrived, stay
	// arrived until beyond the outer one. A single threshold would have the
	// robot stop, coast a centimetre past it, decide it had left, and start
	// again, indefinitely.
	const double ArriveRadius = Settings.ArriveFraction * Inputs.Goal.ToleranceM;
	const double LeaveRadius = FMath::Max(Settings.LeaveFraction * Inputs.Goal.ToleranceM, ArriveRadius);

	const bool bInside = bArrived ? (Distance <= LeaveRadius) : (Distance < ArriveRadius);
	if (bInside)
	{
		bArrived = true;
		Output.Status = EMissionStatus::Arrived;
		Output.Activity = EBrainActivity::Arrived;
		return Output;
	}

	bArrived = false;

	// Double-precision wrap, not FMath::UnwindRadians: the engine's version
	// subtracts a float constant and loses precision.
	const double HeadingError = RoboFrames::WrapRadians(
		FMath::Atan2(DY, DX) - Inputs.Odometry.YawRad);

	Output.Command = SteerTowards(HeadingError, Distance, Settings);

	Output.Status = EMissionStatus::Active;
	Output.Activity = EBrainActivity::DrivingToGoal;
	return Output;
}