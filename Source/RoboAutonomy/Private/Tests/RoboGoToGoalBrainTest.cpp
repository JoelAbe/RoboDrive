#include "RoboGoToGoalBrain.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double GoToGoalTolerance = 0.5;

	/** A snapshot with a goal, the robot at a given pose, and no lidar. */
	FBrainInputs GoToGoalInputs(double GoalX, double GoalY,
		double RobotX = 0.0, double RobotY = 0.0, double RobotYaw = 0.0)
	{
		FBrainInputs Inputs;
		Inputs.Odometry.X = RobotX;
		Inputs.Odometry.Y = RobotY;
		Inputs.Odometry.YawRad = RobotYaw;
		Inputs.Goal.X = GoalX;
		Inputs.Goal.Y = GoalY;
		Inputs.Goal.ToleranceM = GoToGoalTolerance;
		Inputs.bHasGoal = true;
		return Inputs;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalNoGoalTest,
	"RoboDrive.RoboAutonomy.GoToGoal.NoGoal", RoboTest::Flags)

bool FGoToGoalNoGoalTest::RunTest(const FString& Parameters)
{
	FGoToGoalBrain Brain;
	const FBrainOutput Output = Brain.Update(FBrainInputs{});

	TestNearlyEqual(TEXT("No goal means no motion"), Output.Command.Linear, 0.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("No goal means no turning"), Output.Command.Angular, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("No goal means no mission"), Output.Status, EMissionStatus::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalDrivesTowardTest,
	"RoboDrive.RoboAutonomy.GoToGoal.DrivesToward", RoboTest::Flags)

bool FGoToGoalDrivesTowardTest::RunTest(const FString& Parameters)
{
	// Goal straight ahead: drive forward without turning.
	FGoToGoalBrain Brain;
	const FBrainOutput Output = Brain.Update(GoToGoalInputs(5.0, 0.0));

	TestTrue(TEXT("Drives forward"), Output.Command.Linear > 0.0);
	TestNearlyEqual(TEXT("Does not turn"), Output.Command.Angular, 0.0, RoboTest::Tolerance);
	TestEqual(TEXT("Mission is active"), Output.Status, EMissionStatus::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalTurnsTowardTest,
	"RoboDrive.RoboAutonomy.GoToGoal.TurnsToward", RoboTest::Flags)

bool FGoToGoalTurnsTowardTest::RunTest(const FString& Parameters)
{
	FGoToGoalBrain Brain;

	// REP-103: +Y is left, and a left turn is positive.
	TestTrue(TEXT("Goal on the left turns left"),
		Brain.Update(GoToGoalInputs(0.0, 5.0)).Command.Angular > 0.0);
	TestTrue(TEXT("Goal on the right turns right"),
		Brain.Update(GoToGoalInputs(0.0, -5.0)).Command.Angular < 0.0);

	// Goal directly behind: turn on the spot rather than reversing toward it.
	const FBrainOutput Behind = Brain.Update(GoToGoalInputs(-5.0, 0.0));
	TestNearlyEqual(TEXT("Does not drive while facing away"), Behind.Command.Linear, 0.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("Turns at full rate"),
		FMath::Abs(Behind.Command.Angular), FGoToGoalSettings().MaxTurnRadS, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalSpeedLimitTest,
	"RoboDrive.RoboAutonomy.GoToGoal.SpeedLimit", RoboTest::Flags)

bool FGoToGoalSpeedLimitTest::RunTest(const FString& Parameters)
{
	// A distant goal asks for more speed than allowed; the limit must hold.
	FGoToGoalBrain Brain;
	TestNearlyEqual(TEXT("Speed is capped"),
		Brain.Update(GoToGoalInputs(100.0, 0.0)).Command.Linear,
		FGoToGoalSettings().MaxSpeedMS, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalArrivesTest,
	"RoboDrive.RoboAutonomy.GoToGoal.Arrives", RoboTest::Flags)

bool FGoToGoalArrivesTest::RunTest(const FString& Parameters)
{
	// 0.2 m away, inside the 0.25 m arrive radius (half of 0.5): stop and report.
	FGoToGoalBrain Brain;
	const FBrainOutput Output = Brain.Update(GoToGoalInputs(0.2, 0.0));

	TestEqual(TEXT("Reports arrived"), Output.Status, EMissionStatus::Arrived);
	TestNearlyEqual(TEXT("Stops"), Output.Command.Linear, 0.0, RoboTest::Tolerance);
	TestNearlyEqual(TEXT("Stops turning"), Output.Command.Angular, 0.0, RoboTest::Tolerance);
	return true;
}

/**
 * The hysteresis test. Once arrived, coasting a little further away must not
 * restart the robot; only leaving the outer radius does.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalHysteresisTest,
	"RoboDrive.RoboAutonomy.GoToGoal.Hysteresis", RoboTest::Flags)

bool FGoToGoalHysteresisTest::RunTest(const FString& Parameters)
{
	FGoToGoalBrain Brain;

	// Arrive: 0.2 m is inside the 0.25 m arrive radius.
	Brain.Update(GoToGoalInputs(0.2, 0.0));

	// Coast to 0.4 m: outside the arrive radius but inside the 0.5 m leave
	// radius. A single threshold would restart here.
	TestEqual(TEXT("Stays arrived after coasting a little"),
		Brain.Update(GoToGoalInputs(0.4, 0.0)).Status, EMissionStatus::Arrived);

	// Pushed out to 0.6 m: beyond the leave radius, so set off again.
	TestEqual(TEXT("Sets off again once clearly away"),
		Brain.Update(GoToGoalInputs(0.6, 0.0)).Status, EMissionStatus::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGoToGoalNewGoalTest,
	"RoboDrive.RoboAutonomy.GoToGoal.NewGoal", RoboTest::Flags)

bool FGoToGoalNewGoalTest::RunTest(const FString& Parameters)
{
	FGoToGoalBrain Brain;

	// Arrive at a goal at (0.2, 0).
	Brain.Update(GoToGoalInputs(0.2, 0.0));

	// A different goal 0.4 m away. Hysteresis would keep "arrived" for the old
	// goal at this distance, but this is a new goal: arriving at the last one
	// says nothing about it.
	TestEqual(TEXT("A new goal starts a new journey"),
		Brain.Update(GoToGoalInputs(0.0, 0.4)).Status, EMissionStatus::Active);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS