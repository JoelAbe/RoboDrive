#include "RoboFog.h"
#include "RoboTestCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogVisibilityTest,
	"RoboDrive.RoboSim.Fog.Visibility", RoboTest::Flags)

bool FFogVisibilityTest::RunTest(const FString& Parameters)
{
	// Visibility is defined as the distance at which contrast falls to 2%.
	// Check that meaning directly, rather than restating the formula.
	const double VisibilityM = 20.0;
	const double Contrast = FMath::Exp(-RoboFog::ExtinctionPerM(VisibilityM) * VisibilityM);
	TestNearlyEqual(TEXT("Contrast is 2% at the visibility distance"), Contrast, 0.02, 1e-9);

	TestTrue(TEXT("Thicker fog scatters more"),
		RoboFog::ExtinctionPerM(5.0) > RoboFog::ExtinctionPerM(20.0));
	TestNearlyEqual(TEXT("Nonsense visibility is treated as clear"),
		RoboFog::ExtinctionPerM(0.0), 0.0, RoboTest::Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogClearAirTest,
	"RoboDrive.RoboSim.Fog.ClearAir", RoboTest::Flags)

bool FFogClearAirTest::RunTest(const FString& Parameters)
{
	// With no fog, everything must be exactly as it was before fog existed.
	TestTrue(TEXT("Sees to maximum range"), RoboFog::IsDetectable(12.0, 12.0, 0.0));
	TestFalse(TEXT("And no further"), RoboFog::IsDetectable(12.01, 12.0, 0.0));

	// Two streams with the same seed. If sampling an echo drew a number from
	// the first, its next number would no longer match the second's.
	FRandomStream Used(42);
	FRandomStream Untouched(42);

	TestFalse(TEXT("No echo"), RoboLidar::HasReturn(RoboFog::SampleEchoM(Used, 0.0, 0.1)));
	TestEqual(TEXT("And no random number drawn"), Used.GetFraction(), Untouched.GetFraction());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogShortensRangeTest,
	"RoboDrive.RoboSim.Fog.ShortensRange", RoboTest::Flags)

bool FFogShortensRangeTest::RunTest(const FString& Parameters)
{
	// 20 m visibility, a 12 m lidar. Worked by hand:
	//   at 4 m: (12 / 4)^2 * exp(-2 * 0.196 * 4) = 1.88, detectable
	//   at 6 m: (12 / 6)^2 * exp(-2 * 0.196 * 6) = 0.38, lost
	const double Extinction = RoboFog::ExtinctionPerM(20.0);

	TestTrue(TEXT("Sees a surface at 4 m"), RoboFog::IsDetectable(4.0, 12.0, Extinction));
	TestFalse(TEXT("Loses one at 6 m"), RoboFog::IsDetectable(6.0, 12.0, Extinction));
	TestTrue(TEXT("Which clear air would see"), RoboFog::IsDetectable(6.0, 12.0, 0.0));

	TestFalse(TEXT("Thicker fog loses even 4 m"),
		RoboFog::IsDetectable(4.0, 12.0, RoboFog::ExtinctionPerM(5.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFogEchoDistanceTest,
	"RoboDrive.RoboSim.Fog.EchoDistance", RoboTest::Flags)

bool FFogEchoDistanceTest::RunTest(const FString& Parameters)
{
	// Extinction 1 per metre, half of it echoing: a chance of 0.5 per metre,
	// so echoes should come back on average 2 m out.
	FRandomStream Stream(2024);

	constexpr int32 SampleCount = 200000;
	double Sum = 0.0;
	int32 WithinMean = 0;

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double EchoM = RoboFog::SampleEchoM(Stream, 1.0, 0.5);
		Sum += EchoM;
		WithinMean += EchoM < 2.0 ? 1 : 0;
	}

	// Loose tolerances, because this is a statistical claim, as in the noise test.
	TestNearlyEqual(TEXT("Mean distance is 1 / rate"), Sum / SampleCount, 2.0, 0.02);

	// The exponential's signature: 1 - 1/e, about 63%, of echoes come back
	// closer than the mean. Most are near, a few are far.
	TestNearlyEqual(TEXT("Shape is exponential"),
		static_cast<double>(WithinMean) / SampleCount, 1.0 - FMath::Exp(-1.0), 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS