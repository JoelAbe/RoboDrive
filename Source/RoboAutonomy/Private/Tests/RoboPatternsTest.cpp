#include "RoboPatterns.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags =EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double Tolerance = 1e-9;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPatternsCircleTest, "RoboDrive.RoboAutonomy.Patterns.Circle", RoboTestFlags)

bool FPatternsCircleTest::RunTest(const FString& Parameters)
{
	// 1 m/s around a 2 m circle is half a radian per second.
	const RoboDiffDrive::FBodyVelocity Left = RoboPatterns::CircleCommand(1.0, 2.0);
	TestNearlyEqual(TEXT("Speed is passed through"), Left.Linear, 1.0, Tolerance);
	TestNearlyEqual(TEXT("omega = v / R"), Left.Angular, 0.5, Tolerance);

	// Negative radius curves the other way.
	const RoboDiffDrive::FBodyVelocity Right = RoboPatterns::CircleCommand(1.0, -2.0);
	TestTrue(TEXT("Negative radius turns right"), Right.Angular < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPatternsStraightTest, "RoboDrive.RoboAutonomy.Patterns.Straight", RoboTestFlags)

bool FPatternsStraightTest::RunTest(const FString& Parameters)
{
	// Zero radius means straight, not an infinite turn rate.
	const RoboDiffDrive::FBodyVelocity Straight = RoboPatterns::CircleCommand(1.0, 0.0);
	TestNearlyEqual(TEXT("No turn"), Straight.Angular, 0.0, Tolerance);
	TestTrue(TEXT("Angular velocity is finite"), FMath::IsFinite(Straight.Angular));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS