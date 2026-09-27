#include "RoboOdometry.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double Tolerance = 1e-9;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryStraightTest, "RoboDrive.RoboSim.Odometry.Straight", RoboTestFlags)

bool FOdometryStraightTest::RunTest(const FString& Parameters)
{
	// Facing +X, 1 m/s for one second.
	const FPose2D Result = RoboOdometry::Integrate(FPose2D{}, { 1.0, 0.0 }, 1.0);

	TestNearlyEqual(TEXT("Moved one metre forward"), Result.X, 1.0, Tolerance);
	TestNearlyEqual(TEXT("No sideways movement"), Result.Y, 0.0, Tolerance);
	TestNearlyEqual(TEXT("Heading unchanged"), Result.YawRad, 0.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryRespectsHeadingTest, "RoboDrive.RoboSim.Odometry.RespectsHeading", RoboTestFlags)

bool FOdometryRespectsHeadingTest::RunTest(const FString& Parameters)
{
	// Facing 90 degrees left, forward motion must go to +Y. This pins the
	// REP-103 convention into the odometry, not just the frames header.
	FPose2D Start;
	Start.YawRad = UE_DOUBLE_HALF_PI;

	const FPose2D Result = RoboOdometry::Integrate(Start, { 1.0, 0.0 }, 1.0);

	TestNearlyEqual(TEXT("Forward at 90 degrees moves to +Y (left)"), Result.Y, 1.0, 1e-9);
	TestNearlyEqual(TEXT("No movement along X"), Result.X, 0.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometrySpinInPlaceTest, "RoboDrive.RoboSim.Odometry.SpinInPlace", RoboTestFlags)

bool FOdometrySpinInPlaceTest::RunTest(const FString& Parameters)
{
	// Turning without driving must not move the position at all.
	const FPose2D Result = RoboOdometry::Integrate(FPose2D{}, { 0.0, 1.0 }, 1.0);

	TestNearlyEqual(TEXT("Did not move in X"), Result.X, 0.0, Tolerance);
	TestNearlyEqual(TEXT("Did not move in Y"), Result.Y, 0.0, Tolerance);
	TestNearlyEqual(TEXT("Turned one radian"), Result.YawRad, 1.0, Tolerance);
	return true;
}

/**
 * The test that justifies arc integration.
 *
 * Constant v and omega trace a circle, so after one full period the robot is
 * back where it started. The arc form gets this exactly right at any step
 * size. Straight-line stepping would cut every corner and land short, inside
 * the circle, by an amount that grows with the step.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryCircleClosesTest, "RoboDrive.RoboSim.Odometry.CircleCloses", RoboTestFlags)

bool FOdometryCircleClosesTest::RunTest(const FString& Parameters)
{
	const RoboDiffDrive::FBodyVelocity Velocity{ 1.0, 0.5 };   // 2 m radius
	const double PeriodSeconds = 2.0 * UE_DOUBLE_PI / Velocity.Angular;

	// Deliberately coarse: eight steps for the whole circle. An approximate
	// integrator would be visibly wrong here; an exact one does not care.
	constexpr int32 Steps = 8;
	const double StepSeconds = PeriodSeconds / Steps;

	FPose2D Pose;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Pose = RoboOdometry::Integrate(Pose, Velocity, StepSeconds);
	}

	TestNearlyEqual(TEXT("Back at the starting X"), Pose.X, 0.0, 1e-9);
	TestNearlyEqual(TEXT("Back at the starting Y"), Pose.Y, 0.0, 1e-9);
	TestNearlyEqual(TEXT("Back at the starting heading"), Pose.YawRad, 0.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryArcGeometryTest, "RoboDrive.RoboSim.Odometry.ArcGeometry", RoboTestFlags)

bool FOdometryArcGeometryTest::RunTest(const FString& Parameters)
{
	// A quarter circle of radius 2 m, starting at the origin facing +X, must
	// end at (2, 2) facing 90 degrees left: the centre of the turn is at
	// (0, 2) and the robot travels a quarter of the way around it.
	const RoboDiffDrive::FBodyVelocity Velocity{ 1.0, 0.5 };
	const double QuarterPeriod = 0.5 * UE_DOUBLE_PI / Velocity.Angular;

	const FPose2D Result = RoboOdometry::Integrate(FPose2D{}, Velocity, QuarterPeriod);

	TestNearlyEqual(TEXT("Ends one radius forward"), Result.X, 2.0, 1e-9);
	TestNearlyEqual(TEXT("Ends one radius to the left"), Result.Y, 2.0, 1e-9);
	TestNearlyEqual(TEXT("Ends facing 90 degrees left"), Result.YawRad, UE_DOUBLE_HALF_PI, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryWrapsHeadingTest, "RoboDrive.RoboSim.Odometry.WrapsHeading", RoboTestFlags)

bool FOdometryWrapsHeadingTest::RunTest(const FString& Parameters)
{
	// Turning past half a turn wraps rather than accumulating without bound.
	const FPose2D Result = RoboOdometry::Integrate(FPose2D{}, { 0.0, 4.0 }, 1.0);

	TestTrue(TEXT("Heading stays within (-pi, pi]"), Result.YawRad > -UE_DOUBLE_PI && Result.YawRad <= UE_DOUBLE_PI);
	TestNearlyEqual(TEXT("4 radians wraps to 4 - 2pi"), Result.YawRad, 4.0 - UE_DOUBLE_TWO_PI, 1e-14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOdometryErrorTest, "RoboDrive.RoboSim.Odometry.Error", RoboTestFlags)

bool FOdometryErrorTest::RunTest(const FString& Parameters)
{
	FPose2D Truth;
	FPose2D Estimate;
	Estimate.X = 3.0;
	Estimate.Y = 4.0;

	TestNearlyEqual(TEXT("Position error is the straight-line distance"), RoboOdometry::PositionErrorM(Estimate, Truth), 5.0, Tolerance);

	// Headings either side of the wrap are close, not nearly a full turn apart.
	Truth.YawRad = FMath::DegreesToRadians(179.0);
	Estimate.YawRad = FMath::DegreesToRadians(-179.0);

	TestNearlyEqual(TEXT("Heading error wraps the short way"),
		FMath::Abs(RoboOdometry::HeadingErrorRad(Estimate, Truth)),
		FMath::DegreesToRadians(2.0), 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS