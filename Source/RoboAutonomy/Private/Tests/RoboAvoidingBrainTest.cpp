#include "RoboAvoidingBrain.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** A snapshot with the robot at the origin facing +X, a goal, and a scan of the given obstacles. */
	FBrainInputs AvoidingInputs(double GoalX, double GoalY, const TArray<FVector2D>& Obstacles,
		double TimeSeconds = 0.0)
	{
		FBrainInputs Inputs;
		Inputs.SimTimeSeconds = TimeSeconds;
		Inputs.Goal.X = GoalX;
		Inputs.Goal.Y = GoalY;
		Inputs.Goal.ToleranceM = 0.5;
		Inputs.bHasGoal = true;
		Inputs.Scan = RoboTest::MakeScan(Obstacles);
		Inputs.Scan.TimeSeconds = TimeSeconds;
		Inputs.bHasScan = true;
		return Inputs;
	}

	/**
	 * A brain with the clutter filter off, so each test of avoidance itself
	 * can decide from a single scan. The filter has its own tests below.
	 */
	FAvoidingBrain MakeAvoidingBrain()
	{
		FAvoidanceSettings Settings;
		Settings.PersistenceScans = 1;
		return FAvoidingBrain(FGoToGoalSettings(), Settings);
	}

	/** A brain with the clutter filter as it ships. */
	FAvoidingBrain MakeFilteringBrain()
	{
		return FAvoidingBrain(FGoToGoalSettings(), FAvoidanceSettings());
	}

	/** A ring of points 0.8 m away on every bearing the lidar sees: nowhere to go. */
	TArray<FVector2D> AvoidingRing()
	{
		TArray<FVector2D> Ring;
		for (int32 Degrees = -135; Degrees <= 135; ++Degrees)
		{
			const double Rad = FMath::DegreesToRadians(static_cast<double>(Degrees));
			Ring.Emplace(0.8 * FMath::Cos(Rad), 0.8 * FMath::Sin(Rad));
		}
		return Ring;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingOpenSpaceTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.OpenSpace", RoboTest::Flags)

bool FAvoidingOpenSpaceTest::RunTest(const FString& Parameters)
{
	// With nothing in the way, avoidance must change nothing: the command
	// should be exactly what go-to-goal alone would have produced.
	FAvoidingBrain Avoiding = MakeAvoidingBrain();
	FGoToGoalBrain Plain;

	const FBrainInputs Inputs = AvoidingInputs(5.0, 1.0, {});
	const FBrainOutput WithAvoidance = Avoiding.Update(Inputs);
	const FBrainOutput Without = Plain.Update(Inputs);

	TestNearlyEqual(TEXT("Same speed as go-to-goal"),
		WithAvoidance.Command.Linear, Without.Command.Linear, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("Same turn as go-to-goal"),
		WithAvoidance.Command.Angular, Without.Command.Angular, RoboTest::Tolerance);
	TestTrue(TEXT("Says it is heading straight for the goal"),
	WithAvoidance.Activity == EBrainActivity::DrivingToGoal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingSteersAroundTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.SteersAround", RoboTest::Flags)

bool FAvoidingSteersAroundTest::RunTest(const FString& Parameters)
{
	// Goal straight ahead, obstacle 1 m ahead in between: steer off the line.
	FAvoidingBrain Brain = MakeAvoidingBrain();
	const FBrainOutput Output = Brain.Update(AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }));

	TestEqual(TEXT("Still travelling"), Output.Status, EMissionStatus::Active);
	TestTrue(TEXT("A heading was chosen"), Output.ChosenHeadingRad.IsSet());
	TestTrue(TEXT("It is not straight at the goal"), FMath::Abs(Output.ChosenHeadingRad.Get(0.0)) > 0.01);
	TestTrue(TEXT("The robot turns"), FMath::Abs(Output.Command.Angular) > 0.01);
	TestTrue(TEXT("Says it is steering around"), Output.Activity == EBrainActivity::SteeringAround);
	TestNearlyEqual(TEXT("And by how much"), Output.DeviationRad, Output.ChosenHeadingRad.Get(0.0), 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingNoScanTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.NoScan", RoboTest::Flags)

bool FAvoidingNoScanTest::RunTest(const FString& Parameters)
{
	// A goal but no scan yet: wait rather than drive blind.
	FAvoidingBrain Brain = MakeAvoidingBrain();
	FBrainInputs Inputs = AvoidingInputs(5.0, 0.0, {});
	Inputs.bHasScan = false;

	const FBrainOutput Output = Brain.Update(Inputs);

	TestEqual(TEXT("Still has a mission"), Output.Status, EMissionStatus::Active);
	TestNearlyEqual(TEXT("Does not drive"), Output.Command.Linear, 0.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("Does not turn"), Output.Command.Angular, 0.0, RoboTest::Tolerance);
	TestTrue(TEXT("Says it is waiting for a scan"), Output.Activity == EBrainActivity::WaitingForScan);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingBoxedInTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.BoxedIn", RoboTest::Flags)

bool FAvoidingBoxedInTest::RunTest(const FString& Parameters)
{
	FAvoidingBrain Brain = MakeAvoidingBrain();

	// Boxed in at t = 0: stop driving, turn to look for a way out.
	const FBrainOutput First = Brain.Update(AvoidingInputs(5.0, 0.0, AvoidingRing(), 0.0));
	TestEqual(TEXT("Keeps trying at first"), First.Status, EMissionStatus::Active);
	TestNearlyEqual(TEXT("Does not drive into the ring"), First.Command.Linear, 0.0, RoboTest::Tolerance);
	TestTrue(TEXT("Turns to look for a way out"), FMath::Abs(First.Command.Angular) > 0.01);
	TestTrue(TEXT("Says it is boxed in"), First.Activity == EBrainActivity::BoxedIn);

	// Still boxed in at t = 3: within the 5 s timeout, still trying.
	const FBrainOutput Middle = Brain.Update(AvoidingInputs(5.0, 0.0, AvoidingRing(), 3.0));
	TestEqual(TEXT("Still trying before the timeout"), Middle.Status, EMissionStatus::Active);
	TestNearlyEqual(TEXT("Knows how long it has been boxed in"), Middle.BlockedForSeconds, 3.0, RoboTest::Tolerance);

	// Still boxed in at t = 6: past the timeout. Give up honestly.
	const FBrainOutput Last = Brain.Update(AvoidingInputs(5.0, 0.0, AvoidingRing(), 6.0));
	TestEqual(TEXT("Reports failure rather than pretending"), Last.Status, EMissionStatus::Failed);
	TestNearlyEqual(TEXT("And stops"), Last.Command.Angular, 0.0, RoboTest::Tolerance);
	TestTrue(TEXT("Says it gave up"), Last.Activity == EBrainActivity::GaveUp);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingArrivesTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.Arrives", RoboTest::Flags)

bool FAvoidingArrivesTest::RunTest(const FString& Parameters)
{
	// Already at the goal with an obstacle just beyond it: arrival wins.
	// Obstacles only matter while travelling.
	FAvoidingBrain Brain = MakeAvoidingBrain();
	const FBrainOutput Output = Brain.Update(AvoidingInputs(0.2, 0.0, { FVector2D(0.5, 0.0) }));

	TestEqual(TEXT("Arrived, regardless of the obstacle"), Output.Status, EMissionStatus::Arrived);
	TestTrue(TEXT("Says it is at the goal"), Output.Activity == EBrainActivity::Arrived);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingIgnoresFlickerTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.IgnoresFlicker", RoboTest::Flags)

bool FAvoidingIgnoresFlickerTest::RunTest(const FString& Parameters)
{
	// Three scans a tenth of a second apart. A return appears straight ahead
	// in the last one only: a single fog echo. The robot must not swerve.
	FAvoidingBrain Brain = MakeFilteringBrain();

	TestTrue(TEXT("Waits until it has enough scans to judge by"),
		Brain.Update(AvoidingInputs(5.0, 0.0, {}, 0.0)).Activity == EBrainActivity::WaitingForScan);
	Brain.Update(AvoidingInputs(5.0, 0.0, {}, 0.1));

	const FBrainOutput Output = Brain.Update(AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }, 0.2));
	TestTrue(TEXT("A return seen once does not make it swerve"), Output.Activity == EBrainActivity::DrivingToGoal);
	TestEqual(TEXT("And is not counted as an obstacle"), Output.ObstaclePointsM.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingAvoidsSteadyTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.AvoidsSteady", RoboTest::Flags)

bool FAvoidingAvoidsSteadyTest::RunTest(const FString& Parameters)
{
	// The same obstacle in all three scans: a real one. The filter must not
	// throw it away.
	FAvoidingBrain Brain = MakeFilteringBrain();
	Brain.Update(AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }, 0.0));
	Brain.Update(AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }, 0.1));

	const FBrainOutput Output = Brain.Update(AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }, 0.2));
	TestTrue(TEXT("Steers round an obstacle that stays put"), Output.Activity == EBrainActivity::SteeringAround);
	TestTrue(TEXT("And counts it as an obstacle"), Output.ObstaclePointsM.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvoidingScanCountedOnceTest,
	"RoboDrive.RoboAutonomy.AvoidingBrain.ScanCountedOnce", RoboTest::Flags)

bool FAvoidingScanCountedOnceTest::RunTest(const FString& Parameters)
{
	// Three decisions, but the lidar has not swept again: the same scan each
	// time. It is one scan, and must not count as three.
	FAvoidingBrain Brain = MakeFilteringBrain();
	FBrainInputs Inputs = AvoidingInputs(5.0, 0.0, { FVector2D(1.0, 0.0) }, 0.0);

	FBrainOutput Output;
	for (int32 Decision = 0; Decision < 3; ++Decision)
	{
		Inputs.SimTimeSeconds = 0.1 * Decision;
		Output = Brain.Update(Inputs);
	}

	TestTrue(TEXT("One scan seen three times is still one scan"), Output.Activity == EBrainActivity::WaitingForScan);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS