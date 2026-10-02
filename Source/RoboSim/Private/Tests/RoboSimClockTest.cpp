#include "RoboSimClock.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr double ClockTolerance = 1e-12;
	constexpr double ClockStep = 0.01;   // 100 Hz, easy arithmetic
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockStartsAtZeroTest, "RoboDrive.RoboSim.Clock.StartsAtZero", RoboTestFlags)

bool FSimClockStartsAtZeroTest::RunTest(const FString& Parameters)
{
	const FSimClock Clock(ClockStep);
	TestNearlyEqual(TEXT("Time starts at zero"), Clock.GetTimeSeconds(), 0.0, ClockTolerance);
	TestEqual(TEXT("No steps taken yet"), Clock.GetStepCount(), (int64)0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockTakesWholeStepsTest, "RoboDrive.RoboSim.Clock.TakesWholeSteps", RoboTestFlags)

bool FSimClockTakesWholeStepsTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(ClockStep);

	// 25 ms of real time is two whole 10 ms steps, with 5 ms left over.
	TestEqual(TEXT("Two steps fit in 25 ms"), Clock.Advance(0.025), 2);
	TestNearlyEqual(TEXT("Time is exactly two steps"), Clock.GetTimeSeconds(), 0.02, ClockTolerance);
	TestNearlyEqual(TEXT("The remainder is kept"), Clock.GetPendingSeconds(), 0.005, ClockTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockCarriesRemainderTest, "RoboDrive.RoboSim.Clock.CarriesRemainder", RoboTestFlags)

bool FSimClockCarriesRemainderTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(ClockStep);

	// Frames shorter than a step take none, but nothing is lost: two of them
	// together are worth a step.
	TestEqual(TEXT("Half a step takes nothing"), Clock.Advance(0.005), 0);
	TestEqual(TEXT("The other half completes a step"), Clock.Advance(0.005), 1);
	TestNearlyEqual(TEXT("Exactly one step of time passed"), Clock.GetTimeSeconds(), 0.01, ClockTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockLimitsCatchUpTest, "RoboDrive.RoboSim.Clock.LimitsCatchUp", RoboTestFlags)

bool FSimClockLimitsCatchUpTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(ClockStep);

	// A two second stall would be 200 steps. The clock refuses to spiral and
	// lets sim time fall behind instead.
	const int32 StepsTaken = Clock.Advance(2.0);

	TestEqual(TEXT("Catch-up is capped"), StepsTaken, FSimClock::DefaultMaxStepsPerAdvance);
	TestTrue(TEXT("Sim time is behind real time"), Clock.GetTimeSeconds() < 2.0);
	TestNearlyEqual(TEXT("The backlog is dropped rather than queued"), Clock.GetPendingSeconds(), 0.0, ClockTolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockSynchronousIgnoresRealTimeTest, "RoboDrive.RoboSim.Clock.SynchronousIgnoresRealTime", RoboTestFlags)

bool FSimClockSynchronousIgnoresRealTimeTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(ClockStep);
	Clock.SetMode(ESimClockMode::Synchronous);

	TestEqual(TEXT("Real time does not move a synchronous clock"), Clock.Advance(1.0), 0);
	TestNearlyEqual(TEXT("Still at zero"), Clock.GetTimeSeconds(), 0.0, ClockTolerance);

	// Only an explicit step moves it, which is what a test client will drive.
	TestEqual(TEXT("Explicit stepping works"), Clock.Step(5), 5);
	TestNearlyEqual(TEXT("Five steps of time passed"), Clock.GetTimeSeconds(), 0.05, ClockTolerance);
	return true;
}

/**
 * Time is computed as steps x step size rather than accumulated, so it cannot
 * drift. Accumulating 1/120 a hundred thousand times would be visibly wrong by
 * the end; this is the test that would catch a change back to +=.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockDoesNotDriftTest, "RoboDrive.RoboSim.Clock.DoesNotDrift", RoboTestFlags)

bool FSimClockDoesNotDriftTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(FSimClock::DefaultStepSeconds);
	Clock.SetMode(ESimClockMode::Synchronous);

	constexpr int32 Steps = 100000;
	Clock.Step(Steps);

	TestNearlyEqual(TEXT("Time is exactly the step count times the step size"), Clock.GetTimeSeconds(), Steps * FSimClock::DefaultStepSeconds, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimClockResetTest, "RoboDrive.RoboSim.Clock.Reset", RoboTestFlags)

bool FSimClockResetTest::RunTest(const FString& Parameters)
{
	FSimClock Clock(ClockStep);
	Clock.Advance(0.1);
	Clock.Reset();

	TestNearlyEqual(TEXT("Time is back to zero"), Clock.GetTimeSeconds(), 0.0, ClockTolerance);
	TestEqual(TEXT("Step count is back to zero"), Clock.GetStepCount(), (int64)0);
	TestNearlyEqual(TEXT("No leftover time"), Clock.GetPendingSeconds(), 0.0, ClockTolerance);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS