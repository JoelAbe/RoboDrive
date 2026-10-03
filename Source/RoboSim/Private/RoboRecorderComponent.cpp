#include "RoboRecorderComponent.h"

#include "RoboImuComponent.h"
#include "RoboOdometryComponent.h"
#include "RoboSim.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

URoboRecorderComponent::URoboRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboRecorderComponent::BeginPlay()
{
	Super::BeginPlay();

	Vehicle = Cast<ARoboVehicle>(GetOwner());
	if (Vehicle == nullptr)
	{
		UE_LOG(LogRoboSim, Warning, TEXT("%s is not on a RoboVehicle; recording disabled."), *GetName());
		return;
	}

	Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>();
	Imu = Vehicle->FindComponentByClass<URoboImuComponent>();

	if (bRecordFromStart)
	{
		StartRecording();
	}
}

void URoboRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Stopping play is the usual way a run ends, so write the file rather than
	// silently discarding everything recorded.
	if (bRecording)
	{
		StopRecording();
	}

	Super::EndPlay(EndPlayReason);
}

void URoboRecorderComponent::StartRecording()
{
	Rows.Reset();
	bRecording = true;

	if (const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this))
	{
		NextSampleTimeSeconds = Sim->GetSimTimeSeconds();
	}

	UE_LOG(LogRoboSim, Log, TEXT("Recording started at %.1f Hz"), RecordRateHz);
}

void URoboRecorderComponent::StopRecording()
{
	if (!bRecording)
	{
		return;
	}

	bRecording = false;

	TArray<FString> Lines = BuildMetadataLines();
	Lines.Add(RoboRunRecord::Header());
	Lines.Append(Rows);

	const FString FileName = FString::Printf(TEXT("%s_%s.csv"),
		*RunName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	const FString FullPath = FPaths::ProjectSavedDir() / TEXT("RoboDrive/Runs") / FileName;

	if (FFileHelper::SaveStringArrayToFile(Lines, *FullPath))
	{
		UE_LOG(LogRoboSim, Log, TEXT("Wrote %d samples to %s"), Rows.Num(), *FullPath);
	}
	else
	{
		UE_LOG(LogRoboSim, Error, TEXT("Failed to write run file to %s"), *FullPath);
	}

	Rows.Reset();
}

TArray<FString> URoboRecorderComponent::BuildMetadataLines() const
{
	// Comment lines, which pandas skips with comment='#'. A run without the
	// settings that produced it cannot be reproduced or compared.
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("# recorded %s"), *FDateTime::Now().ToString()));
	Lines.Add(FString::Printf(TEXT("# record_rate_hz=%.3f"), RecordRateHz));

	if (Vehicle != nullptr)
	{
		Lines.Add(FString::Printf(TEXT("# mass_kg=%.3f wheel_separation_m=%.4f wheel_radius_m=%.4f"),
			Vehicle->MassKg, Vehicle->GetWheelSeparationM(), Vehicle->GetWheelRadiusM()));
	}

	if (Odometry != nullptr)
	{
		Lines.Add(FString::Printf(
			TEXT("# odom_rate_hz=%.1f ticks_per_rev=%d encoder_noise_rads=%.5f ")
			TEXT("radius_error_factor=%.5f separation_error_factor=%.5f seed=%d"),
			Odometry->SampleRateHz, Odometry->TicksPerRevolution,
			Odometry->EncoderNoiseStdDevRadS, Odometry->WheelRadiusErrorFactor,
			Odometry->WheelSeparationErrorFactor, Odometry->RandomSeed));
	}

	if (Imu != nullptr)
	{
		Lines.Add(FString::Printf(
			TEXT("# imu_rate_hz=%.1f gyro_noise_rads=%.5f gyro_bias_z_rads=%.5f seed=%d"),
			Imu->SampleRateHz, Imu->GyroNoiseStdDevRadS, Imu->GyroBiasRadS.Z, Imu->RandomSeed));
	}

	return Lines;
}

void URoboRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (!bRecording || Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}

	const double NowSeconds = Sim->GetSimTimeSeconds();
	if (NowSeconds < NextSampleTimeSeconds)
	{
		return;
	}

	NextSampleTimeSeconds = NowSeconds + 1.0 / RecordRateHz;
	Rows.Add(RoboRunRecord::Format(TakeSample(NowSeconds, Sim->GetStepCount())));
}

FRunSample URoboRecorderComponent::TakeSample(double NowSeconds, int64 Step) const
{
	FRunSample Sample;
	Sample.TimeSeconds = NowSeconds;
	Sample.Step = Step;
	Sample.SpeedMS = Vehicle->GetSpeedMS();

	const FDriveCommand Command = Vehicle->GetActiveCommand();
	Sample.CommandLinearMS = Command.Linear;
	Sample.CommandAngularRadS = Command.Angular;
	Sample.CommandSource = static_cast<int32>(Command.Source);

	if (Odometry != nullptr)
	{
		const FPose2D True = Odometry->GetTruePose();
		const FPose2D Odom = Odometry->GetEstimatedPose();

		Sample.TrueX = True.X;
		Sample.TrueY = True.Y;
		Sample.TrueYawRad = True.YawRad;
		Sample.OdomX = Odom.X;
		Sample.OdomY = Odom.Y;
		Sample.OdomYawRad = Odom.YawRad;
		Sample.PositionErrorM = Odometry->GetPositionErrorM();
		Sample.HeadingErrorRad = Odometry->GetHeadingErrorRad();
		Sample.TravelledM = Odometry->GetTravelledM();
	}

	if (Imu != nullptr)
	{
		Sample.GyroHeadingRad = Imu->GetGyroHeadingRad();
		Sample.GyroHeadingErrorRad = Imu->GetGyroHeadingErrorRad();
	}

	const RoboDiffDrive::FWheelSpeeds Commanded = Vehicle->GetCommandedWheelSpeeds();
	Sample.CommandedWheelLeftMS = Commanded.Left;
	Sample.CommandedWheelRightMS = Commanded.Right;

	const FVector2D Measured = Vehicle->GetMeasuredWheelSpeeds();
	Sample.MeasuredWheelLeftMS = Measured.X;
	Sample.MeasuredWheelRightMS = Measured.Y;
	
	return Sample;
}