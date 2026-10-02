#include "RoboTestCommon.h"
#include "RoboWheel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double Stiffness = 1500.0;
	constexpr double Damping = 120.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuspensionForceTest, "RoboDrive.RoboSim.Suspension.Force", RoboTest::Flags)

bool FSuspensionForceTest::RunTest(const FString& Parameters)
{
	TestNearlyEqual(TEXT("Fully extended suspension pushes nothing"),
		RoboSuspension::ComputeForceN(0.0, 0.0, Stiffness, Damping), 0.0, RoboTest::Tolerance);

	// 3.2 cm of sag on a 1500 N/m spring carries roughly a 5 kg corner.
	TestNearlyEqual(TEXT("Spring force is stiffness times compression"),
		RoboSuspension::ComputeForceN(0.032, 0.0, Stiffness, Damping), 48.0, RoboTest::Tolerance);

	TestTrue(TEXT("Compressing quickly adds damping force"),
		RoboSuspension::ComputeForceN(0.032, 0.1, Stiffness, Damping) > 48.0);

	TestTrue(TEXT("Extending removes force"),
		RoboSuspension::ComputeForceN(0.032, -0.1, Stiffness, Damping) < 48.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuspensionNeverPullsTest, "RoboDrive.RoboSim.Suspension.NeverPulls", RoboTest::Flags)

bool FSuspensionNeverPullsTest::RunTest(const FString& Parameters)
{
	// A wheel extending fast enough for the damper to dominate must not
	// produce a downward force, which would suck the robot into the ground.
	TestNearlyEqual(TEXT("Force is clamped at zero"),
		RoboSuspension::ComputeForceN(0.01, -10.0, Stiffness, Damping), 0.0, RoboTest::Tolerance);
	return true;
}

#endif