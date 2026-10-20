#include "RoboAvoidingBrain.h"

#include "RoboFrames.h"

FAvoidingBrain::FAvoidingBrain(const FGoToGoalSettings& InGoToGoalSettings,
	const FAvoidanceSettings& InAvoidanceSettings)
	: GoToGoal(InGoToGoalSettings)
	, GoToGoalSettings(InGoToGoalSettings)
	, AvoidanceSettings(InAvoidanceSettings)
{
}

void FAvoidingBrain::Reset()
{
	GoToGoal.Reset();
	BlockedSinceSeconds.Reset();
}

FBrainOutput FAvoidingBrain::Update(const FBrainInputs& Inputs)
{
	// Go-to-goal decides whether there is a goal and whether we have reached
	// it. If not travelling, obstacles are irrelevant: pass the answer through.
	FBrainOutput Output = GoToGoal.Update(Inputs);

	if (Output.Status != EMissionStatus::Active)
	{
		BlockedSinceSeconds.Reset();
		return Output;
	}

	// Never drive blind. Just after a reset the lidar may not have swept yet,
	// and moving with no idea what is ahead is how robots hit things in their
	// first moment.
	if (!Inputs.bHasScan)
	{
		Output.Command = RoboDiffDrive::FBodyVelocity();
		Output.Activity = EBrainActivity::WaitingForScan;
		return Output;
	}

	const double DX = Inputs.Goal.X - Inputs.Odometry.X;
	const double DY = Inputs.Goal.Y - Inputs.Odometry.Y;
	const double DistanceM = FMath::Sqrt(DX * DX + DY * DY);

	// Where the goal is, relative to the robot's own heading. The lidar also
	// measures relative to the robot, so goal and obstacles share one frame.
	// Odometry drift can therefore send the robot to the wrong place, but it
	// cannot make it misjudge an obstacle directly in front of it.
	const double GoalBearingRad = RoboFrames::WrapRadians(
		FMath::Atan2(DY, DX) - Inputs.Odometry.YawRad);

	const TOptional<double> Heading =
		RoboAvoidance::ChooseHeading(Inputs.Scan, GoalBearingRad, AvoidanceSettings);

	if (Heading.IsSet())
	{
		BlockedSinceSeconds.Reset();
		Output.Command = FGoToGoalBrain::SteerTowards(Heading.GetValue(), DistanceM, GoToGoalSettings);
		Output.ChosenHeadingRad = Heading;

		// The search tries the goal's own bearing first, so a clear line to the
		// goal gives a deviation of exactly zero; the tolerance only absorbs rounding.
		Output.DeviationRad = RoboFrames::WrapRadians(Heading.GetValue() - GoalBearingRad);
		Output.Activity = FMath::IsNearlyZero(Output.DeviationRad, UE_KINDA_SMALL_NUMBER)
			? EBrainActivity::DrivingToGoal
			: EBrainActivity::SteeringAround;
		return Output;
	}

	// Boxed in: nothing ahead is clear. Remember when this started.
	if (!BlockedSinceSeconds.IsSet())
	{
		BlockedSinceSeconds = Inputs.SimTimeSeconds;
	}

	Output.BlockedForSeconds = Inputs.SimTimeSeconds - BlockedSinceSeconds.GetValue();
	
	if (Inputs.SimTimeSeconds - BlockedSinceSeconds.GetValue() > AvoidanceSettings.BlockedTimeoutS)
	{
		// Admitting failure is far better than the alternative: pushing against
		// an obstacle while odometry counts wheel rotation as progress, until
		// the brain confidently reports arriving somewhere it never reached.
		Output.Command = RoboDiffDrive::FBodyVelocity();
		Output.Status = EMissionStatus::Failed;
		Output.Activity = EBrainActivity::GaveUp;
		return Output;
	}

	// Stop driving forward and turn slowly toward the goal's side. A new
	// viewpoint may reveal a gap the current one cannot see.
	Output.Command.Linear = 0.0;
	Output.Command.Angular = (GoalBearingRad >= 0.0 ? 0.5 : -0.5) * GoToGoalSettings.MaxTurnRadS;
	Output.Activity = EBrainActivity::BoxedIn;
	
	return Output;
}