#include "RoboReplayComponent.h"

#include "RoboFrames.h"
#include "RoboSim.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "RoboImuComponent.h"
#include "RoboOdometryComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

URoboReplayComponent::URoboReplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboReplayComponent::BeginPlay()
{
	Super::BeginPlay();
	Vehicle = Cast<ARoboVehicle>(GetOwner());
}

bool URoboReplayComponent::LoadFile(const FString& Path)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		UE_LOG(LogRoboSim, Error, TEXT("Replay: could not read %s"), *Path);
		return false;
	}

	int32 HeaderIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		if (!Lines[Index].StartsWith(TEXT("#")))
		{
			HeaderIndex = Index;
			break;
		}
	}

	if (HeaderIndex == INDEX_NONE)
	{
		return false;
	}

	TArray<FString> Columns;
	Lines[HeaderIndex].ParseIntoArray(Columns, TEXT(","), false);

	// Columns are found by name rather than position, so adding a column to
	// the recorder cannot silently shift what replay reads.
	auto ColumnIndex = [&Columns](const TCHAR* Name)
	{
		return Columns.IndexOfByPredicate([Name](const FString& Column) { return Column.Equals(Name); });
	};

	const int32 TimeCol = ColumnIndex(TEXT("time_s"));
	const int32 LinearCol = ColumnIndex(TEXT("cmd_v_ms"));
	const int32 AngularCol = ColumnIndex(TEXT("cmd_w_rads"));
	const int32 XCol = ColumnIndex(TEXT("true_x_m"));
	const int32 YCol = ColumnIndex(TEXT("true_y_m"));
	const int32 YawCol = ColumnIndex(TEXT("true_yaw_rad"));

	if (TimeCol == INDEX_NONE || LinearCol == INDEX_NONE || AngularCol == INDEX_NONE
		|| XCol == INDEX_NONE || YCol == INDEX_NONE || YawCol == INDEX_NONE)
	{
		UE_LOG(LogRoboSim, Error, TEXT("Replay: %s is missing required columns"), *Path);
		return false;
	}

	Entries.Reset();

	for (int32 Index = HeaderIndex + 1; Index < Lines.Num(); ++Index)
	{
		TArray<FString> Fields;
		Lines[Index].ParseIntoArray(Fields, TEXT(","), false);
		if (Fields.Num() < Columns.Num())
		{
			continue;
		}

		FReplayEntry Entry;
		Entry.TimeSeconds = FCString::Atod(*Fields[TimeCol]);
		Entry.LinearMS = FCString::Atod(*Fields[LinearCol]);
		Entry.AngularRadS = FCString::Atod(*Fields[AngularCol]);
		Entry.TrueX = FCString::Atod(*Fields[XCol]);
		Entry.TrueY = FCString::Atod(*Fields[YCol]);
		Entry.TrueYawRad = FCString::Atod(*Fields[YawCol]);
		Entries.Add(Entry);
	}

	UE_LOG(LogRoboSim, Log, TEXT("Replay: loaded %d entries from %s"), Entries.Num(), *Path);
	return Entries.Num() > 1;
}

void URoboReplayComponent::StartReplay()
{
	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}

	const FString Path = FPaths::IsRelative(ReplayFile)
		? FPaths::ProjectSavedDir() / TEXT("RoboDrive/Runs") / ReplayFile
		: ReplayFile;

	if (!LoadFile(Path))
	{
		return;
	}

	// Same commands from a different starting pose would diverge for a
	// completely uninteresting reason, so put the robot back where it began
	// and take its momentum away.
	const FVector StartUU = RoboFrames::PositionToUe(FVector(Entries[0].TrueX, Entries[0].TrueY, 0.0));
	const FRotator StartRotation(0.0, RoboFrames::YawToUe(Entries[0].TrueYawRad), 0.0);

	Vehicle->SetActorLocationAndRotation(
		FVector(StartUU.X, StartUU.Y, Vehicle->GetActorLocation().Z), StartRotation,
		false, nullptr, ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	NextIndex = 0;
	DivergenceM = 0.0;
	MaxDivergenceM = 0.0;
	StartOffsetSeconds = Sim->GetSimTimeSeconds() - Entries[0].TimeSeconds;

	// The robot has been teleported back to the recorded start pose, so the
	// dead reckoning estimates have to start from there too. Otherwise they
	// carry error from before the replay and the drift being measured is not
	// the drift of this run.
	if (URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
	{
		Odometry->ResetOdometry();
	}

	if (URoboImuComponent* Imu = Vehicle->FindComponentByClass<URoboImuComponent>())
	{
		Imu->ResetImu();
	}
	
	bReplaying = true;
}

void URoboReplayComponent::StopReplay()
{
	if (bReplaying)
	{
		UE_LOG(LogRoboSim, Log, TEXT("Replay finished: final divergence %.3f m, max %.3f m"),
			DivergenceM, MaxDivergenceM);
	}

	bReplaying = false;
}

void URoboReplayComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (!bReplaying || Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}

	const double NowSeconds = Sim->GetSimTimeSeconds();
	const double ReplayTimeSeconds = NowSeconds - StartOffsetSeconds;

	while (NextIndex + 1 < Entries.Num() && Entries[NextIndex + 1].TimeSeconds <= ReplayTimeSeconds)
	{
		++NextIndex;
	}

	if (NextIndex + 1 >= Entries.Num())
	{
		StopReplay();
		return;
	}

	const FReplayEntry& Entry = Entries[NextIndex];

	FDriveCommand Command;
	Command.Linear = Entry.LinearMS;
	Command.Angular = Entry.AngularRadS;
	Command.TimeSeconds = NowSeconds;
	Command.Source = EDriveSource::External;
	Vehicle->SubmitDriveCommand(Command);

	// How far the robot is from where it was at this point in the original run.
	const FVector NowRos = RoboFrames::PositionToRos(Vehicle->GetActorLocation());
	DivergenceM = FVector2D::Distance(
		FVector2D(NowRos.X, NowRos.Y), FVector2D(Entry.TrueX, Entry.TrueY));
	MaxDivergenceM = FMath::Max(MaxDivergenceM, DivergenceM);

	if (bDrawReadout)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 120.0),
			FString::Printf(TEXT("replay t=%.1f  divergence %.3f m (max %.3f)"),
				ReplayTimeSeconds, DivergenceM, MaxDivergenceM),
			Vehicle, FColor::Magenta, 0.0f, true);
	}
}