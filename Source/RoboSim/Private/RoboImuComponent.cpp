#include "RoboImuComponent.h"

#include "RoboFrames.h"
#include "RoboSim.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 MaxSamplesPerTick = 32;
}

URoboImuComponent::URoboImuComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboImuComponent::BeginPlay()
{
	Super::BeginPlay();

	Vehicle = Cast<ARoboVehicle>(GetOwner());
	if (Vehicle == nullptr)
	{
		UE_LOG(LogRoboSim, Warning, TEXT("%s is not on a RoboVehicle; IMU disabled."), *GetName());
		return;
	}

	ResetImu();
}

void URoboImuComponent::ResetImu()
{
	NoiseStream.Initialize(RandomSeed);
	LatestSample = FImuSample();
	PreviousVelocityMS = FVector::ZeroVector;
	bHasPreviousVelocity = false;

	if (Vehicle != nullptr)
	{
		// Start aligned with truth, like a robot told its initial heading.
		GyroHeadingRad = RoboFrames::YawToRos(Vehicle->GetActorRotation().Yaw);
	}

	if (const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this))
	{
		NextSampleTimeSeconds = Sim->GetSimTimeSeconds();
	}
}

void URoboImuComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}

	const double IntervalSeconds = 1.0 / SampleRateHz;
	const double NowSeconds = Sim->GetSimTimeSeconds();

	int32 Samples = 0;
	while (NowSeconds >= NextSampleTimeSeconds && Samples < MaxSamplesPerTick)
	{
		SampleOnce(IntervalSeconds, NextSampleTimeSeconds);
		NextSampleTimeSeconds += IntervalSeconds;
		++Samples;
	}

	if (bDrawReadout)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 95.0),
			FString::Printf(TEXT("imu |a|=%.2f m/s2  gyro z=%.3f rad/s  gyro heading err %.1f deg"),
				LatestSample.AccelerationMS2.Size(),
				LatestSample.AngularVelocityRadS.Z,
				FMath::RadiansToDegrees(GetGyroHeadingErrorRad())),
			Vehicle, FColor::Cyan, 0.0f, true);
	}
}

void URoboImuComponent::SampleOnce(double DeltaSeconds, double NowSeconds)
{
	const FVector VelocityMS = Vehicle->GetLinearVelocityMS();

	// Acceleration by differencing velocity. The physics solver does not hand
	// out acceleration directly, and a real IMU is differentiating motion too.
	FVector WorldAccelerationMS2 = FVector::ZeroVector;
	if (bHasPreviousVelocity && DeltaSeconds > 0.0)
	{
		WorldAccelerationMS2 = (VelocityMS - PreviousVelocityMS) / DeltaSeconds;
	}
	PreviousVelocityMS = VelocityMS;
	bHasPreviousVelocity = true;

	const FQuat BodyRotation = Vehicle->GetRotationRos();

	const FVector IdealAccel = RoboImu::ProperAcceleration(WorldAccelerationMS2, BodyRotation);
	const FVector IdealGyro = RoboImu::BodyAngularVelocity(Vehicle->GetAngularVelocityRadS(), BodyRotation);

	LatestSample.AccelerationMS2 =
		RoboImu::Corrupt(IdealAccel, AccelBiasMS2, AccelNoiseStdDevMS2, NoiseStream);
	LatestSample.AngularVelocityRadS =
		RoboImu::Corrupt(IdealGyro, GyroBiasRadS, GyroNoiseStdDevRadS, NoiseStream);
	LatestSample.TimeSeconds = NowSeconds;

	// Dead reckoning from the gyroscope alone. Nothing corrects this, so a
	// bias of a thousandth of a radian per second becomes a degree of heading
	// error every seventeen seconds, for ever.
	GyroHeadingRad = RoboFrames::WrapRadians(
		GyroHeadingRad + LatestSample.AngularVelocityRadS.Z * DeltaSeconds);
}

double URoboImuComponent::GetGyroHeadingErrorRad() const
{
	if (Vehicle == nullptr)
	{
		return 0.0;
	}

	const double TrueHeading = RoboFrames::YawToRos(Vehicle->GetActorRotation().Yaw);
	return RoboFrames::WrapRadians(GyroHeadingRad - TrueHeading);
}