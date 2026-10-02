#include "RoboTestCommon.h"
#include "RoboWheelEncoder.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 EncoderTicksPerRev = 2048;
	constexpr double EncoderWheelRadiusM = 0.1;

	/** No noise: most of these tests are about quantisation and scaling. */
	int64 UpdateClean(FWheelEncoder& Encoder, double SpeedRadS, double DeltaSeconds)
	{
		FRandomStream Unused(0);
		return Encoder.Update(SpeedRadS, DeltaSeconds, Unused, 0.0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderCountsRevolutionTest,
	"RoboDrive.RoboSim.Encoder.CountsRevolution", RoboTest::Flags)

bool FEncoderCountsRevolutionTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(EncoderTicksPerRev);

	// Exactly one revolution: 2*pi rad/s for one second.
	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);

	TestEqual(TEXT("One revolution is one full tick count"),
		Encoder.GetTickCount(), (int64)EncoderTicksPerRev);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderKeepsRemainderTest,
	"RoboDrive.RoboSim.Encoder.KeepsRemainder", RoboTest::Flags)

bool FEncoderKeepsRemainderTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(EncoderTicksPerRev);
	const double RadiansPerTick = UE_DOUBLE_TWO_PI / EncoderTicksPerRev;

	// Two half-ticks make a whole tick: quantisation must not lose rotation,
	// or slow driving would silently under-report distance forever.
	TestEqual(TEXT("Half a tick reports nothing"),
		UpdateClean(Encoder, 0.5 * RadiansPerTick, 1.0), (int64)0);
	TestEqual(TEXT("The second half completes a tick"),
		UpdateClean(Encoder, 0.5 * RadiansPerTick, 1.0), (int64)1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderReversesTest,
	"RoboDrive.RoboSim.Encoder.Reverses", RoboTest::Flags)

bool FEncoderReversesTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(EncoderTicksPerRev);

	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);
	UpdateClean(Encoder, -UE_DOUBLE_TWO_PI, 1.0);

	TestEqual(TEXT("Driving back returns the count to zero"),
		Encoder.GetTickCount(), (int64)0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderDistanceTest,
	"RoboDrive.RoboSim.Encoder.Distance", RoboTest::Flags)

bool FEncoderDistanceTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(EncoderTicksPerRev);

	// One revolution of a 0.1 m wheel rolls its circumference, 2*pi*0.1 m.
	UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);

	const double DistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), EncoderTicksPerRev, EncoderWheelRadiusM);

	TestNearlyEqual(TEXT("Distance is one circumference"),
		DistanceM, UE_DOUBLE_TWO_PI * EncoderWheelRadiusM, 1e-9);
	return true;
}

/**
 * The error that matters most. If the software believes the wheel is 2%
 * larger than it is, every distance is 2% long, in the same direction, for
 * ever. This is what separates a systematic error from a noisy one.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderRadiusErrorTest,
	"RoboDrive.RoboSim.Encoder.RadiusError", RoboTest::Flags)

bool FEncoderRadiusErrorTest::RunTest(const FString& Parameters)
{
	FWheelEncoder Encoder(EncoderTicksPerRev);

	constexpr int32 Revolutions = 100;
	for (int32 Index = 0; Index < Revolutions; ++Index)
	{
		UpdateClean(Encoder, UE_DOUBLE_TWO_PI, 1.0);
	}

	const double TrueDistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), EncoderTicksPerRev, EncoderWheelRadiusM);

	const double BelievedDistanceM = FWheelEncoder::TicksToDistanceM(
		Encoder.GetTickCount(), EncoderTicksPerRev, EncoderWheelRadiusM * 1.02);

	TestNearlyEqual(TEXT("A 2 percent radius error is a 2 percent distance error"),
		BelievedDistanceM / TrueDistanceM, 1.02, 1e-9);

	// And it scales with distance travelled, rather than averaging away.
	TestTrue(TEXT("Over 100 revolutions that is more than a metre"),
		BelievedDistanceM - TrueDistanceM > 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncoderNoiseDoesNotBiasTest,
	"RoboDrive.RoboSim.Encoder.NoiseDoesNotBias", RoboTest::Flags)

bool FEncoderNoiseDoesNotBiasTest::RunTest(const FString& Parameters)
{
	// Noisy but unbiased: over many steps the random errors largely cancel,
	// unlike the radius error above. Different shapes of wrongness.
	FWheelEncoder Encoder(EncoderTicksPerRev);
	FRandomStream Stream(4242);

	constexpr int32 Steps = 10000;
	constexpr double SpeedRadS = 10.0;
	constexpr double StepSeconds = 0.01;

	for (int32 Index = 0; Index < Steps; ++Index)
	{
		Encoder.Update(SpeedRadS, StepSeconds, Stream, 1.0);
	}

	const double MeasuredRadians =
		Encoder.GetTickCount() * UE_DOUBLE_TWO_PI / EncoderTicksPerRev;
	const double TrueRadians = SpeedRadS * StepSeconds * Steps;

	TestTrue(TEXT("Noise stays small relative to distance travelled"),
		FMath::Abs(MeasuredRadians - TrueRadians) / TrueRadians < 0.01);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS