#include "RoboAvoidance.h"

#include "RoboFrames.h"

TArray<FVector2D> RoboAvoidance::ScanToPoints(const FLidarScan& Scan)
{
	TArray<FVector2D> Points;
	Points.Reserve(Scan.RangesM.Num());

	for (int32 Index = 0; Index < Scan.RangesM.Num(); ++Index)
	{
		const double RangeM = Scan.RangesM[Index];

		// No return means nothing was seen within range. In this simulation
		// every surface reflects, so that genuinely means clear. On a real robot,
		// glass and matte black surfaces also return nothing, and treating them
		// as clear is how robots drive into them: this assumption would need
		// revisiting before trusting it on hardware.
		if (!RoboLidar::HasReturn(RangeM))
		{
			continue;
		}

		const double AngleRad = Scan.AngleMinRad + Index * Scan.AngleIncrementRad;
		Points.Emplace(RangeM * FMath::Cos(AngleRad), RangeM * FMath::Sin(AngleRad));
	}

	return Points;
}

bool RoboAvoidance::IsHeadingClear(const TArray<FVector2D>& Points, double HeadingRad,
	const FAvoidanceSettings& Settings)
{
	const double Cos = FMath::Cos(HeadingRad);
	const double Sin = FMath::Sin(HeadingRad);
	const double HalfCorridorM = Settings.RobotHalfWidthM + Settings.MarginM;

	for (const FVector2D& Point : Points)
	{
		// Rotate the point into a frame whose x axis lies along the heading.
		// Along is how far ahead of the robot it is in that direction; Across
		// is how far to the side.
		const double Along = Point.X * Cos + Point.Y * Sin;
		const double Across = -Point.X * Sin + Point.Y * Cos;

		// Ahead, within the lookahead, and inside the robot's width: the robot
		// would hit it driving this way. Points behind cannot block a forward move.
		if (Along > 0.0 && Along < Settings.LookaheadM && FMath::Abs(Across) < HalfCorridorM)
		{
			return false;
		}
	}

	return true;
}

TOptional<double> RoboAvoidance::ChooseHeading(const FLidarScan& Scan, double GoalBearingRad,
	const FAvoidanceSettings& Settings)
{
	if (Scan.RangesM.Num() == 0)
	{
		return TOptional<double>();
	}

	const TArray<FVector2D> Points = ScanToPoints(Scan);

	// Only headings the scan covers can be checked. A heading in the blind spot
	// behind a 270 degree lidar is unknown, and unknown is not clear.
	const double FieldMinRad = Scan.AngleMinRad;
	const double FieldMaxRad = Scan.AngleMinRad + Scan.AngleIncrementRad * (Scan.RangesM.Num() - 1);

	const double StepRad = FMath::DegreesToRadians(FMath::Max(Settings.CandidateStepDeg, 0.5));
	const int32 MaxSteps = FMath::CeilToInt(UE_DOUBLE_PI / StepRad);

	// Search outward from the goal's bearing, alternating sides, so the first
	// clear heading found is the one that deviates least. Left is tried before
	// right at each step, so ties favour left: arbitrary, but deterministic.
	for (int32 StepIndex = 0; StepIndex <= MaxSteps; ++StepIndex)
	{
		for (const double Side : { 1.0, -1.0 })
		{
			// The goal's own bearing is only tried once.
			if (StepIndex == 0 && Side < 0.0)
			{
				continue;
			}

			const double HeadingRad = RoboFrames::WrapRadians(GoalBearingRad + Side * StepIndex * StepRad);

			if (HeadingRad < FieldMinRad || HeadingRad > FieldMaxRad)
			{
				continue;
			}

			if (IsHeadingClear(Points, HeadingRad, Settings))
			{
				return HeadingRad;
			}
		}
	}

	return TOptional<double>();
}