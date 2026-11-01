#include "RoboAvoidance.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FAvoidanceSettings AvoidanceDefaults;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceOpenSpaceTest,
	"RoboDrive.RoboAutonomy.Avoidance.OpenSpace", RoboTest::Flags)

bool FAvoidanceOpenSpaceTest::RunTest(const FString& Parameters)
{
	// Nothing in the way: go straight for the goal, with no detour.
	const TOptional<double> Heading = RoboAvoidance::ChooseHeading(RoboTest::MakeScan({}), 0.3, AvoidanceDefaults);

	TestTrue(TEXT("A heading is found"), Heading.IsSet());
	TestNearlyEqual(TEXT("It is the goal's own bearing"), Heading.Get(0.0), 0.3, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceDeviatesTest,
	"RoboDrive.RoboAutonomy.Avoidance.Deviates", RoboTest::Flags)

bool FAvoidanceDeviatesTest::RunTest(const FString& Parameters)
{
	// An obstacle dead ahead, 1 m away, with the goal beyond it.
	const FLidarScan Scan = RoboTest::MakeScan({ FVector2D(1.0, 0.0) });
	const TOptional<double> Heading = RoboAvoidance::ChooseHeading(Scan, 0.0, AvoidanceDefaults);

	TestTrue(TEXT("A way round is found"), Heading.IsSet());
	TestTrue(TEXT("It is not straight ahead"), FMath::Abs(Heading.Get(0.0)) > 0.01);
	TestTrue(TEXT("And it really is clear"),
		RoboAvoidance::IsHeadingClear(RoboAvoidance::ScanToPoints(Scan), Heading.Get(0.0), AvoidanceDefaults));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceSmallestDeviationTest,
	"RoboDrive.RoboAutonomy.Avoidance.SmallestDeviation", RoboTest::Flags)

bool FAvoidanceSmallestDeviationTest::RunTest(const FString& Parameters)
{
	// An obstacle slightly LEFT of straight ahead. Going right needs a smaller
	// turn to clear it than going left, so despite left being tried first at
	// each step, the search must settle on right.
	const FLidarScan Scan = RoboTest::MakeScan({ FVector2D(1.0, 0.2) });
	const TOptional<double> Heading = RoboAvoidance::ChooseHeading(Scan, 0.0, AvoidanceDefaults);

	TestTrue(TEXT("A way round is found"), Heading.IsSet());
	TestTrue(TEXT("It passes on the side needing less turn: right"), Heading.Get(0.0) < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceRobotWidthTest,
	"RoboDrive.RoboAutonomy.Avoidance.RobotWidth", RoboTest::Flags)

bool FAvoidanceRobotWidthTest::RunTest(const FString& Parameters)
{
	// The corridor is the robot's half-width plus margin: 0.3 + 0.15 = 0.45 m.
	// A point 0.4 m to the side would clip the robot; one 0.6 m to the side
	// would not. A check that treated the robot as a point would miss this.
	TestFalse(TEXT("A point 0.4 m off the line blocks it"),
		RoboAvoidance::IsHeadingClear({ FVector2D(1.0, 0.4) }, 0.0, AvoidanceDefaults));
	TestTrue(TEXT("A point 0.6 m off the line does not"),
		RoboAvoidance::IsHeadingClear({ FVector2D(1.0, 0.6) }, 0.0, AvoidanceDefaults));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceLookaheadTest,
	"RoboDrive.RoboAutonomy.Avoidance.Lookahead", RoboTest::Flags)

bool FAvoidanceLookaheadTest::RunTest(const FString& Parameters)
{
	// Behind the robot, and beyond the 1.5 m lookahead, neither blocks.
	TestTrue(TEXT("A point behind does not block a forward heading"),
		RoboAvoidance::IsHeadingClear({ FVector2D(-0.5, 0.0) }, 0.0, AvoidanceDefaults));
	TestTrue(TEXT("A point beyond the lookahead does not block yet"),
		RoboAvoidance::IsHeadingClear({ FVector2D(3.0, 0.0) }, 0.0, AvoidanceDefaults));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceNoReturnTest,
	"RoboDrive.RoboAutonomy.Avoidance.NoReturn", RoboTest::Flags)

bool FAvoidanceNoReturnTest::RunTest(const FString& Parameters)
{
	// A scan of nothing but no-returns produces no points, so nothing blocks.
	// This is the simulation-only assumption, made deliberately and pinned here.
	TestEqual(TEXT("No-returns produce no points"),
		RoboAvoidance::ScanToPoints(RoboTest::MakeScan({})).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceBoxedInTest,
	"RoboDrive.RoboAutonomy.Avoidance.BoxedIn", RoboTest::Flags)

bool FAvoidanceBoxedInTest::RunTest(const FString& Parameters)
{
	// A ring of points 0.8 m away on every visible bearing: nowhere to go.
	TArray<FVector2D> Ring;
	for (int32 Degrees = -135; Degrees <= 135; ++Degrees)
	{
		const double Rad = FMath::DegreesToRadians(static_cast<double>(Degrees));
		Ring.Emplace(0.8 * FMath::Cos(Rad), 0.8 * FMath::Sin(Rad));
	}

	TestFalse(TEXT("No heading is offered when everything is blocked"),
		RoboAvoidance::ChooseHeading(RoboTest::MakeScan(Ring), 0.0, AvoidanceDefaults).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceBlindSpotTest,
	"RoboDrive.RoboAutonomy.Avoidance.BlindSpot", RoboTest::Flags)

bool FAvoidanceBlindSpotTest::RunTest(const FString& Parameters)
{
	// The goal is directly behind, in the lidar's blind spot. Open space or
	// not, the robot cannot know that heading is clear, so it must pick one it
	// can see. The robot will turn toward the goal over successive decisions.
	const TOptional<double> Heading =
		RoboAvoidance::ChooseHeading(RoboTest::MakeScan({}), UE_DOUBLE_PI, AvoidanceDefaults);

	TestTrue(TEXT("A heading is found"), Heading.IsSet());
	TestTrue(TEXT("It is inside the field of view"),
		FMath::Abs(Heading.Get(0.0)) <= FMath::DegreesToRadians(135.0) + RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceFrameRoundTripTest,
	"RoboDrive.RoboAutonomy.Avoidance.FrameRoundTrip", RoboTest::Flags)

bool FAvoidanceFrameRoundTripTest::RunTest(const FString& Parameters)
{
	// The robot believes it is at (2, 3), turned a quarter left to face +Y.
	FPose2D Pose;
	Pose.X = 2.0;
	Pose.Y = 3.0;
	Pose.YawRad = UE_DOUBLE_HALF_PI;

	TestTrue(TEXT("1 m ahead of it is 1 m further along +Y"),
		RoboAvoidance::RobotToOdometry(FVector2D(1.0, 0.0), Pose).Equals(FVector2D(2.0, 4.0), 1e-9));
	TestTrue(TEXT("1 m to its left is towards -X"),
		RoboAvoidance::RobotToOdometry(FVector2D(0.0, 1.0), Pose).Equals(FVector2D(1.0, 3.0), 1e-9));

	const FVector2D Point(0.7, -1.3);
	TestTrue(TEXT("There and back again is where it started"),
		RoboAvoidance::OdometryToRobot(RoboAvoidance::RobotToOdometry(Point, Pose), Pose).Equals(Point, 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidancePersistentKeptTest,
	"RoboDrive.RoboAutonomy.Avoidance.PersistentKept", RoboTest::Flags)

bool FAvoidancePersistentKeptTest::RunTest(const FString& Parameters)
{
	// A post seen in all three scans, a couple of centimetres apart each time,
	// as lidar noise would put it.
	const TArray<FVector2D> Now = { FVector2D(2.0, 0.0) };
	const TArray<TArray<FVector2D>> Earlier = { { FVector2D(2.02, 0.01) }, { FVector2D(1.98, -0.01) } };

	TestEqual(TEXT("Seen in every scan: an obstacle"),
		RoboAvoidance::KeepPersistent(Now, Earlier, 0.15).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidanceFlickerDroppedTest,
	"RoboDrive.RoboAutonomy.Avoidance.FlickerDropped", RoboTest::Flags)

bool FAvoidanceFlickerDroppedTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Now = { FVector2D(1.0, 0.5) };

	// Never seen before: a fog echo.
	const TArray<TArray<FVector2D>> NothingBefore = { TArray<FVector2D>(), TArray<FVector2D>() };
	TestEqual(TEXT("Seen only now: ignored"),
		RoboAvoidance::KeepPersistent(Now, NothingBefore, 0.15).Num(), 0);

	// Seen in the last scan but not the one before: two echoes that happened to
	// land together. Every earlier scan must agree, not just one.
	const TArray<TArray<FVector2D>> OnlyLastTime = { { FVector2D(3.0, -2.0) }, { FVector2D(1.0, 0.5) } };
	TestEqual(TEXT("Seen in only some earlier scans: ignored"),
		RoboAvoidance::KeepPersistent(Now, OnlyLastTime, 0.15).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidancePersistenceRadiusTest,
	"RoboDrive.RoboAutonomy.Avoidance.PersistenceRadius", RoboTest::Flags)

bool FAvoidancePersistenceRadiusTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Now = { FVector2D(2.0, 0.0) };
	const TArray<TArray<FVector2D>> NearBefore = { { FVector2D(2.1, 0.0) } };
	const TArray<TArray<FVector2D>> FarBefore = { { FVector2D(2.2, 0.0) } };

	TestEqual(TEXT("0.1 m away is the same thing"),
		RoboAvoidance::KeepPersistent(Now, NearBefore, 0.15).Num(), 1);
	TestEqual(TEXT("0.2 m away is not"),
		RoboAvoidance::KeepPersistent(Now, FarBefore, 0.15).Num(), 0);

	// No earlier scans to compare with: the filter is off, and everything counts.
	TestEqual(TEXT("With nothing to compare against, everything is kept"),
		RoboAvoidance::KeepPersistent(Now, TArray<TArray<FVector2D>>(), 0.15).Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS