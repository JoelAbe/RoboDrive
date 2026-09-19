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


#endif