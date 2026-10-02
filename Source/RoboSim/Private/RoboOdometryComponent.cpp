#include "RoboOdometryComponent.h"

#include "RoboDiffDrive.h"
#include "RoboFrames.h"
#include "RoboSim.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace
{
	/** Trails are drawn slightly above the ground so they are not z-fighting it. */
	constexpr double TrailHeightUU = 3.0;

	/** Never run more than this many catch-up samples in one frame. */
	constexpr int32 OdometryMaxSamplesPerTick = 16;
}

URoboOdometryComponent::URoboOdometryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboOdometryComponent::BeginPlay()
{
	Super::BeginPlay();

	Vehicle = Cast<ARoboVehicle>(GetOwner());
	if (Vehicle == nullptr)
	{
		UE_LOG(LogRoboSim, Warning,
			TEXT("%s is not attached to a RoboVehicle; odometry disabled."), *GetName());
		return;
	}

	bRunning = false;
	ResetOdometry();
}

void URoboOdometryComponent::ResetOdometry()
{
	LeftEncoder = FWheelEncoder(TicksPerRevolution);
	RightEncoder = FWheelEncoder(TicksPerRevolution);
	NoiseStream.Initialize(RandomSeed);

	// The estimate starts correct. Everything after this is accumulated error.
	TruePose = ReadTruePose();
	EstimatedPose = TruePose;

	TravelledM = 0.0;

	bHasTrailPoints = false;

	if (const UWorld* World = GetWorld())
	{
		FlushPersistentDebugLines(World);
	}

	if (const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this))
	{
		NextSampleTimeSeconds = Sim->GetSimTimeSeconds();
	}
}

FPose2D URoboOdometryComponent::ReadTruePose() const
{
	FPose2D Pose;
	if (Vehicle == nullptr)
	{
		return Pose;
	}

	const FVector PositionM = RoboFrames::PositionToRos(Vehicle->GetActorLocation());
	Pose.X = PositionM.X;
	Pose.Y = PositionM.Y;
	Pose.YawRad = RoboFrames::YawToRos(Vehicle->GetActorRotation().Yaw);
	return Pose;
}

void URoboOdometryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}

	if (!bRunning)
	{
		// Spawning drops the robot, and it slides and rocks as the suspension
		// settles. That is real motion no wheel was commanded to make, so
		// counting it as drift measures the drop rather than the odometry.
		if (bStartWhenSettled && Vehicle->GetSpeedMS() > SettledSpeedThresholdMS)
		{
			return;
		}

		ResetOdometry();
		bRunning = true;
	}

	// Sample on the encoder's own schedule, not the frame's. Real encoders are
	// polled at a fixed rate, and tying sensor timing to frame rate is the
	// mistake that makes a simulation subtly untrue.
	const double IntervalSeconds = 1.0 / SampleRateHz;
	const double NowSeconds = Sim->GetSimTimeSeconds();

	int32 SamplesThisTick = 0;
	while (NowSeconds >= NextSampleTimeSeconds && SamplesThisTick < OdometryMaxSamplesPerTick)
	{
		SampleOnce(IntervalSeconds);
		NextSampleTimeSeconds += IntervalSeconds;
		++SamplesThisTick;
	}

	if (bDrawReadout)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 70.0),
	FString::Printf(TEXT("odom drift %.2f m over %.1f m (%.1f%%)  heading %.1f deg"),
		GetPositionErrorM(), GetTravelledM(),
		GetTravelledM() > 0.01 ? 100.0 * GetPositionErrorM() / GetTravelledM() : 0.0,
		FMath::RadiansToDegrees(GetHeadingErrorRad())),
	Vehicle, FColor::Yellow, 0.0f, true);
	}
}

void URoboOdometryComponent::SampleOnce(double DeltaSeconds)
{
	const double TrueRadiusM = Vehicle->GetWheelRadiusM();
	if (TrueRadiusM <= 0.0)
	{
		return;
	}

	// What the wheels were told to do, as rotation rates.
	const RoboDiffDrive::FWheelSpeeds Commanded = Vehicle->GetCommandedWheelSpeeds();
	const double LeftRadS = RoboDiffDrive::ToWheelAngularSpeed(Commanded.Left, TrueRadiusM);
	const double RightRadS = RoboDiffDrive::ToWheelAngularSpeed(Commanded.Right, TrueRadiusM);

	const int64 LeftTicks = LeftEncoder.Update(LeftRadS, DeltaSeconds, NoiseStream, EncoderNoiseStdDevRadS);
	const int64 RightTicks = RightEncoder.Update(RightRadS, DeltaSeconds, NoiseStream, EncoderNoiseStdDevRadS);

	// Ticks become distance using what the software BELIEVES the wheel is.
	const double AssumedRadiusM = TrueRadiusM * WheelRadiusErrorFactor;
	const double LeftDistanceM = FWheelEncoder::TicksToDistanceM(LeftTicks, TicksPerRevolution, AssumedRadiusM);
	const double RightDistanceM = FWheelEncoder::TicksToDistanceM(RightTicks, TicksPerRevolution, AssumedRadiusM);

	// Path length, not displacement: this is the denominator for "error per
	// metre travelled", which is how odometry accuracy is normally quoted.
	TravelledM += FMath::Abs(0.5 * (LeftDistanceM + RightDistanceM));
	
	// Forward kinematics: the same equations that command the wheels, used
	// backwards to infer how the body must have moved.
	const RoboDiffDrive::FWheelSpeeds MeasuredSpeeds{
		LeftDistanceM / DeltaSeconds,
		RightDistanceM / DeltaSeconds
	};

	const double AssumedSeparationM = Vehicle->GetWheelSeparationM() * WheelSeparationErrorFactor;
	const RoboDiffDrive::FBodyVelocity MeasuredVelocity =
		RoboDiffDrive::ToBodyVelocity(MeasuredSpeeds, AssumedSeparationM);

	EstimatedPose = RoboOdometry::Integrate(EstimatedPose, MeasuredVelocity, DeltaSeconds);
	TruePose = ReadTruePose();

	if (!bDrawTrails)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const FVector TruthPointUU =
		RoboFrames::PositionToUe(FVector(TruePose.X, TruePose.Y, 0.0)) + FVector(0, 0, TrailHeightUU);
	const FVector EstimatePointUU =
		RoboFrames::PositionToUe(FVector(EstimatedPose.X, EstimatedPose.Y, 0.0)) + FVector(0, 0, TrailHeightUU);

	if (bHasTrailPoints)
	{
		DrawDebugLine(World, LastTruthPointUU, TruthPointUU, FColor::Green, true, -1.0f, 0, 2.0f);
		DrawDebugLine(World, LastEstimatePointUU, EstimatePointUU, FColor::Red, true, -1.0f, 0, 2.0f);
	}

	LastTruthPointUU = TruthPointUU;
	LastEstimatePointUU = EstimatePointUU;
	bHasTrailPoints = true;
}

double URoboOdometryComponent::GetPositionErrorM() const
{
	return RoboOdometry::PositionErrorM(EstimatedPose, TruePose);
}

double URoboOdometryComponent::GetHeadingErrorRad() const
{
	return RoboOdometry::HeadingErrorRad(EstimatedPose, TruePose);
}