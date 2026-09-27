#include "RoboNoise.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
}

/**
 * Characterises the noise model statistically: draw many samples and check the
 * distribution is the one that was asked for. This is the test that makes
 * "Gaussian noise with sigma = x" a claim rather than a comment.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseDistributionTest, "RoboDrive.RoboSim.Noise.Distribution", RoboTestFlags)

bool FNoiseDistributionTest::RunTest(const FString& Parameters)
{
	FRandomStream Stream(12345);

	constexpr int32 SampleCount = 200000;
	constexpr double Mean = 2.0;
	constexpr double StdDev = 0.5;

	double Sum = 0.0;
	double SumOfSquares = 0.0;

	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Sample = RoboNoise::Gaussian(Stream, Mean, StdDev);
		Sum += Sample;
		SumOfSquares += Sample * Sample;
	}

	const double SampleMean = Sum / SampleCount;
	const double SampleVariance = SumOfSquares / SampleCount - SampleMean * SampleMean;
	const double SampleStdDev = FMath::Sqrt(SampleVariance);

	// Tolerances are loose because this is a statistical claim, not an exact
	// one: 200k samples put the mean within a few thousandths of the truth.
	TestNearlyEqual(TEXT("Mean matches what was asked for"), SampleMean, Mean, 0.01);
	TestNearlyEqual(TEXT("Standard deviation matches"), SampleStdDev, StdDev, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseIsReproducibleTest, "RoboDrive.RoboSim.Noise.Reproducible", RoboTestFlags)

bool FNoiseIsReproducibleTest::RunTest(const FString& Parameters)
{
	// Same seed, same noise. Replay in Phase 2 depends on this entirely.
	FRandomStream First(999);
	FRandomStream Second(999);

	for (int32 Index = 0; Index < 100; ++Index)
	{
		TestEqual(TEXT("Same seed gives the same sequence"),
			RoboNoise::Gaussian(First, 0.0, 1.0),
			RoboNoise::Gaussian(Second, 0.0, 1.0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoiseZeroSigmaTest, "RoboDrive.RoboSim.Noise.ZeroSigma", RoboTestFlags)

bool FNoiseZeroSigmaTest::RunTest(const FString& Parameters)
{
	// Turning noise off must be exact, so a run with noise disabled is a clean
	// baseline to compare against rather than "almost no noise".
	FRandomStream Stream(1);
	TestEqual(TEXT("Zero sigma returns the mean exactly"),
		RoboNoise::Gaussian(Stream, 3.5, 0.0), 3.5);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS