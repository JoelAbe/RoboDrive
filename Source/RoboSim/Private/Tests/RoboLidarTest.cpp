#include "RoboLidar.h"
#include "RoboTestCommon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarSweepGeometryTest,
	"RoboDrive.RoboSim.Lidar.SweepGeometry", RoboTest::Flags)

bool FLidarSweepGeometryTest::RunTest(const FString& Parameters)
{
	// A 180 degree sweep of 5 beams: edges at -90 and +90, middle straight ahead.
	constexpr int32 Beams = 5;
	const double Span = UE_DOUBLE_PI;

	TestNearlyEqual(TEXT("First beam is at the right edge"),
		RoboLidar::BeamAngleRad(0, Beams, Span), -UE_DOUBLE_HALF_PI, 1e-12);
	TestNearlyEqual(TEXT("Middle beam points forward"),
		RoboLidar::BeamAngleRad(2, Beams, Span), 0.0, 1e-12);
	TestNearlyEqual(TEXT("Last beam is at the left edge"),
		RoboLidar::BeamAngleRad(4, Beams, Span), UE_DOUBLE_HALF_PI, 1e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarBeamDirectionTest,
	"RoboDrive.RoboSim.Lidar.BeamDirection", RoboTest::Flags)

bool FLidarBeamDirectionTest::RunTest(const FString& Parameters)
{
	// Positive angles are to the left, matching REP-103 everywhere else.
	TestTrue(TEXT("Zero points forward"),
		RoboLidar::BeamDirection(0.0).Equals(FVector(1.0, 0.0, 0.0), 1e-9));
	TestTrue(TEXT("Positive 90 degrees points left"),
		RoboLidar::BeamDirection(UE_DOUBLE_HALF_PI).Equals(FVector(0.0, 1.0, 0.0), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarNoReturnTest,
	"RoboDrive.RoboSim.Lidar.NoReturn", RoboTest::Flags)

bool FLidarNoReturnTest::RunTest(const FString& Parameters)
{
	// No return must be distinguishable from a measurement at maximum range.
	// Code that confuses the two drives into glass doors.
	TestFalse(TEXT("No return is not a reading"), RoboLidar::HasReturn(RoboLidar::NoReturn()));
	TestTrue(TEXT("A long reading is still a reading"), RoboLidar::HasReturn(11.9));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS