#include "RoboScenario.h"

#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "RoboFrames.h"
#include "RoboImuComponent.h"
#include "RoboMission.h"
#include "RoboObstacle.h"
#include "RoboOdometryComponent.h"
#include "RoboSim.h"
#include "RoboVehicle.h"
#include "RoboWeather.h"

URoboScenarioSubsystem* URoboScenarioSubsystem::Get(const UObject* WorldContext)
{
	if (GEngine == nullptr || WorldContext == nullptr)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContext, EGetWorldErrorMode::ReturnNull);

	return World != nullptr ? World->GetSubsystem<URoboScenarioSubsystem>() : nullptr;
}

bool URoboScenarioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URoboScenarioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Record the spawn pose now, while the robot is still standing where the
	// level placed it, rather than whenever it is first needed.
	FindVehicle();
}

ARoboVehicle* URoboScenarioSubsystem::FindVehicle()
{
	if (CachedVehicle.IsValid())
	{
		return CachedVehicle.Get();
	}

	for (TActorIterator<ARoboVehicle> It(GetWorld()); It; ++It)
	{
		CachedVehicle = *It;

		if (!bHasSpawnPose)
		{
			SpawnLocationUU = It->GetActorLocation();
			SpawnRotation = It->GetActorRotation();
			bHasSpawnPose = true;
		}

		return CachedVehicle.Get();
	}

	return nullptr;
}

bool URoboScenarioSubsystem::Validate(const FJsonObject& Scenario, FString& OutError)
{
	// Refuse anything not written for this version of the format, rather
	// than half-understanding it.
	double SchemaVersion = 0.0;
	if (!Scenario.TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| FMath::RoundToInt(SchemaVersion) != 1)
	{
		OutError = TEXT("missing or unsupported schema_version (expected 1)");
		return false;
	}

	const TSharedPtr<FJsonObject>* Start = nullptr;
	if (!Scenario.TryGetObjectField(TEXT("start"), Start))
	{
		OutError = TEXT("scenario has no 'start'");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Obstacles = nullptr;
	if (Scenario.TryGetArrayField(TEXT("obstacles"), Obstacles))
	{
		for (int32 Index = 0; Index < Obstacles->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Obstacle = (*Obstacles)[Index]->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;

			if (!Obstacle.IsValid()
				|| !Obstacle->HasField(TEXT("x")) || !Obstacle->HasField(TEXT("y"))
				|| !Obstacle->TryGetArrayField(TEXT("size"), Size) || Size->Num() != 3)
			{
				OutError = FString::Printf(
					TEXT("obstacle %d needs x, y and a three-element size"), Index);
				return false;
			}
		}
	}

	// Visibility is optional, and absent means clear air. If present it must be
	// a positive number: zero or negative visibility has no physical meaning.
	double VisibilityM = 0.0;
	if (Scenario.HasField(TEXT("visibility_m"))
		&& (!Scenario.TryGetNumberField(TEXT("visibility_m"), VisibilityM) || VisibilityM <= 0.0))
	{
		OutError = TEXT("visibility_m must be a positive number");
		return false;
	}

	return true;
}

bool URoboScenarioSubsystem::Load(const TSharedRef<FJsonObject>& Scenario, FString& OutError,
	const FString& FallbackName)
{
	ARoboVehicle* Vehicle = FindVehicle();
	if (Vehicle == nullptr)
	{
		OutError = TEXT("no robot in the world");
		return false;
	}

	// Validate everything before changing anything, so a malformed scenario
	// leaves the previous world intact instead of half-replaced.
	if (!Validate(*Scenario, OutError))
	{
		return false;
	}

	ClearObstacles();

	// Obstacles. Positions and sizes are REP-103 metres, converted at this
	// boundary like everything else.
	const TArray<TSharedPtr<FJsonValue>>* Obstacles = nullptr;
	if (Scenario->TryGetArrayField(TEXT("obstacles"), Obstacles))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Obstacles)
		{
			const TSharedPtr<FJsonObject> Obstacle = Value->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Size = nullptr;
			Obstacle->TryGetArrayField(TEXT("size"), Size);

			const FVector SizeM((*Size)[0]->AsNumber(), (*Size)[1]->AsNumber(), (*Size)[2]->AsNumber());

			double Yaw = 0.0;
			Obstacle->TryGetNumberField(TEXT("yaw"), Yaw);

			const FVector PositionUU = RoboFrames::PositionToUe(FVector(
				Obstacle->GetNumberField(TEXT("x")), Obstacle->GetNumberField(TEXT("y")), 0.0));

			// Sit the box on the floor: its origin is at its centre.
			const double CentreZ = GroundHeightUU(PositionUU.X, PositionUU.Y)
				+ 0.5 * SizeM.Z * RoboFrames::UUPerMeter;

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			ARoboObstacle* Spawned = GetWorld()->SpawnActor<ARoboObstacle>(
				FVector(PositionUU.X, PositionUU.Y, CentreZ),
				FRotator(0.0, RoboFrames::YawToUe(Yaw), 0.0),
				Params);

			if (Spawned != nullptr)
			{
				Spawned->SetSizeM(SizeM);
				ScenarioActors.Add(Spawned);
			}
		}
	}

	// The robot's starting pose. Validate has already checked it exists.
	const TSharedPtr<FJsonObject>& Start = Scenario->GetObjectField(TEXT("start"));
	double StartX = 0.0, StartY = 0.0, StartYaw = 0.0;
	Start->TryGetNumberField(TEXT("x"), StartX);
	Start->TryGetNumberField(TEXT("y"), StartY);
	Start->TryGetNumberField(TEXT("yaw"), StartYaw);

	ResetRobotTo(StartX, StartY, StartYaw, OutError);

	// The goal, drawn so a person watching can see what the robot is meant to
	// reach. Must come after the reset, which clears persistent debug lines.
	URoboMissionSubsystem* Mission = GetWorld()->GetSubsystem<URoboMissionSubsystem>();

	const TSharedPtr<FJsonObject>* Goal = nullptr;
	if (Scenario->TryGetObjectField(TEXT("goal"), Goal))
	{
		double GoalX = 0.0, GoalY = 0.0, ToleranceM = 0.5;
		(*Goal)->TryGetNumberField(TEXT("x"), GoalX);
		(*Goal)->TryGetNumberField(TEXT("y"), GoalY);
		(*Goal)->TryGetNumberField(TEXT("tolerance_m"), ToleranceM);

		const FVector GoalUU = RoboFrames::PositionToUe(FVector(GoalX, GoalY, 0.0));
		const double GroundZ = GroundHeightUU(GoalUU.X, GoalUU.Y);

		DrawDebugCylinder(GetWorld(),
			FVector(GoalUU.X, GoalUU.Y, GroundZ),
			FVector(GoalUU.X, GoalUU.Y, GroundZ + 50.0),
			ToleranceM * RoboFrames::UUPerMeter, 32, FColor::Green, true, -1.0f, 0, 2.0f);

		if (Mission != nullptr)
		{
			FMissionGoal MissionGoal;
			MissionGoal.X = GoalX;
			MissionGoal.Y = GoalY;
			MissionGoal.ToleranceM = ToleranceM;
			Mission->SetGoal(MissionGoal);
		}
	}
	else if (Mission != nullptr)
	{
		// A scenario without a goal must not inherit the previous scenario's.
		Mission->ClearGoal();
	}

	// The weather. Likewise, a scenario without fog must not inherit the
	// previous one's.
	if (URoboWeatherSubsystem* Weather = GetWorld()->GetSubsystem<URoboWeatherSubsystem>())
	{
		double VisibilityM = 0.0;
		if (Scenario->TryGetNumberField(TEXT("visibility_m"), VisibilityM))
		{
			Weather->SetFog(VisibilityM);
		}
		else
		{
			Weather->SetClear();
		}
	}

	// Remembered last, once it has loaded, so Restart always has something
	// that is known to work.
	Current = Scenario;
	CurrentName = FallbackName;
	Scenario->TryGetStringField(TEXT("name"), CurrentName);

	UE_LOG(LogRoboSim, Log, TEXT("Scenario '%s' loaded: %d obstacles"), *CurrentName, ScenarioActors.Num());
	return true;
}

bool URoboScenarioSubsystem::Restart(FString& OutError)
{
	if (!Current.IsValid())
	{
		return ResetRobot(OutError);
	}

	// Load from a copy of the pointer: Load replaces Current as it finishes.
	const TSharedRef<FJsonObject> Scenario = Current.ToSharedRef();
	return Load(Scenario, OutError, CurrentName);
}

bool URoboScenarioSubsystem::ResetRobot(FString& OutError)
{
	ARoboVehicle* Vehicle = FindVehicle();
	if (Vehicle == nullptr)
	{
		OutError = TEXT("no robot in the world");
		return false;
	}

	TeleportRobot(*Vehicle, SpawnLocationUU, SpawnRotation);
	return true;
}

bool URoboScenarioSubsystem::ResetRobotTo(double X, double Y, double YawRad, FString& OutError)
{
	ARoboVehicle* Vehicle = FindVehicle();
	if (Vehicle == nullptr)
	{
		OutError = TEXT("no robot in the world");
		return false;
	}

	// The height it spawned at, which is resting on the floor. The robot's
	// height now may be mid-bounce.
	const FVector RequestedUU = RoboFrames::PositionToUe(FVector(X, Y, 0.0));
	const double HeightUU = bHasSpawnPose ? SpawnLocationUU.Z : Vehicle->GetActorLocation().Z;

	TeleportRobot(*Vehicle,
		FVector(RequestedUU.X, RequestedUU.Y, HeightUU),
		FRotator(0.0, RoboFrames::YawToUe(YawRad), 0.0));
	return true;
}

void URoboScenarioSubsystem::TeleportRobot(ARoboVehicle& Vehicle, const FVector& LocationUU, const FRotator& Rotation)
{
	Vehicle.SetActorLocationAndRotation(LocationUU, Rotation, false, nullptr,
		ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Vehicle.GetRootComponent()))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// After the teleport, so the estimates start from where the robot now is.
	if (URoboOdometryComponent* Odometry = Vehicle.FindComponentByClass<URoboOdometryComponent>())
	{
		Odometry->ResetOdometry();
	}

	if (URoboImuComponent* Imu = Vehicle.FindComponentByClass<URoboImuComponent>())
	{
		Imu->ResetImu();
	}
}

double URoboScenarioSubsystem::GroundHeightUU(double XUU, double YUU) const
{
	// Trace for WorldStatic objects only. Scenario obstacles are WorldDynamic,
	// so a box placed a moment ago cannot be mistaken for the floor and have
	// the next one stacked on top of it.
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByObjectType(
		Hit,
		FVector(XUU, YUU, 10000.0),
		FVector(XUU, YUU, -10000.0),
		FCollisionObjectQueryParams(ECC_WorldStatic));

	return bHit ? Hit.ImpactPoint.Z : 0.0;
}

void URoboScenarioSubsystem::ClearObstacles()
{
	for (const TWeakObjectPtr<AActor>& Actor : ScenarioActors)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}

	ScenarioActors.Reset();
}
