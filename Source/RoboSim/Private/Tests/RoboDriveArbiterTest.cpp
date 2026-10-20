#include "RoboDriveArbiter.h"
#include "RoboTestCommon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FDriveCommand MakeCommand(EDriveSource Source, double Linear, double TimeSeconds)
	{
		FDriveCommand Command;
		Command.Source = Source;
		Command.Linear = Linear;
		Command.Angular = 0.0;
		Command.TimeSeconds = TimeSeconds;
		return Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterPriorityOrderTest, "RoboDrive.RoboSim.Arbiter.PriorityOrder", RoboTest::Flags)

bool FDriveArbiterPriorityOrderTest::RunTest(const FString& Parameters)
{
	// The arbiter uses the enum's numeric order as its priority order, so
	// reordering the enum would silently change behaviour. Pin it here.
	TestTrue(TEXT("Manual outranks external"), EDriveSource::Manual > EDriveSource::External);
	TestTrue(TEXT("External outranks autonomy"), EDriveSource::External > EDriveSource::Autonomy);
	TestTrue(TEXT("Autonomy outranks none"), EDriveSource::Autonomy > EDriveSource::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterStopsWhenIdleTest, "RoboDrive.RoboSim.Arbiter.StopsWhenIdle", RoboTest::Flags)

bool FDriveArbiterStopsWhenIdleTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	const FDriveCommand Result = Arbiter.Evaluate(0.0);

	TestNearlyEqual(TEXT("No command means no motion"), Result.Linear, 0.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("No command means no rotation"), Result.Angular, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("Nobody is driving"), Result.Source, EDriveSource::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterPassesThroughTest, "RoboDrive.RoboSim.Arbiter.PassesThrough", RoboTest::Flags)

bool FDriveArbiterPassesThroughTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.5, 10.0));

	const FDriveCommand Result = Arbiter.Evaluate(10.1);

	TestNearlyEqual(TEXT("The only fresh command is used"), Result.Linear, 1.5, RoboTest::Tolerance);
	TestEqual(TEXT("Autonomy is driving"), Arbiter.GetActiveSource(), EDriveSource::Autonomy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterManualOverridesTest, "RoboDrive.RoboSim.Arbiter.ManualOverrides", RoboTest::Flags)

bool FDriveArbiterManualOverridesTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.5, 10.0));
	Arbiter.Submit(MakeCommand(EDriveSource::Manual, -0.5, 10.0));

	const FDriveCommand Result = Arbiter.Evaluate(10.1);

	TestNearlyEqual(TEXT("Manual input wins"), Result.Linear, -0.5, RoboTest::Tolerance);
	TestEqual(TEXT("Manual is driving"), Arbiter.GetActiveSource(), EDriveSource::Manual);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterManualReleasesTest, "RoboDrive.RoboSim.Arbiter.ManualReleases", RoboTest::Flags)

bool FDriveArbiterManualReleasesTest::RunTest(const FString& Parameters)
{
	// The driver lets go: manual commands stop arriving and expire, while the
	// autonomy keeps sending. Control returns to the autonomy by itself.
	FDriveArbiter Arbiter;
	Arbiter.Submit(MakeCommand(EDriveSource::Manual, -0.5, 10.0));
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.5, 10.9));

	const FDriveCommand Result = Arbiter.Evaluate(11.0);

	TestNearlyEqual(TEXT("Stale manual input is ignored"), Result.Linear, 1.5, RoboTest::Tolerance);
	TestEqual(TEXT("Autonomy has control again"), Arbiter.GetActiveSource(), EDriveSource::Autonomy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterWatchdogTest, "RoboDrive.RoboSim.Arbiter.Watchdog", RoboTest::Flags)

bool FDriveArbiterWatchdogTest::RunTest(const FString& Parameters)
{
	// The autonomy dies mid-drive. The robot must stop rather than continue
	// executing the last command it was given.
	FDriveArbiter Arbiter;
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.5, 10.0));

	TestNearlyEqual(TEXT("Still driving just before the timeout"), Arbiter.Evaluate(10.2).Linear, 1.5, RoboTest::Tolerance);

	const FDriveCommand Result = Arbiter.Evaluate(10.5);

	TestNearlyEqual(TEXT("Stops once the command expires"), Result.Linear, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("Nobody is driving"), Arbiter.GetActiveSource(), EDriveSource::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterTimeoutBoundaryTest, "RoboDrive.RoboSim.Arbiter.TimeoutBoundary", RoboTest::Flags)

bool FDriveArbiterTimeoutBoundaryTest::RunTest(const FString& Parameters)
{
	// A command exactly at the timeout counts as still valid. Pinned so the
	// choice is deliberate rather than accidental.
	FDriveArbiter Arbiter(0.25);
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.0, 5.0));

	TestNearlyEqual(TEXT("Valid exactly at the timeout"), Arbiter.Evaluate(5.25).Linear, 1.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("Expired just after"), Arbiter.Evaluate(5.2500001).Linear, 0.0, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterStopBeatsManualTest, "RoboDrive.RoboSim.Arbiter.StopBeatsManual", RoboTest::Flags)

bool FDriveArbiterStopBeatsManualTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	Arbiter.Submit(MakeCommand(EDriveSource::Manual, 1.0, 10.0));
	Arbiter.EngageStop();

	const FDriveCommand Result = Arbiter.Evaluate(10.0);

	TestNearlyEqual(TEXT("Even a person at the keys cannot drive"), Result.Linear, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("Nobody is driving"), Arbiter.GetActiveSource(), EDriveSource::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterStopDoesNotExpireTest, "RoboDrive.RoboSim.Arbiter.StopDoesNotExpire", RoboTest::Flags)

bool FDriveArbiterStopDoesNotExpireTest::RunTest(const FString& Parameters)
{
	// Engaged at the start, checked 100 s later against a fresh command:
	// far past the 0.25 s that any command lasts.
	FDriveArbiter Arbiter;
	Arbiter.EngageStop();
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.0, 100.0));

	TestNearlyEqual(TEXT("Still stopped long after it was pressed"),
		Arbiter.Evaluate(100.0).Linear, 0.0, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterStopReleasesTest, "RoboDrive.RoboSim.Arbiter.StopReleases", RoboTest::Flags)

bool FDriveArbiterStopReleasesTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	Arbiter.EngageStop();
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.0, 10.0));
	TestNearlyEqual(TEXT("Stopped"), Arbiter.Evaluate(10.0).Linear, 0.0, RoboTest::Tolerance);

	Arbiter.ReleaseStop();
	TestNearlyEqual(TEXT("Drives again once released"), Arbiter.Evaluate(10.1).Linear, 1.0, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterManualModeTest, "RoboDrive.RoboSim.Arbiter.ManualMode", RoboTest::Flags)

bool FDriveArbiterManualModeTest::RunTest(const FString& Parameters)
{
	FDriveArbiter Arbiter;
	Arbiter.SetMode(EDriveMode::Manual);
	Arbiter.Submit(MakeCommand(EDriveSource::Autonomy, 1.0, 10.0));
	Arbiter.Submit(MakeCommand(EDriveSource::External, 2.0, 10.0));

	// Fresh commands from both, and still nothing: they are ignored, not outranked.
	TestNearlyEqual(TEXT("Autonomy and external are ignored"), Arbiter.Evaluate(10.0).Linear, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("So nobody is driving"), Arbiter.GetActiveSource(), EDriveSource::None);

	Arbiter.Submit(MakeCommand(EDriveSource::Manual, 0.5, 10.0));
	TestNearlyEqual(TEXT("A person still can"), Arbiter.Evaluate(10.0).Linear, 0.5, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDriveArbiterResetKeepsOperatorTest, "RoboDrive.RoboSim.Arbiter.ResetKeepsOperator", RoboTest::Flags)

bool FDriveArbiterResetKeepsOperatorTest::RunTest(const FString& Parameters)
{
	// Resetting a scenario must not undo what the operator decided.
	FDriveArbiter Arbiter;
	Arbiter.EngageStop();
	Arbiter.SetMode(EDriveMode::Manual);
	Arbiter.Reset();

	TestTrue(TEXT("Still stopped"), Arbiter.IsStopped());
	TestTrue(TEXT("Still in manual"), Arbiter.GetMode() == EDriveMode::Manual);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS