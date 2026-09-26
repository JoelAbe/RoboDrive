#include "RoboWheel.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double Tolerance = 1e-9;
	constexpr double LoadMassKg = 5.0;
	constexpr double Step = 1.0 / 120.0;
	constexpr double NormalForceN = 49.0;   // a 5 kg corner at rest
	constexpr double Grip = 1.0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTractionAtRestTest, "RoboDrive.RoboSim.Traction.AtRest", RoboTestFlags)

bool FTractionAtRestTest::RunTest(const FString& Parameters)
{
	// Nothing demanded, nothing slipping: no force.
	const RoboTraction::FContactForceN Force =
		RoboTraction::ComputeForceN(0.0, 0.0, LoadMassKg, Step, NormalForceN, Grip);

	TestNearlyEqual(TEXT("No drive force"), Force.Longitudinal, 0.0, Tolerance);
	TestNearlyEqual(TEXT("No lateral force"), Force.Lateral, 0.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTractionResistsSlideTest, "RoboDrive.RoboSim.Traction.ResistsSlide", RoboTestFlags)

bool FTractionResistsSlideTest::RunTest(const FString& Parameters)
{
	// Sliding sideways produces a force opposing the slide. This is where
	// non-holonomic behaviour comes from: it is modelled, not asserted.
	const RoboTraction::FContactForceN Force =
		RoboTraction::ComputeForceN(0.0, 0.05, LoadMassKg, Step, NormalForceN, Grip);

	TestTrue(TEXT("Lateral force opposes the slide"), Force.Lateral < 0.0);
	TestNearlyEqual(TEXT("No drive force was demanded"), Force.Longitudinal, 0.0, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTractionFrictionCircleTest, "RoboDrive.RoboSim.Traction.FrictionCircle", RoboTestFlags)

bool FTractionFrictionCircleTest::RunTest(const FString& Parameters)
{
	// A demand far beyond what grip allows is capped at mu * N.
	const RoboTraction::FContactForceN Force =
		RoboTraction::ComputeForceN(50.0, 0.0, LoadMassKg, Step, NormalForceN, Grip);

	TestNearlyEqual(TEXT("Capped at the friction limit"),
		Force.Longitudinal, NormalForceN * Grip, 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTractionSharedBudgetTest, "RoboDrive.RoboSim.Traction.SharedBudget", RoboTestFlags)

bool FTractionSharedBudgetTest::RunTest(const FString& Parameters)
{
	// Longitudinal and lateral share one budget. Demanding both keeps the
	// total at the limit and the direction intact, which is a skid.
	const RoboTraction::FContactForceN Force =
		RoboTraction::ComputeForceN(50.0, 50.0, LoadMassKg, Step, NormalForceN, Grip);

	const double Magnitude = FMath::Sqrt(
		Force.Longitudinal * Force.Longitudinal + Force.Lateral * Force.Lateral);

	TestNearlyEqual(TEXT("Combined force stays within the friction circle"),
		Magnitude, NormalForceN * Grip, 1e-6);
	TestTrue(TEXT("Drive force is reduced by the lateral demand"),
		Force.Longitudinal < NormalForceN * Grip);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTractionUnloadedWheelTest, "RoboDrive.RoboSim.Traction.UnloadedWheel", RoboTestFlags)

bool FTractionUnloadedWheelTest::RunTest(const FString& Parameters)
{
	// A wheel carrying no load has no grip, so it can apply nothing.
	const RoboTraction::FContactForceN Force =
		RoboTraction::ComputeForceN(5.0, 1.0, LoadMassKg, Step, 0.0, Grip);

	TestNearlyEqual(TEXT("No load means no drive force"), Force.Longitudinal, 0.0, Tolerance);
	TestNearlyEqual(TEXT("No load means no lateral force"), Force.Lateral, 0.0, Tolerance);
	return true;
}

#endif