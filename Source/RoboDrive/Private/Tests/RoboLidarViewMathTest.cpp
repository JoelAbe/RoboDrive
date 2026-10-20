#include "RoboLidarViewMath.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LidarViewNear(const FVector2D& Actual, const FVector2D& Expected)
	{
		return Actual.Equals(Expected, 1e-9);
	}

	/** The robot drawn at (100, 200) px, 10 px to the metre. */
	const FVector2D LidarViewRobotPx(100.0, 200.0);
	constexpr double LidarViewScale = 10.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarViewForwardIsUpTest,
	"RoboDrive.Operator.LidarView.ForwardIsUp", RoboTest::Flags)

bool FLidarViewForwardIsUpTest::RunTest(const FString& Parameters)
{
	// 1 m ahead is 10 px up the screen, which is a SMALLER screen Y.
	TestTrue(TEXT("Ahead is drawn above the robot"),
		LidarViewNear(RoboLidarViewMath::RobotToView(FVector2D(1.0, 0.0), LidarViewRobotPx, LidarViewScale),
			FVector2D(100.0, 190.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarViewLeftIsLeftTest,
	"RoboDrive.Operator.LidarView.LeftIsLeft", RoboTest::Flags)

bool FLidarViewLeftIsLeftTest::RunTest(const FString& Parameters)
{
	// 1 m to the robot's left is 10 px to the left on screen. Get this sign
	// wrong and the view is a mirror image: the robot appears to pass posts on
	// the side it does not.
	TestTrue(TEXT("Left of the robot is drawn to its left"),
		LidarViewNear(RoboLidarViewMath::RobotToView(FVector2D(0.0, 1.0), LidarViewRobotPx, LidarViewScale),
			FVector2D(90.0, 200.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarViewAlongHeadingTest,
	"RoboDrive.Operator.LidarView.AlongHeading", RoboTest::Flags)

bool FLidarViewAlongHeadingTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Heading 0 is straight ahead"),
		LidarViewNear(RoboLidarViewMath::AlongHeadingM(0.0, 2.0), FVector2D(2.0, 0.0)));
	TestTrue(TEXT("A quarter turn positive is to the left"),
		RoboLidarViewMath::AlongHeadingM(UE_DOUBLE_HALF_PI, 2.0).Equals(FVector2D(0.0, 2.0), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarViewCorridorAheadTest,
	"RoboDrive.Operator.LidarView.CorridorAhead", RoboTest::Flags)

bool FLidarViewCorridorAheadTest::RunTest(const FString& Parameters)
{
	// The avoidance defaults: 1.5 m ahead, 0.3 m half-width plus 0.15 m margin.
	const TArray<FVector2D> Corners = RoboLidarViewMath::CorridorCornersM(0.0, 1.5, 0.45);

	TestEqual(TEXT("Four corners"), Corners.Num(), 4);
	TestTrue(TEXT("Near left"), LidarViewNear(Corners[0], FVector2D(0.0, 0.45)));
	TestTrue(TEXT("Far left"), LidarViewNear(Corners[1], FVector2D(1.5, 0.45)));
	TestTrue(TEXT("Far right"), LidarViewNear(Corners[2], FVector2D(1.5, -0.45)));
	TestTrue(TEXT("Near right"), LidarViewNear(Corners[3], FVector2D(0.0, -0.45)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarViewCorridorTurnedTest,
	"RoboDrive.Operator.LidarView.CorridorTurned", RoboTest::Flags)

bool FLidarViewCorridorTurnedTest::RunTest(const FString& Parameters)
{
	// Turned a quarter turn left, the corridor runs along +Y, and its own
	// "left" side is now towards -X: behind the robot's original facing.
	const TArray<FVector2D> Corners = RoboLidarViewMath::CorridorCornersM(UE_DOUBLE_HALF_PI, 1.5, 0.45);

	TestTrue(TEXT("Far left"), Corners[1].Equals(FVector2D(-0.45, 1.5), 1e-9));
	TestTrue(TEXT("Far right"), Corners[2].Equals(FVector2D(0.45, 1.5), 1e-9));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS