#include "RoboScenario.h"
#include "RoboTestCommon.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Parse a scenario written inline, exactly as it would appear in a file. */
	TSharedRef<FJsonObject> ScenarioFromText(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object);
		return Object.IsValid() ? Object.ToSharedRef() : MakeShared<FJsonObject>();
	}

	bool ScenarioIsValid(const FString& Text, FString& OutError)
	{
		return URoboScenarioSubsystem::Validate(*ScenarioFromText(Text), OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioAcceptsMinimalTest,
	"RoboDrive.RoboSim.Scenario.AcceptsMinimal", RoboTest::Flags)

bool FScenarioAcceptsMinimalTest::RunTest(const FString& Parameters)
{
	// Only the version and a start are required; everything else is optional.
	FString Error;
	TestTrue(TEXT("A version and a start are enough"),
		ScenarioIsValid(TEXT(R"({ "schema_version": 1, "start": { "x": 0, "y": 0 } })"), Error));
	TestTrue(TEXT("And no error is given"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioAcceptsFullTest,
	"RoboDrive.RoboSim.Scenario.AcceptsFull", RoboTest::Flags)

bool FScenarioAcceptsFullTest::RunTest(const FString& Parameters)
{
	// Every field the format has, as in Scenarios/slalom_fog.json.
	FString Error;
	TestTrue(TEXT("A scenario using every field is accepted"), ScenarioIsValid(TEXT(R"({
		"schema_version": 1,
		"name": "everything",
		"start": { "x": 0, "y": 0, "yaw": 0.5 },
		"goal": { "x": 10, "y": 0, "tolerance_m": 0.5 },
		"time_limit_s": 60,
		"visibility_m": 10,
		"known_failure": "a reason",
		"obstacles": [ { "x": 3, "y": 0.15, "yaw": 0.2, "size": [0.4, 0.4, 1.0] } ]
	})"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioRejectsOtherVersionTest,
	"RoboDrive.RoboSim.Scenario.RejectsOtherVersion", RoboTest::Flags)

bool FScenarioRejectsOtherVersionTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestFalse(TEXT("A future version is refused rather than half-understood"),
		ScenarioIsValid(TEXT(R"({ "schema_version": 2, "start": { "x": 0, "y": 0 } })"), Error));
	TestTrue(TEXT("And the error says why"), Error.Contains(TEXT("schema_version")));

	TestFalse(TEXT("So is a scenario with no version at all"),
		ScenarioIsValid(TEXT(R"({ "start": { "x": 0, "y": 0 } })"), Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioRejectsMissingStartTest,
	"RoboDrive.RoboSim.Scenario.RejectsMissingStart", RoboTest::Flags)

bool FScenarioRejectsMissingStartTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestFalse(TEXT("A scenario must say where the robot starts"),
		ScenarioIsValid(TEXT(R"({ "schema_version": 1 })"), Error));
	TestTrue(TEXT("And the error says so"), Error.Contains(TEXT("start")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioRejectsBadObstacleTest,
	"RoboDrive.RoboSim.Scenario.RejectsBadObstacle", RoboTest::Flags)

bool FScenarioRejectsBadObstacleTest::RunTest(const FString& Parameters)
{
	// The second obstacle has a two-element size. The error must point at it,
	// not at the first, which is fine.
	FString Error;
	TestFalse(TEXT("An obstacle without a full size is refused"), ScenarioIsValid(TEXT(R"({
		"schema_version": 1,
		"start": { "x": 0, "y": 0 },
		"obstacles": [
			{ "x": 2, "y": 1, "size": [1, 1, 1] },
			{ "x": 4, "y": 1, "size": [1, 1] }
		]
	})"), Error));
	TestTrue(TEXT("And the error names the right one"), Error.Contains(TEXT("obstacle 1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScenarioRejectsBadVisibilityTest,
	"RoboDrive.RoboSim.Scenario.RejectsBadVisibility", RoboTest::Flags)

bool FScenarioRejectsBadVisibilityTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestFalse(TEXT("Zero visibility has no physical meaning"),
		ScenarioIsValid(TEXT(R"({ "schema_version": 1, "start": { "x": 0, "y": 0 }, "visibility_m": 0 })"), Error));
	TestFalse(TEXT("Nor does visibility given as words"),
		ScenarioIsValid(TEXT(R"({ "schema_version": 1, "start": { "x": 0, "y": 0 }, "visibility_m": "thick" })"), Error));
	TestTrue(TEXT("And the error says so"), Error.Contains(TEXT("visibility_m")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
