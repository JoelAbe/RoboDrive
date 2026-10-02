#include "RoboFrames.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace 
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double Tolerance = 1e-9;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesPositionTest, "RoboDrive.RoboSim.Frames.Position", RoboTestFlags)

bool FRoboFramesPositionTest::RunTest(const FString& Parameters)
{
	// 1 m forward, 2 m to the right, 3 m up, in Unreal units.
	const FVector UePoint(100.0, 200.0, 300.0);
	const FVector RosPoint = RoboFrames::PositionToRos(UePoint);

	TestNearlyEqual(TEXT("Forward stays forward"), RosPoint.X, 1.0, Tolerance);
	TestNearlyEqual(TEXT("Right becomes negative Y"), RosPoint.Y, -2.0, Tolerance);
	TestNearlyEqual(TEXT("Up stays up"), RosPoint.Z, 3.0, Tolerance);

	TestTrue(TEXT("Round trip returns the original point"),RoboFrames::PositionToUe(RosPoint).Equals(UePoint, Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesYawTest,"RoboDrive.RoboSim.Frames.Yaw", RoboTestFlags)

bool FRoboFramesYawTest::RunTest(const FString& Parameters)
{
	// A 90-degree right turn: +90 in Unreal, -pi/2 in REP-103.
	TestNearlyEqual(TEXT("Right turn is negative yaw"),RoboFrames::YawToRos(90.0), -UE_DOUBLE_HALF_PI, Tolerance);

	TestNearlyEqual(TEXT("Round trip returns the original yaw"),RoboFrames::YawToUe(RoboFrames::YawToRos(37.5)), 37.5, Tolerance);
	return true;
}

/**
 * The tests above check the functions against numbers we worked out by hand.
 * If the reasoning about Unreal's conventions were wrong, they'd pass anyway.
 *
 * This one asks Unreal itself: which way does "forward" point at a given yaw?
 * It then checks the position rule, and the yaw rule agrees about the answer.
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesHeadingConsistencyTest,"RoboDrive.RoboSim.Frames.HeadingConsistency", RoboTestFlags)

bool FRoboFramesHeadingConsistencyTest::RunTest(const FString& Parameters)
{
	for (const double UeYaw : { -170.0, -90.0, -25.0, 0.0, 30.0, 90.0, 135.0 })
	{
		// Unreal's own answer: the forward direction at this yaw.
		const FVector UeForward = FRotator(0.0, UeYaw, 0.0).Vector();

		// Path 1: convert that direction as a vector.
		const FVector ViaPosition = RoboFrames::PositionToRos(UeForward).GetSafeNormal();

		// Path 2: convert the yaw, then build the direction in REP-103.
		const double RosYaw = RoboFrames::YawToRos(UeYaw);
		const FVector ViaYaw(FMath::Cos(RosYaw), FMath::Sin(RosYaw), 0.0);

		TestTrue(FString::Printf(TEXT("Heading agrees at %.0f degrees"), UeYaw),ViaPosition.Equals(ViaYaw, 1e-9));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesAngularVelocityTest, "RoboDrive.RoboSim.Frames.AngularVelocity", RoboTestFlags)

bool FRoboFramesAngularVelocityTest::RunTest(const FString& Parameters)
{
	// Rotation about Z: axis survives the mirror, spin reverses, so it flips.
	const FVector Yaw = RoboFrames::AngularVelocityToRos(FVector(0.0, 0.0, 180.0));
	TestNearlyEqual(TEXT("Yaw rate flips sign and becomes radians"), Yaw.Z, -UE_DOUBLE_PI, Tolerance);

	// Rotation about Y: axis and spin both reverse, so the sign survives.
	const FVector Pitch = RoboFrames::AngularVelocityToRos(FVector(0.0, 90.0, 0.0));
	TestNearlyEqual(TEXT("Pitch rate keeps its sign"), Pitch.Y, UE_DOUBLE_HALF_PI, Tolerance);

	const FVector Original(15.0, -40.0, 73.0);
	TestTrue(TEXT("Angular velocity survives a round trip"),
		RoboFrames::AngularVelocityToUe(RoboFrames::AngularVelocityToRos(Original)).Equals(Original, 1e-9));
	return true;
}

/**
 * The yaw rule written in Phase 0 is the special case of the angular rule for
 * rotation about Z. If the two ever disagreed, a robot's heading and its
 * measured turn rate would drift apart, which is nearly impossible to spot by
 * eye and ruinous for odometry.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesYawRateConsistencyTest, "RoboDrive.RoboSim.Frames.YawRateConsistency", RoboTestFlags)

bool FRoboFramesYawRateConsistencyTest::RunTest(const FString& Parameters)
{
	constexpr double UeYawRateDegS = 45.0;
	constexpr double DeltaSeconds = 0.01;

	// Turning the heading conversion into a rate, by differencing.
	const double YawChangeRos =
		RoboFrames::YawToRos(30.0 + UeYawRateDegS * DeltaSeconds) - RoboFrames::YawToRos(30.0);

	const double RateFromAngularRos =
		RoboFrames::AngularVelocityToRos(FVector(0.0, 0.0, UeYawRateDegS)).Z * DeltaSeconds;

	TestNearlyEqual(TEXT("Yaw conversion and angular velocity conversion agree"),
		YawChangeRos, RateFromAngularRos, 1e-12);
	return true;
}

/**
 * The same invariant as the heading test from Phase 0, now for full 3D
 * rotation: rotating a point and converting must equal converting both and
 * then rotating. This is what catches an inconsistent handedness fix.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesRotationConsistencyTest, "RoboDrive.RoboSim.Frames.RotationConsistency", RoboTestFlags)

bool FRoboFramesRotationConsistencyTest::RunTest(const FString& Parameters)
{
	// Nothing axis-aligned, so a sign error anywhere shows up.
	const FQuat UeRotation = FRotator(21.0, 143.0, -62.0).Quaternion();
	const FVector UePoint(137.0, -48.0, 92.0);

	const FVector RotatedThenConverted =
		RoboFrames::PositionToRos(UeRotation.RotateVector(UePoint));

	const FVector ConvertedThenRotated =
		RoboFrames::RotationToRos(UeRotation).RotateVector(RoboFrames::PositionToRos(UePoint));

	TestTrue(TEXT("Rotate-then-convert equals convert-then-rotate"),
		RotatedThenConverted.Equals(ConvertedThenRotated, 1e-9));

	TestTrue(TEXT("Rotation survives a round trip"),
		RoboFrames::RotationToUe(RoboFrames::RotationToRos(UeRotation))
			.Equals(UeRotation, 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesRotationMatchesYawTest, "RoboDrive.RoboSim.Frames.RotationMatchesYaw", RoboTestFlags)

bool FRoboFramesRotationMatchesYawTest::RunTest(const FString& Parameters)
{
	// A yaw-only rotation converted as a quaternion must give the same heading
	// as converting the yaw angle directly.
	for (const double UeYaw : { -150.0, -35.0, 0.0, 47.0, 120.0 })
	{
		const FQuat RosRotation = RoboFrames::RotationToRos(FRotator(0.0, UeYaw, 0.0).Quaternion());
		const FVector RosForward = RosRotation.RotateVector(FVector::ForwardVector);
		const double HeadingFromQuat = FMath::Atan2(RosForward.Y, RosForward.X);

		TestNearlyEqual(FString::Printf(TEXT("Quaternion heading matches YawToRos at %.0f"), UeYaw),
			RoboFrames::WrapRadians(HeadingFromQuat - RoboFrames::YawToRos(UeYaw)), 0.0, 1e-9);
	}
	return true;
}

/**
 * For a point on a rigid body, v = omega x r. That relationship only survives
 * the conversion if angular velocity flips as a pseudovector while position
 * and linear velocity flip as ordinary vectors. It is the cleanest check that
 * the two rules belong together.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoboFramesAngularVelocityConsistencyTest, "RoboDrive.RoboSim.Frames.AngularVelocityConsistency", RoboTestFlags)

bool FRoboFramesAngularVelocityConsistencyTest::RunTest(const FString& Parameters)
{
	const FVector UeOmegaDegS(15.0, -40.0, 73.0);
	const FVector UeRadiusUU(120.0, 65.0, -30.0);

	const FVector UeOmegaRadS = UeOmegaDegS * UE_DOUBLE_PI / 180.0;
	const FVector UeVelocityUU = FVector::CrossProduct(UeOmegaRadS, UeRadiusUU);

	const FVector Expected = RoboFrames::LinearVelocityToRos(UeVelocityUU);
	const FVector Actual = FVector::CrossProduct(
		RoboFrames::AngularVelocityToRos(UeOmegaDegS),
		RoboFrames::PositionToRos(UeRadiusUU));

	TestTrue(TEXT("v = omega x r survives the handedness flip"),
		Expected.Equals(Actual, 1e-9));
	return true;
}

#endif