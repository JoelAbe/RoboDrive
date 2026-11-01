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

	// Scans from before a reset were taken somewhere else, in an odometry
	// frame that no longer applies.
	CurrentScanOdom.Reset();
	CurrentScanTimeSeconds.Reset();
	EarlierScansOdom.Reset();
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

	// A new scan becomes the current one, and the one it replaces joins the
	// earlier scans. Returns are stored in the odometry frame, the world as
	// the robot believes it to be, so they still line up after it moves. The
	// brain can run twice on one scan before the lidar sweeps again; counting
	// that scan twice would let it confirm itself, so it is stored only once.
	if (!CurrentScanTimeSeconds.IsSet() || Inputs.Scan.TimeSeconds != CurrentScanTimeSeconds.GetValue())
	{
		if (CurrentScanTimeSeconds.IsSet())
		{
			EarlierScansOdom.Add(MoveTemp(CurrentScanOdom));
		}

		CurrentScanOdom.Reset();
		for (const FVector2D& PointM : RoboAvoidance::ScanToPoints(Inputs.Scan))
		{
			CurrentScanOdom.Add(RoboAvoidance::RobotToOdometry(PointM, Inputs.Odometry));
		}
		CurrentScanTimeSeconds = Inputs.Scan.TimeSeconds;
	}

	// Keep only as many earlier scans as the filter compares against.
	const int32 EarlierNeeded = FMath::Max(AvoidanceSettings.PersistenceScans, 1) - 1;
	while (EarlierScansOdom.Num() > EarlierNeeded)
	{
		EarlierScansOdom.RemoveAt(0);
	}

	// Too few scans yet to tell an obstacle from clutter: hold still, for the
	// same reason as having no scan at all. A fifth of a second after a reset.
	if (EarlierScansOdom.Num() < EarlierNeeded)
	{
		Output.Command = RoboDiffDrive::FBodyVelocity();
		Output.Activity = EBrainActivity::WaitingForScan;
		return Output;
	}

	// The clutter filter, then back into the robot's frame to choose a heading.
	const TArray<FVector2D> PersistentOdom = RoboAvoidance::KeepPersistent(
		CurrentScanOdom, EarlierScansOdom, AvoidanceSettings.PersistenceRadiusM);

	for (const FVector2D& PointOdom : PersistentOdom)
	{
		Output.ObstaclePointsM.Add(RoboAvoidance::OdometryToRobot(PointOdom, Inputs.Odometry));
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

	const TOptional<double> Heading = RoboAvoidance::ChooseHeadingAmong(
		Output.ObstaclePointsM, Inputs.Scan, GoalBearingRad, AvoidanceSettings);

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