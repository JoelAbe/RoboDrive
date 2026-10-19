#include "RoboApiSubsystem.h"

#include "Common/TcpSocketBuilder.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "RoboApi.h"
#include "RoboApiSettings.h"
#include "RoboImuComponent.h"
#include "RoboLidar.h"
#include "RoboLidarComponent.h"
#include "RoboOdometry.h"
#include "RoboOdometryComponent.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "RoboMission.h"
#include "DrawDebugHelpers.h"
#include "RoboObstacle.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
	/** One request or response per line, so framing needs no length prefix. */
	constexpr uint8 LineTerminator = '\n';
	constexpr int32 ApiReceiveChunkBytes = 8192;

	/** A planar pose as {x, y, yaw}, REP-103 metres and radians. */
	TSharedRef<FJsonObject> ApiMakePose(const FPose2D& Pose)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("x"), Pose.X);
		Object->SetNumberField(TEXT("y"), Pose.Y);
		Object->SetNumberField(TEXT("yaw"), Pose.YawRad);
		return Object;
	}

	/**
	 * Wire names for mission statuses. Spelled out rather than taken from the
	 * enum's display names, because these strings are part of the API contract:
	 * renaming the C++ enum must not silently change what clients receive.
	 */
	const TCHAR* ApiMissionStatusName(EMissionStatus Status)
	{
		switch (Status)
		{
		case EMissionStatus::Active:	return TEXT("active");
		case EMissionStatus::Arrived:	return TEXT("arrived");
		case EMissionStatus::Failed:	return TEXT("failed");
		default:						return TEXT("none");
		}
	}
	
	/** A vector as a three-element JSON array. */
	TArray<TSharedPtr<FJsonValue>> ApiMakeVector(const FVector& Vector)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(Vector.X));
		Values.Add(MakeShared<FJsonValueNumber>(Vector.Y));
		Values.Add(MakeShared<FJsonValueNumber>(Vector.Z));
		return Values;
	}
}

bool URoboApiSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId URoboApiSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoboApiSubsystem, STATGROUP_Tickables);
}

void URoboApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	StartListening();
}

void URoboApiSubsystem::Deinitialize()
{
	StopListening();
	Super::Deinitialize();
}
bool URoboApiSubsystem::StartListening()
{
	const URoboApiSettings* Settings = GetDefault<URoboApiSettings>();
	if (!Settings->bAutoStart)
	{
		return false;
	}

	FIPv4Address Address;
	if (!FIPv4Address::Parse(Settings->BindAddress, Address))
	{
		UE_LOG(LogRoboApi, Error, TEXT("Invalid bind address '%s'"), *Settings->BindAddress);
		return false;
	}

	if (Address != FIPv4Address::InternalLoopback)
	{
		// Not forbidden, because a deliberate setup may want it, but it should
		// never happen by accident and silently.
		UE_LOG(LogRoboApi, Warning,
			TEXT("API bound to %s, which is reachable beyond this machine. There is no authentication."),
			*Settings->BindAddress);
	}

	BoundPort = Settings->ResolvePort();

	ListenSocket = FTcpSocketBuilder(TEXT("RoboApiListener"))
		.AsNonBlocking()
		.AsReusable()
		.BoundToAddress(Address)
		.BoundToPort(BoundPort)
		.Listening(1)
		.Build();

	if (ListenSocket == nullptr)
	{
		UE_LOG(LogRoboApi, Error, TEXT("Could not listen on %s:%d"), *Settings->BindAddress, BoundPort);
		return false;
	}

	UE_LOG(LogRoboApi, Log, TEXT("API listening on %s:%d"), *Settings->BindAddress, BoundPort);
	return true;
}

void URoboApiSubsystem::StopListening()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	for (FSocket** Socket : { &ClientSocket, &ListenSocket })
	{
		if (*Socket != nullptr)
		{
			(*Socket)->Close();
			if (Sockets != nullptr)
			{
				Sockets->DestroySocket(*Socket);
			}
			*Socket = nullptr;
		}
	}
}

void URoboApiSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ListenSocket == nullptr)
	{
		return;
	}

	AcceptPendingClient();
	ReadFromClient();
	CompletePendingStep();
}

void URoboApiSubsystem::AcceptPendingClient()
{
	bool bPending = false;
	if (!ListenSocket->HasPendingConnection(bPending) || !bPending)
	{
		return;
	}

	FSocket* Incoming = ListenSocket->Accept(TEXT("RoboApiClient"));
	if (Incoming == nullptr)
	{
		return;
	}
	
	if (ClientSocket != nullptr)
	{
		UE_LOG(LogRoboApi, Warning, TEXT("Replacing existing API client"));
		CloseClient();
	}

	ClientSocket = Incoming;
	ReceiveBuffer.Reset();
	UE_LOG(LogRoboApi, Log, TEXT("API client connected"));
}

void URoboApiSubsystem::CloseClient()
{
	if (ClientSocket == nullptr)
	{
		return;
	}

	ClientSocket->Close();
	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		Sockets->DestroySocket(ClientSocket);
	}
	ClientSocket = nullptr;
	ReceiveBuffer.Reset();

	// A client that disappears mid-step is owed a reply that will never be
	// sent. Clearing it here stops the next client's first step being answered
	// instantly with the previous session's target.
	bStepPending = false;

	UE_LOG(LogRoboApi, Log, TEXT("API client disconnected"));
}

void URoboApiSubsystem::ReadFromClient()
{
	if (ClientSocket == nullptr)
	{
		return;
	}

	uint32 PendingBytes = 0;
	while (ClientSocket->HasPendingData(PendingBytes))
	{
		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(FMath::Min(static_cast<int32>(PendingBytes), ApiReceiveChunkBytes));

		int32 BytesRead = 0;
		if (!ClientSocket->Recv(Chunk.GetData(), Chunk.Num(), BytesRead) || BytesRead <= 0)
		{
			// A failed read on a socket that claimed to have data means the
			// peer has gone.
			CloseClient();
			return;
		}

		ReceiveBuffer.Append(Chunk.GetData(), BytesRead);
	}

	// A closed connection reports no pending data at all, so the loop above
	// would never notice it. The connection state is the other half of the
	// signal.
	if (ClientSocket->GetConnectionState() == SCS_ConnectionError)
	{
		CloseClient();
		return;
	}

	// TCP is a stream, not a sequence of messages: one read may contain half a
	// request or three of them. The newline is what restores the boundaries.
	int32 Newline = INDEX_NONE;
	while (ReceiveBuffer.Find(LineTerminator, Newline))
	{
		const FString Line = FString(FUTF8ToTCHAR(
			reinterpret_cast<const ANSICHAR*>(ReceiveBuffer.GetData()), Newline));

		ReceiveBuffer.RemoveAt(0, Newline + 1, EAllowShrinking::No);

		if (!Line.IsEmpty())
		{
			HandleLine(Line);
		}
	}
}

void URoboApiSubsystem::HandleLine(const FString& Line)
{
	TSharedPtr<FJsonObject> Request;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();

	if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid())
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), TEXT("malformed JSON"));
		SendResponse(Response);
		return;
	}
	
	double RequestId = 0.0;
	Request->TryGetNumberField(TEXT("id"), RequestId);
	Response->SetNumberField(TEXT("id"), RequestId);

	FString Operation;
	if (!Request->TryGetStringField(TEXT("op"), Operation))
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), TEXT("missing 'op'"));
		SendResponse(Response);
		return;
	}
	
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	FString Error;
	bool bDeferred = false;

	const bool bSucceeded = Dispatch(Operation, Request, Result, Error, bDeferred);

	if (bSucceeded && bDeferred)
	{
		// The reply goes out when the simulation reaches the requested step.
		return;
	}
	
	if (bSucceeded)
	{
		Response->SetBoolField(TEXT("ok"), true);
		Response->SetObjectField(TEXT("result"), Result);
	}
	else
	{
		Response->SetBoolField(TEXT("ok"), false);
		Response->SetStringField(TEXT("error"), Error);
	}

	SendResponse(Response);
}

bool URoboApiSubsystem::Dispatch(const FString& Operation, const TSharedPtr<FJsonObject>& Request,
	TSharedRef<FJsonObject>& OutResult, FString& OutError, bool& bOutDeferred)
{
	if (Operation == TEXT("ping"))
	{
		OutResult->SetStringField(TEXT("pong"), TEXT("RoboDrive"));
		return true;
	}

	const URoboSimSubsystem* Sim = GetWorld()->GetSubsystem<URoboSimSubsystem>();
	if (Sim == nullptr)
	{
		OutError = TEXT("simulation subsystem unavailable");
		return false;
	}
	ARoboVehicle* Vehicle = FindVehicle();
	URoboMissionSubsystem* Mission = GetWorld()->GetSubsystem<URoboMissionSubsystem>();
	
	if (Operation == TEXT("get_state"))
	{

		OutResult->SetNumberField(TEXT("sim_time"), Sim->GetSimTimeSeconds());
		OutResult->SetNumberField(TEXT("step"), static_cast<double>(Sim->GetStepCount()));
		OutResult->SetNumberField(TEXT("real_time_factor"), Sim->GetRealTimeFactor());

		if (Vehicle != nullptr)
		{
			OutResult->SetStringField(TEXT("robot"), Vehicle->GetName());
			OutResult->SetNumberField(TEXT("speed_ms"), Vehicle->GetSpeedMS());
		}

		if (Mission != nullptr)
		{
			TSharedRef<FJsonObject> MissionObject = MakeShared<FJsonObject>();
			MissionObject->SetStringField(TEXT("status"), ApiMissionStatusName(Mission->GetStatus()));

			if (Mission->HasGoal())
			{
				const FMissionGoal Goal = Mission->GetGoal();

				TSharedRef<FJsonObject> GoalObject = MakeShared<FJsonObject>();
				GoalObject->SetNumberField(TEXT("x"), Goal.X);
				GoalObject->SetNumberField(TEXT("y"), Goal.Y);
				GoalObject->SetNumberField(TEXT("tolerance_m"), Goal.ToleranceM);
				MissionObject->SetObjectField(TEXT("goal"), GoalObject);
			}

			OutResult->SetObjectField(TEXT("mission"), MissionObject);
		}

		// Null in clear air rather than absent, so a client can tell "clear"
		// apart from "a simulator too old to report weather".
		if (const URoboWeatherSubsystem* Weather = GetWorld()->GetSubsystem<URoboWeatherSubsystem>())
		{
			if (Weather->HasFog())
			{
				OutResult->SetNumberField(TEXT("visibility_m"), Weather->GetVisibilityM());
			}
			else
			{
				OutResult->SetField(TEXT("visibility_m"), MakeShared<FJsonValueNull>());
			}
		}
		
		return true;
	}

	if (Operation == TEXT("reset"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		// Default to where the robot spawned; a scenario can ask for a pose.
		FVector LocationUU = SpawnLocationUU;
		FRotator Rotation = SpawnRotation;

		double X = 0.0, Y = 0.0, Yaw = 0.0;
		if (Request->TryGetNumberField(TEXT("x"), X) && Request->TryGetNumberField(TEXT("y"), Y))
		{
			Request->TryGetNumberField(TEXT("yaw"), Yaw);
			const FVector RequestedUU = RoboFrames::PositionToUe(FVector(X, Y, 0.0));
			LocationUU = FVector(RequestedUU.X, RequestedUU.Y, SpawnLocationUU.Z);
			Rotation = FRotator(0.0, RoboFrames::YawToUe(Yaw), 0.0);
		}

		ResetRobotTo(Vehicle, LocationUU, Rotation);

		// Sim time deliberately keeps running: rewinding it would strand every
		// sensor that has already scheduled its next sample.
		OutResult->SetBoolField(TEXT("reset"), true);
		return true;
	}

	if (Operation == TEXT("set_command"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		double Linear = 0.0;
		double Angular = 0.0;
		if (!Request->TryGetNumberField(TEXT("v"), Linear) || !Request->TryGetNumberField(TEXT("w"), Angular))
		{
			OutError = TEXT("set_command needs both v and w");
			return false;
		}

		FDriveCommand Command;
		Command.Linear = Linear;
		Command.Angular = Angular;
		Command.TimeSeconds = Sim->GetSimTimeSeconds();
		Command.Source = EDriveSource::External;

		Vehicle->SubmitDriveCommand(Command);

		OutResult->SetNumberField(TEXT("v"), Command.Linear);
		OutResult->SetNumberField(TEXT("w"), Command.Angular);
		return true;
	}

	if (Operation == TEXT("step"))
	{
		if (bStepPending)
		{
			OutError = TEXT("a step is already in progress");
			return false;
		}

		double RequestedSteps = 1.0;
		Request->TryGetNumberField(TEXT("steps"), RequestedSteps);

		StepTargetCount = Sim->GetStepCount() + FMath::Max(1, FMath::RoundToInt(RequestedSteps));
		double StepId = 0.0;
		Request->TryGetNumberField(TEXT("id"), StepId);
		StepRequestId = static_cast<int32>(StepId);
		bStepPending = true;
		bOutDeferred = true;
		return true;
	}

	if (Operation == TEXT("get_sensors"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		OutResult->SetNumberField(TEXT("sim_time"), Sim->GetSimTimeSeconds());

		// Only what the robot could know. Ground truth is a separate operation
		// so that autonomy cannot reach it by accident.
		if (const URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
		{
			OutResult->SetObjectField(TEXT("odometry"), ApiMakePose(Odometry->GetEstimatedPose()));
			OutResult->SetNumberField(TEXT("travelled_m"), Odometry->GetTravelledM());
		}

		if (const URoboImuComponent* Imu = Vehicle->FindComponentByClass<URoboImuComponent>())
		{
			const FImuSample Sample = Imu->GetLatestSample();

			TSharedRef<FJsonObject> ImuObject = MakeShared<FJsonObject>();
			ImuObject->SetArrayField(TEXT("accel"), ApiMakeVector(Sample.AccelerationMS2));
			ImuObject->SetArrayField(TEXT("gyro"), ApiMakeVector(Sample.AngularVelocityRadS));
			ImuObject->SetNumberField(TEXT("time"), Sample.TimeSeconds);

			OutResult->SetObjectField(TEXT("imu"), ImuObject);
		}

		// Lidar is by far the largest part of a reply, so a client can decline
		// it or thin it out.
		bool bIncludeLidar = true;
		Request->TryGetBoolField(TEXT("include_lidar"), bIncludeLidar);

		double RequestedStride = 1.0;
		Request->TryGetNumberField(TEXT("lidar_stride"), RequestedStride);
		const int32 Stride = FMath::Max(1, FMath::RoundToInt(RequestedStride));

		const URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>();
		if (bIncludeLidar && Lidar != nullptr)
		{
			const FLidarScan& Scan = Lidar->GetLatestScan();

			TArray<TSharedPtr<FJsonValue>> Ranges;
			Ranges.Reserve(Scan.RangesM.Num() / Stride + 1);

			for (int32 Index = 0; Index < Scan.RangesM.Num(); Index += Stride)
			{
				const double RangeM = Scan.RangesM[Index];

				// JSON has no infinity, and "no return" must stay distinct from
				// "a reading at maximum range". Null is the natural encoding,
				// and it arrives in Python as None.
				if (RoboLidar::HasReturn(RangeM))
				{
					Ranges.Add(MakeShared<FJsonValueNumber>(RangeM));
				}
				else
				{
					Ranges.Add(MakeShared<FJsonValueNull>());
				}
			}

			TSharedRef<FJsonObject> ScanObject = MakeShared<FJsonObject>();
			ScanObject->SetNumberField(TEXT("angle_min"), Scan.AngleMinRad);

			// Thinning the beams widens the gap between the ones that remain.
			// Reporting the original increment would misplace every point.
			ScanObject->SetNumberField(TEXT("angle_increment"), Scan.AngleIncrementRad * Stride);

			ScanObject->SetNumberField(TEXT("range_min"), Scan.RangeMinM);
			ScanObject->SetNumberField(TEXT("range_max"), Scan.RangeMaxM);
			ScanObject->SetNumberField(TEXT("time"), Scan.TimeSeconds);
			ScanObject->SetArrayField(TEXT("ranges"), Ranges);

			OutResult->SetObjectField(TEXT("lidar"), ScanObject);
		}

		return true;
	}

	if (Operation == TEXT("get_ground_truth"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		OutResult->SetNumberField(TEXT("sim_time"), Sim->GetSimTimeSeconds());

		// What the simulation knows and the robot does not. This is the oracle
		// a test judges against, and the thing autonomy must never read.
		if (const URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
		{
			OutResult->SetObjectField(TEXT("pose"), ApiMakePose(Odometry->GetTruePose()));
			OutResult->SetNumberField(TEXT("odometry_error_m"), Odometry->GetPositionErrorM());
			OutResult->SetNumberField(TEXT("heading_error_rad"), Odometry->GetHeadingErrorRad());
		}

		OutResult->SetNumberField(TEXT("speed_ms"), Vehicle->GetSpeedMS());
		OutResult->SetArrayField(TEXT("velocity"), ApiMakeVector(Vehicle->GetLinearVelocityMS()));

		return true;
	}

	if (Operation == TEXT("load_scenario"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		const TSharedPtr<FJsonObject>* Scenario = nullptr;
		if (!Request->TryGetObjectField(TEXT("scenario"), Scenario))
		{
			OutError = TEXT("load_scenario needs a 'scenario' object");
			return false;
		}

		return LoadScenario(Vehicle, *Scenario, OutResult, OutError);
	}

	if (Operation == TEXT("set_goal"))
	{
		if (Mission == nullptr)
		{
			OutError = TEXT("mission subsystem unavailable");
			return false;
		}

		FMissionGoal Goal;
		if (!Request->TryGetNumberField(TEXT("x"), Goal.X) || !Request->TryGetNumberField(TEXT("y"), Goal.Y))
		{
			OutError = TEXT("set_goal needs x and y");
			return false;
		}

		Request->TryGetNumberField(TEXT("tolerance_m"), Goal.ToleranceM);
		if (Goal.ToleranceM <= 0.0)
		{
			OutError = TEXT("tolerance_m must be positive");
			return false;
		}

		Mission->SetGoal(Goal);
		OutResult->SetStringField(TEXT("status"), ApiMissionStatusName(Mission->GetStatus()));
		return true;
	}

	if (Operation == TEXT("clear_goal"))
	{
		if (Mission != nullptr)
		{
			Mission->ClearGoal();
		}

		OutResult->SetStringField(TEXT("status"), TEXT("none"));
		return true;
	}

	if (Operation == TEXT("set_lidar"))
	{
		if (Vehicle == nullptr)
		{
			OutError = TEXT("no robot in the world");
			return false;
		}

		URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>();
		if (Lidar == nullptr)
		{
			OutError = TEXT("the robot has no lidar");
			return false;
		}

		// Every field is optional, so a request with none simply reads the
		// current settings. That lets a client note them and put them back.
		double FogEchoFraction = 0.0;
		if (Request->TryGetNumberField(TEXT("fog_echo_fraction"), FogEchoFraction))
		{
			if (FogEchoFraction < 0.0 || FogEchoFraction > 1.0)
			{
				OutError = TEXT("fog_echo_fraction must be between 0 and 1");
				return false;
			}

			Lidar->FogEchoFraction = FogEchoFraction;
		}

		OutResult->SetNumberField(TEXT("fog_echo_fraction"), Lidar->FogEchoFraction);
		return true;
	}
	
	OutError = FString::Printf(TEXT("unknown op '%s'"), *Operation);
	return false;
}

void URoboApiSubsystem::SendResponse(const TSharedRef<FJsonObject>& Response)
{
	if (ClientSocket == nullptr)
	{
		return;
	}
	
	FString Serialised;
	// Condensed, not pretty: pretty-printed JSON contains newlines, and the
	// newline is what separates one message from the next.
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Serialised);
	FJsonSerializer::Serialize(Response, Writer);
	Serialised.AppendChar(LineTerminator);
	
	const FTCHARToUTF8 Utf8(*Serialised);
	int32 BytesSent = 0;
	ClientSocket->Send(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent);
}

ARoboVehicle* URoboApiSubsystem::FindVehicle()
{
	if (CachedVehicle.IsValid())
	{
		return CachedVehicle.Get();
	}

	for (TActorIterator<ARoboVehicle> It(GetWorld()); It; ++It)
	{
		CachedVehicle = *It;

		// Remember where it started, so reset has somewhere to go back to.
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

void URoboApiSubsystem::CompletePendingStep()
{
	if (!bStepPending)
	{
		return;
	}

	const URoboSimSubsystem* Sim = GetWorld()->GetSubsystem<URoboSimSubsystem>();
	if (Sim == nullptr || Sim->GetStepCount() < StepTargetCount)
	{
		return;
	}

	bStepPending = false;

	TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetNumberField(TEXT("id"), StepRequestId);
	Response->SetBoolField(TEXT("ok"), true);

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("sim_time"), Sim->GetSimTimeSeconds());
	Result->SetNumberField(TEXT("step"), static_cast<double>(Sim->GetStepCount()));
	Response->SetObjectField(TEXT("result"), Result);

	SendResponse(Response);
}

void URoboApiSubsystem::ResetRobotTo(ARoboVehicle* Vehicle, const FVector& LocationUU, const FRotator& Rotation)
{
	Vehicle->SetActorLocationAndRotation(LocationUU, Rotation, false, nullptr,
		ETeleportType::TeleportPhysics);

	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()))
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	// After the teleport, so the estimates start from where the robot now is.
	if (URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
	{
		Odometry->ResetOdometry();
	}

	if (URoboImuComponent* Imu = Vehicle->FindComponentByClass<URoboImuComponent>())
	{
		Imu->ResetImu();
	}
}

double URoboApiSubsystem::GroundHeightUU(double XUU, double YUU) const
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

void URoboApiSubsystem::ClearScenario()
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

bool URoboApiSubsystem::LoadScenario(ARoboVehicle* Vehicle, const TSharedPtr<FJsonObject>& Scenario,
	TSharedRef<FJsonObject>& OutResult, FString& OutError)
{
	// Refuse anything not written for this version of the format, rather
	// than half-understanding it.
	double SchemaVersion = 0.0;
	if (!Scenario->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
		|| FMath::RoundToInt(SchemaVersion) != 1)
	{
		OutError = TEXT("missing or unsupported schema_version (expected 1)");
		return false;
	}

	const TSharedPtr<FJsonObject>* Start = nullptr;
	if (!Scenario->TryGetObjectField(TEXT("start"), Start))
	{
		OutError = TEXT("scenario has no 'start'");
		return false;
	}

	// Validate everything before changing anything, so a malformed scenario
	// leaves the previous world intact instead of half-replaced.
	const TArray<TSharedPtr<FJsonValue>>* Obstacles = nullptr;
	Scenario->TryGetArrayField(TEXT("obstacles"), Obstacles);

	if (Obstacles != nullptr)
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
	const bool bHasVisibility = Scenario->HasField(TEXT("visibility_m"));

	if (bHasVisibility
		&& (!Scenario->TryGetNumberField(TEXT("visibility_m"), VisibilityM) || VisibilityM <= 0.0))
	{
		OutError = TEXT("visibility_m must be a positive number");
		return false;
	}
	
	ClearScenario();

	// Obstacles. Positions and sizes are REP-103 metres, converted at this
	// boundary like everything else.
	if (Obstacles != nullptr)
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

	// The robot's starting pose.
	double StartX = 0.0, StartY = 0.0, StartYaw = 0.0;
	(*Start)->TryGetNumberField(TEXT("x"), StartX);
	(*Start)->TryGetNumberField(TEXT("y"), StartY);
	(*Start)->TryGetNumberField(TEXT("yaw"), StartYaw);

	const FVector StartUU = RoboFrames::PositionToUe(FVector(StartX, StartY, 0.0));
	ResetRobotTo(Vehicle,
		FVector(StartUU.X, StartUU.Y, Vehicle->GetActorLocation().Z),
		FRotator(0.0, RoboFrames::YawToUe(StartYaw), 0.0));

	// Draw the goal, if there is one, so a person watching can see what the
	// robot is meant to reach. Must come after the reset, which clears
	// persistent debug lines.
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

		if (URoboMissionSubsystem* Mission = GetWorld()->GetSubsystem<URoboMissionSubsystem>())
		{
			FMissionGoal MissionGoal;
			MissionGoal.X = GoalX;
			MissionGoal.Y = GoalY;
			MissionGoal.ToleranceM = ToleranceM;
			Mission->SetGoal(MissionGoal);
		}
	}
	else if (URoboMissionSubsystem* Mission = GetWorld()->GetSubsystem<URoboMissionSubsystem>())
	{
		// A scenario without a goal must not inherit the previous scenario's.
		Mission->ClearGoal();
	}

	// A scenario without fog must not inherit the previous one's, exactly as
	// with the goal.
	if (URoboWeatherSubsystem* Weather = GetWorld()->GetSubsystem<URoboWeatherSubsystem>())
	{
		if (bHasVisibility)
		{
			Weather->SetFog(VisibilityM);
		}
		else
		{
			Weather->SetClear();
		}
	}
	
	FString Name = TEXT("unnamed");
	Scenario->TryGetStringField(TEXT("name"), Name);

	OutResult->SetStringField(TEXT("name"), Name);
	OutResult->SetNumberField(TEXT("obstacles"), ScenarioActors.Num());
	return true;
}