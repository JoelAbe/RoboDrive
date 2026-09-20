#include "RoboDiffDrive.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags =EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double Tolerance = 1e-9;

	/** A plausible small robot: 40 cm between wheels, 10 cm wheel radius. */
	constexpr double TestWheelSeparation = 0.4;
	constexpr double TestWheelRadius = 0.1;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveStraightTest,"RoboDrive.RoboSim.DiffDrive.Straight", RoboTestFlags)

bool FDiffDriveStraightTest::RunTest(const FString& Parameters)
{
	// Driving straight: both wheels run at the body speed.
	const RoboDiffDrive::FWheelSpeeds Wheels = RoboDiffDrive::ToWheelSpeeds({ 1.0, 0.0 }, TestWheelSeparation);

	TestNearlyEqual(TEXT("Left wheel matches body speed"), Wheels.Left, 1.0, Tolerance);
	TestNearlyEqual(TEXT("Right wheel matches body speed"), Wheels.Right, 1.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveSpinInPlaceTest,"RoboDrive.RoboSim.DiffDrive.SpinInPlace", RoboTestFlags)

bool FDiffDriveSpinInPlaceTest::RunTest(const FString& Parameters)
{
	// Spinning left on the spot: wheels equal and opposite. A car cannot do this.
	const RoboDiffDrive::FWheelSpeeds Wheels = RoboDiffDrive::ToWheelSpeeds({ 0.0, 1.0 }, TestWheelSeparation);

	TestNearlyEqual(TEXT("Left wheel reverses"), Wheels.Left, -0.2, Tolerance);
	TestNearlyEqual(TEXT("Right wheel drives forward"), Wheels.Right, 0.2, Tolerance);
	TestNearlyEqual(TEXT("Wheels are equal and opposite"),Wheels.Left + Wheels.Right, 0.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveTurnDirectionTest, "RoboDrive.RoboSim.DiffDrive.TurnDirection", RoboTestFlags)

bool FDiffDriveTurnDirectionTest::RunTest(const FString& Parameters)
{
	// Positive omega is a left turn, so the right (outer) wheel runs faster.
	const RoboDiffDrive::FWheelSpeeds Left = RoboDiffDrive::ToWheelSpeeds({ 1.0, 0.5 }, TestWheelSeparation);
	TestTrue(TEXT("Turning left runs the right wheel faster"), Left.Right > Left.Left);

	const RoboDiffDrive::FWheelSpeeds Right =RoboDiffDrive::ToWheelSpeeds({ 1.0, -0.5 }, TestWheelSeparation);
	TestTrue(TEXT("Turning right runs the left wheel faster"), Right.Left > Right.Right);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveRoundTripTest, "RoboDrive.RoboSim.DiffDrive.RoundTrip", RoboTestFlags)

bool FDiffDriveRoundTripTest::RunTest(const FString& Parameters)
{
	// Forward and inverse kinematics must be exact inverses of each other.
	const RoboDiffDrive::FBodyVelocity Cases[] = {
		{ 0.0, 0.0 }, { 1.5, 0.0 }, { 0.0, -2.0 }, { 0.8, 0.35 }, { -0.4, 1.2 }
	};

	for (const RoboDiffDrive::FBodyVelocity& Original : Cases)
	{
		const RoboDiffDrive::FBodyVelocity Recovered = RoboDiffDrive::ToBodyVelocity(RoboDiffDrive::ToWheelSpeeds(Original, TestWheelSeparation), TestWheelSeparation);

		TestNearlyEqual(TEXT("Linear velocity survives the round trip"),Recovered.Linear, Original.Linear, Tolerance);
		TestNearlyEqual(TEXT("Angular velocity survives the round trip"),Recovered.Angular, Original.Angular, Tolerance);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveWheelRotationTest,"RoboDrive.RoboSim.DiffDrive.WheelRotation", RoboTestFlags)

bool FDiffDriveWheelRotationTest::RunTest(const FString& Parameters)
{
	// A 0.1 m wheel rolling at 1 m/s turns at 10 rad/s.
	TestNearlyEqual(TEXT("1 m/s on a 0.1 m wheel is 10 rad/s"),RoboDiffDrive::ToWheelAngularSpeed(1.0, TestWheelRadius), 10.0, Tolerance);

	TestNearlyEqual(TEXT("Wheel speed conversion round trips"),
		RoboDiffDrive::ToWheelLinearSpeed(RoboDiffDrive::ToWheelAngularSpeed(0.75, TestWheelRadius), TestWheelRadius),
		0.75, Tolerance);
	return true;
}

/**
 * The tests above restate the formulas we are testing, so a wrong formula
 * could be tested wrongly in the same way and still pass.
 *
 * This one derives the answer from something more fundamental: a wheel is a
 * point on a rigid body, so its velocity is the body velocity plus the
 * rotation's contribution at that point (v = v_body + omega x r). If the
 * kinematics are right, they agree with that.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiffDriveRigidBodyConsistencyTest,"RoboDrive.RoboSim.DiffDrive.RigidBodyConsistency", RoboTestFlags)

bool FDiffDriveRigidBodyConsistencyTest::RunTest(const FString& Parameters)
{
	const RoboDiffDrive::FBodyVelocity Body{ 0.8, 0.35 };
	const RoboDiffDrive::FWheelSpeeds Wheels =
		RoboDiffDrive::ToWheelSpeeds(Body, TestWheelSeparation);

	// REP-103: +Y is left, so the left wheel sits at +L/2 and the right at -L/2.
	const FVector BodyVelocity(Body.Linear, 0.0, 0.0);
	const FVector Spin(0.0, 0.0, Body.Angular);
	const FVector LeftOffset(0.0, 0.5 * TestWheelSeparation, 0.0);
	const FVector RightOffset(0.0, -0.5 * TestWheelSeparation, 0.0);

	const FVector LeftPointVelocity = BodyVelocity + FVector::CrossProduct(Spin, LeftOffset);
	const FVector RightPointVelocity = BodyVelocity + FVector::CrossProduct(Spin, RightOffset);

	TestNearlyEqual(TEXT("Left wheel matches rigid body motion at that point"),
		Wheels.Left, LeftPointVelocity.X, Tolerance);
	TestNearlyEqual(TEXT("Right wheel matches rigid body motion at that point"),
		Wheels.Right, RightPointVelocity.X, Tolerance);
	return true;
}

#endif