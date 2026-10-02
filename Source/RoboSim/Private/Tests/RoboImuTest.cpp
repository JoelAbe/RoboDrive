#include "RoboImu.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags =
		EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double G = 9.80665;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuAtRestTest,
	"RoboDrive.RoboSim.Imu.AtRest", RoboTestFlags)

bool FImuAtRestTest::RunTest(const FString& Parameters)
{
	// Stationary and level: reads one g upward, not zero. This is the detail
	// that separates proper acceleration from rate of change of velocity.
	const FVector Reading = RoboImu::ProperAcceleration(FVector::ZeroVector, FQuat::Identity);

	TestNearlyEqual(TEXT("Reads +1 g upward at rest"), Reading.Z, G, 1e-9);
	TestNearlyEqual(TEXT("Nothing along X"), Reading.X, 0.0, 1e-9);
	TestNearlyEqual(TEXT("Nothing along Y"), Reading.Y, 0.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuFreeFallTest,
	"RoboDrive.RoboSim.Imu.FreeFall", RoboTestFlags)

bool FImuFreeFallTest::RunTest(const FString& Parameters)
{
	// Falling freely: the test mass is weightless, so the sensor reads zero
	// while accelerating hard. The opposite of intuition, and correct.
	const FVector Reading = RoboImu::ProperAcceleration(RoboImu::GravityMS2, FQuat::Identity);

	TestTrue(TEXT("A falling accelerometer reads nothing"),
		Reading.IsNearlyZero(1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuTiltTest,
	"RoboDrive.RoboSim.Imu.Tilt", RoboTestFlags)

bool FImuTiltTest::RunTest(const FString& Parameters)
{
	// Pitched nose-down by 90 degrees, gravity moves onto the body's X axis.
	// This is how an IMU knows which way is down when it is not accelerating.
	const FQuat PitchedDown(FVector(0.0, 1.0, 0.0), UE_DOUBLE_HALF_PI);
	const FVector Reading = RoboImu::ProperAcceleration(FVector::ZeroVector, PitchedDown);

	TestNearlyEqual(TEXT("Gravity now appears along the body X axis"),
		FMath::Abs(Reading.X), G, 1e-6);
	TestNearlyEqual(TEXT("And no longer along body Z"), Reading.Z, 0.0, 1e-6);
	TestNearlyEqual(TEXT("Magnitude is still one g"), Reading.Size(), G, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuAccelerationTest,
	"RoboDrive.RoboSim.Imu.Acceleration", RoboTestFlags)

bool FImuAccelerationTest::RunTest(const FString& Parameters)
{
	// Accelerating forward at 2 m/s^2 while level: forward acceleration on X,
	// gravity still on Z. Gravity and real acceleration are summed, which is
	// why an IMU cannot tell a tilt from an acceleration.
	const FVector Reading =
		RoboImu::ProperAcceleration(FVector(2.0, 0.0, 0.0), FQuat::Identity);

	TestNearlyEqual(TEXT("Forward acceleration appears on X"), Reading.X, 2.0, 1e-9);
	TestNearlyEqual(TEXT("Gravity is still on Z"), Reading.Z, G, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuBodyFrameTest,
	"RoboDrive.RoboSim.Imu.BodyFrame", RoboTestFlags)

bool FImuBodyFrameTest::RunTest(const FString& Parameters)
{
	// Yawed 90 degrees left, so the body's X axis points along world Y.
	// Acceleration along world Y should read as forward in the body frame.
	const FQuat YawedLeft(FVector(0.0, 0.0, 1.0), UE_DOUBLE_HALF_PI);
	const FVector Reading =
		RoboImu::ProperAcceleration(FVector(0.0, 3.0, 0.0), YawedLeft);

	TestNearlyEqual(TEXT("World +Y is forward for a robot yawed 90 degrees left"),
		Reading.X, 3.0, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuGyroBodyFrameTest,
	"RoboDrive.RoboSim.Imu.GyroBodyFrame", RoboTestFlags)

bool FImuGyroBodyFrameTest::RunTest(const FString& Parameters)
{
	const FQuat YawedLeft(FVector(0.0, 0.0, 1.0), UE_DOUBLE_HALF_PI);

	// Yaw rate is about the same axis the robot turned about, so it is
	// unchanged by that rotation.
	const FVector Yawing = RoboImu::BodyAngularVelocity(FVector(0.0, 0.0, 1.0), YawedLeft);
	TestNearlyEqual(TEXT("Yaw rate is unaffected by yaw"), Yawing.Z, 1.0, 1e-9);

	// A roll about world X is seen as a pitch by a robot facing world +Y.
	const FVector Rolling = RoboImu::BodyAngularVelocity(FVector(1.0, 0.0, 0.0), YawedLeft);
	TestNearlyEqual(TEXT("World roll is body pitch when yawed 90 degrees"),
		Rolling.Y, -1.0, 1e-6);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS