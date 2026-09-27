#include "RoboWheelEncoder.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags RoboTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	constexpr int32 TicksPerRev = 2048;
	constexpr double WheelRadiusM = 0.1;

	/** No noise: most of these tests are about quantisation and scaling. */
	int64 UpdateClean(FWheelEncoder& Encoder, double SpeedRadS, double DeltaSeconds)
	{
		FRandomStream Unused(0);
		return Encoder.Update(SpeedRadS, DeltaSeconds, Unused, 0.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderCountsRevolutionTest,
	"RoboDrive.RoboSim.Encoder.CountsRevolution", RoboTestFlags)

bool FEncoderCountsRevolutionTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(TicksPerRev);

	// Exactly one revolution: 2*pi rad/s for one second.
	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);

	TestEqual(TEXT("One revolution is one full tick count"),
		Encoder.GetTickCount(), (int64)TicksPerRev);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderKeepsRemainderTest,
	"RoboDrive.RoboSim.Encoder.KeepsRemainder", RoboTestFlags)

bool FEncoderKeepsRemainderTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(TicksPerRev);
	const double RadiansPerTick = UE_DOUBLE_TWO_PI / TicksPerRev;

	// Two half-ticks make a whole tick: quantisation must not lose rotation,
	// or slow driving would silently under-report distance forever.
	TestEqual(TEXT("Half a tick reports nothing"),
		UpdateClean(Encoder, 0.5 * RadiansPerTick, 1.0), (int64)0);
	TestEqual(TEXT("The second half completes a tick"),
		UpdateClean(Encoder, 0.5 * RadiansPerTick, 1.0), (int64)1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderReversesTest,
	"RoboDrive.RoboSim.Encoder.Reverses", RoboTestFlags)

bool FEncoderReversesTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(TicksPerRev);

	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);
	UpdateClean(Encoder, -UE_DOUBLE_TWO_PI, 1.0);

	TestEqual(TEXT("Driving back returns the count to zero"),
		Encoder.GetTickCount(), (int64)0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderDistanceTest,
	"RoboDrive.RoboSim.Encoder.Distance", RoboTestFlags)

bool FEncoderDistanceTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(TicksPerRev);

	// One revolution of a 0.1 m wheel rolls its circumference, 2*pi*0.1 m.
	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);

	const double DistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), TicksPerRev, WheelRadiusM);

	TestNearlyEqual(TEXT("Distance is one circumference"),
		DistanceM, UE_DOUBLE_TWO_PI * WheelRadiusM, 1e-9);
	return true;
}

/**
 * The error that matters most. If the software believes the wheel is 2%
 * larger than it is, every distance is 2% long, in the same direction, for
 * ever. This is what separates a systematic error from a noisy one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderRadiusErrorTest,
	"RoboDrive.RoboSim.Encoder.RadiusError", RoboTestFlags)

bool FEncoderRadiusErrorTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(TicksPerRev);

	constexpr int32 Revolutions = 100;
	for (int32 Index = 0; Index < Revolutions; ++Index)
	{
		UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);
	}

	const double TrueDistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), TicksPerRev, WheelRadiusM);

	const double BelievedDistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), TicksPerRev, WheelRadiusM * 1.02);

	TestNearlyEqual(TEXT("A 2 percent radius error is a 2 percent distance error"),
		BelievedDistanceM / TrueDistanceM, 1.02, 1e-9);

	// And it scales with distance travelled, rather than averaging away.
	TestTrue(TEXT("Over 100 revolutions that is more than a metre"),
		BelievedDistanceM - TrueDistanceM > 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderNoiseDoesNotBiasTest,
	"RoboDrive.RoboSim.Encoder.NoiseDoesNotBias", RoboTestFlags)

bool FEncoderNoiseDoesNotBiasTest::RunTest(const FString& Parameters)
{
	// Noisy but unbiased: over many steps the random errors largely cancel,
	// unlike the radius error above. Different shapes of wrongness.
	FWheelEncoder Encoder(TicksPerRev);
	FRandomStream Stream(4242);

	constexpr int32 Steps = 10000;
	constexpr double SpeedRadS = 10.0;
	constexpr double StepSeconds = 0.01;

	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Encoder.Update(SpeedRadS, StepSeconds, Stream, 1.0);
	}

	const double MeasuredRadians =
		Encoder.GetTickCount() * UE_DOUBLE_TWO_PI / TicksPerRev;
	const double TrueRadians = SpeedRadS * StepSeconds * Steps;

	TestTrue(TEXT("Noise stays small relative to distance travelled"),
		FMath::Abs(MeasuredRadians - TrueRadians) / TrueRadians < 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS