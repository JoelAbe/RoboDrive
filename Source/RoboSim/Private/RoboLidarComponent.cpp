#include "RoboLidarComponent.h"

#include "RoboFrames.h"
#include "RoboFog.h"
#include "RoboWeather.h"
#include "RoboNoise.h"
#include "RoboSimSubsystem.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "DrawDebugHelpers.h"
#include "RoboSim.h"
#include "Engine/World.h"

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

	// Read once per sweep: the weather does not change partway through one.
	const URoboWeatherSubsystem* Weather = URoboWeatherSubsystem::Get(this);
	const double ExtinctionPerM = Weather != nullptr ? Weather->GetExtinctionPerM() : 0.0;
	
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

		// What the beam really meets first: a surface, if the fog leaves its
		// return strong enough to detect, or the fog itself.
		double TrueRangeM = RoboLidar::NoReturn();
		bool bFogEcho = false;

		if (bHit)
		{
			const double SurfaceM = Hit.Distance * RoboFrames::MetersPerUU;
			if (RoboFog::IsDetectable(SurfaceM, RangeMaxM, ExtinctionPerM))
			{
				TrueRangeM = SurfaceM;
			}
		}

		// An echo only counts if it comes before the surface, is beyond the
		// minimum range a real sensor can measure, and is itself strong enough.
		const double EchoM = RoboFog::SampleEchoM(NoiseStream, ExtinctionPerM, FogEchoFraction);
		if (EchoM >= RangeMinM && EchoM < TrueRangeM
			&& RoboFog::IsDetectable(EchoM, RangeMaxM, ExtinctionPerM))
		{
			TrueRangeM = EchoM;
			bFogEcho = true;
		}

		double RangeM = RoboLidar::NoReturn();

		if (RoboLidar::HasReturn(TrueRangeM))
		{
			const double MeasuredM = RoboNoise::Gaussian(NoiseStream, TrueRangeM, RangeNoiseStdDevM);

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

			// Fog echoes are drawn cyan. The sensor cannot tell them apart from
			// real returns, and neither can the brain; only the simulation, which
			// knows the truth, can colour them differently for a person watching.
			const FColor Colour = !bReturned ? FColor(40, 40, 40)
				: bFogEcho ? FColor::Cyan
				: FColor::Orange;

			DrawDebugLine(World, OriginUU, OriginUU + DirectionUe * DrawLengthUU,
				Colour, false, 1.0f / ScanRateHz, 0, 0.5f);
		}
	}
}