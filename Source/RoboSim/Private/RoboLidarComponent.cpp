#include "RoboLidarComponent.h"

#include "RoboFrames.h"
#include "RoboNoise.h"
#include "RoboSimSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

DECLARE_STATS_GROUP(TEXT("RoboSim"), STATGROUP_RoboSim, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Lidar Sweep"), STAT_RoboLidarSweep, STATGROUP_RoboSim);

URoboLidarComponent::URoboLidarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboLidarComponent::BeginPlay()
{
	Super::BeginPlay();
	NoiseStream.Initialize(RandomSeed);
}

void URoboLidarComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Sim == nullptr)
	{
		return;
	}

	// One sweep per scan period. Unlike the encoders we do not catch up on
	// missed scans: a lidar that fell behind simply did not take that scan.
	const double NowSeconds = Sim->GetSimTimeSeconds();
	if (NowSeconds < NextScanTimeSeconds)
	{
		return;
	}

	NextScanTimeSeconds = NowSeconds + 1.0 / ScanRateHz;
	Sweep(NowSeconds);
}

void URoboLidarComponent::Sweep(double NowSeconds)
{
	// Names this work in Unreal Insights, so its cost is attributable rather
	// than hidden inside the component tick.
	TRACE_CPUPROFILER_EVENT_SCOPE(RoboLidarSweep);
	SCOPE_CYCLE_COUNTER(STAT_RoboLidarSweep);
	
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const double SpanRad = FMath::DegreesToRadians(AngularSpanDeg);

	LatestScan.RangesM.SetNum(BeamCount);
	LatestScan.AngleMinRad = -0.5 * SpanRad;
	LatestScan.AngleIncrementRad = BeamCount > 1 ? SpanRad / (BeamCount - 1) : 0.0;
	LatestScan.RangeMinM = RangeMinM;
	LatestScan.RangeMaxM = RangeMaxM;
	LatestScan.TimeSeconds = NowSeconds;

	const FVector OriginUU = GetComponentLocation();
	const FQuat SensorRotation = GetComponentQuat();
	const double MaxRangeUU = RangeMaxM * RoboFrames::UUPerMeter;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoboLidarSweep), false, GetOwner());

	for (int32 Index = 0; Index < BeamCount; ++Index)
	{
		const double AngleRad = RoboLidar::BeamAngleRad(Index, BeamCount, SpanRad);

		// Beam direction is built in REP-103 and converted, so the sensor's
		// angle convention matches everything else in the simulation.
		const FVector DirectionUe =
			SensorRotation.RotateVector(RoboFrames::PositionToUe(RoboLidar::BeamDirection(AngleRad)).GetSafeNormal());

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit, OriginUU, OriginUU + DirectionUe * MaxRangeUU, ECC_Visibility, Params);

		double RangeM = RoboLidar::NoReturn();

		if (bHit)
		{
			const double MeasuredM = RoboNoise::Gaussian(
				NoiseStream, Hit.Distance * RoboFrames::MetersPerUU, RangeNoiseStdDevM);

			// A real sensor reports nothing when the surface is too dark or too
			// oblique, and nothing inside its minimum range either.
			const bool bDropped = NoiseStream.GetFraction() < DropoutProbability;
			if (!bDropped && MeasuredM >= RangeMinM && MeasuredM <= RangeMaxM)
			{
				RangeM = MeasuredM;
			}
		}

		LatestScan.RangesM[Index] = RangeM;

		if (bDrawScan)
		{
			const bool bReturned = RoboLidar::HasReturn(RangeM);
			const double DrawLengthUU = bReturned ? RangeM * RoboFrames::UUPerMeter : MaxRangeUU;

			DrawDebugLine(World, OriginUU, OriginUU + DirectionUe * DrawLengthUU,
				bReturned ? FColor::Orange : FColor(40, 40, 40),
				false, 1.0f / ScanRateHz, 0, 0.5f);
		}
	}
}