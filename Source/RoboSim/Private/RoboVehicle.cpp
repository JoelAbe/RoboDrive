#include "RoboVehicle.h"

#include "RoboCameraComponent.h"
#include "RoboDiffDrive.h"
#include "RoboSim.h"
#include "RoboWheel.h"
#include "RoboFrames.h"
#include "RoboImuComponent.h"
#include "RoboLidarComponent.h"
#include "RoboOdometryComponent.h"
#include "RoboRecorderComponent.h"
#include "RoboSimSubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"

ARoboVehicle::ARoboVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// The rigid body is a box primitive at real size, NOT a scaled mesh.
	// Anything attached to a scaled component has its offsets scaled too, which
	// silently shrinks the wheel layout and disagrees with the kinematics.
	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	SetRootComponent(Chassis);
	Chassis->SetBoxExtent(FVector(30.0, 20.0, 10.0));   // 60 x 40 x 20 cm
	Chassis->SetCollisionProfileName(TEXT("PhysicsActor"));
	Chassis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chassis->SetSimulatePhysics(true);

	// Visual shell, no collision of its own.
	ChassisMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChassisMesh"));
	ChassisMesh->SetupAttachment(Chassis);
	ChassisMesh->SetRelativeScale3D(FVector(0.6, 0.4, 0.2));
	ChassisMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		ChassisMesh->SetStaticMesh(CubeMesh.Object);
	}
	
	struct FWheelSetup
	{
		const TCHAR* Name;
		bool bDriven;
	};

	// Order matters: Wheels[0] is the left driven wheel and Wheels[1] the right.
	const FWheelSetup Setups[] = {
		{ TEXT("WheelLeft"),   true  },
		{ TEXT("WheelRight"),  true  },
		{ TEXT("CasterFront"), false },
		{ TEXT("CasterRear"),  false }
	};

	for (const FWheelSetup& Setup : Setups)
	{
		URoboWheel* Wheel = CreateDefaultSubobject<URoboWheel>(Setup.Name);
		Wheel->SetupAttachment(Chassis);
		Wheel->bIsDriven = Setup.bDriven;
		Wheels.Add(Wheel);
	}

	Lidar = CreateDefaultSubobject<URoboLidarComponent>(TEXT("Lidar"));
	Lidar->SetupAttachment(Chassis);
	Lidar->SetRelativeLocation(FVector(0.0, 0.0, 15.0));   // on top of the chassis

	Camera2D = CreateDefaultSubobject<URoboCameraComponent>(TEXT("RoboCamera"));
	Camera2D->SetupAttachment(Chassis);
	Camera2D->SetRelativeLocation(FVector(25.0, 0.0, 12.0));   // front of the chassis, facing forward
	
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Chassis);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->SocketOffset = FVector(0.0, 0.0, 60.0);
	CameraBoom->SetRelativeRotation(FRotator(-25.0, 0.0, 0.0));

	// Follow the robot's heading, but ignore chassis pitch and roll. The
	// suspension leans constantly, and a view that leans with it is unwatchable
	// and makes it impossible to judge whether the robot is level.
	CameraBoom->bInheritYaw = true;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bUsePawnControlRotation = false;

	// Smooth out the physics jitter so the view is readable while driving.
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.0f;
	CameraBoom->bEnableCameraRotationLag = true;
	CameraBoom->CameraRotationLagSpeed = 8.0f;

	// Let the camera pass through geometry rather than snapping in against it.
	// A sim view should stay predictable, not behave like a game camera.
	CameraBoom->bDoCollisionTest = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	Odometry = CreateDefaultSubobject<URoboOdometryComponent>(TEXT("Odometry"));
	Imu = CreateDefaultSubobject<URoboImuComponent>(TEXT("Imu"));

	Recorder = CreateDefaultSubobject<URoboRecorderComponent>(TEXT("Recorder"));
}

void ARoboVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Re-runs whenever a property changes in the editor, so moving the wheels
	// is a number change rather than a recompile.
	ApplyWheelLayout();
}

void ARoboVehicle::ApplyWheelLayout()
{
	if (Wheels.Num() < 4)
	{
		return;
	}

	const double HalfTrackUU = 0.5 * WheelTrackM * RoboFrames::UUPerMeter;
	const double CasterUU = CasterOffsetM * RoboFrames::UUPerMeter;
	const double HeightUU = WheelMountHeightM * RoboFrames::UUPerMeter;

	// Unreal's +Y is to the right, so the left wheel takes the negative offset.
	Wheels[0]->SetRelativeLocation(FVector(0.0, -HalfTrackUU, HeightUU));
	Wheels[1]->SetRelativeLocation(FVector(0.0,  HalfTrackUU, HeightUU));
	Wheels[2]->SetRelativeLocation(FVector( CasterUU, 0.0, HeightUU));
	Wheels[3]->SetRelativeLocation(FVector(-CasterUU, 0.0, HeightUU));
}

void ARoboVehicle::BeginPlay()
{
	Super::BeginPlay();

	// Override the mass the mesh volume would imply. A robot is not a solid
	// block of whatever the cube's default density is.
	Chassis->SetMassOverrideInKg(NAME_None, static_cast<float>(MassKg), true);

	// Robots carry their battery and motors low, so the centre of mass sits
	// below the geometric centre. Unreal would otherwise place it mid-box,
	// which leans the chassis enough in turns to unload the inside wheel.
	Chassis->SetCenterOfMass(CenterOfMassOffsetM * RoboFrames::UUPerMeter);

	UE_LOG(LogRoboSim, Log, TEXT("%s ready: %.1f kg"), *GetName(), MassKg);
}

void ARoboVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Sim == nullptr)
	{
		return;
	}
	const double NowSeconds = Sim->GetSimTimeSeconds();

	if (bUseDebugCommand)
	{
		FDriveCommand Debug;
		Debug.Linear = DebugLinearMS;
		Debug.Angular = DebugAngularRadS;
		Debug.TimeSeconds = NowSeconds;
		Debug.Source = EDriveSource::External;
		Arbiter.Submit(Debug);
	}

	// Whoever wins arbitration drives. Nothing fresh means a stop.
	const FDriveCommand Command = Arbiter.Evaluate(NowSeconds);

	LastCommand = Command;

	if (bDrawDebugState)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 40.0),
			FString::Printf(TEXT("%s  v=%.2f  w=%.2f\nt=%.2fs  step %lld  RTF %.2f"),
				*UEnum::GetDisplayValueAsText(Command.Source).ToString(),
				Command.Linear, Command.Angular,
				Sim->GetSimTimeSeconds(), Sim->GetStepCount(), Sim->GetRealTimeFactor()),
			this, FColor::White, 0.0f, true);

		DrawDebugSphere(GetWorld(), Chassis->GetCenterOfMass(), 4.0f, 8, FColor::Orange, false, 0.0f, 0, 1.0f);
	}

	LastWheelSpeeds = RoboDiffDrive::ToWheelSpeeds(
	RoboDiffDrive::FBodyVelocity{ Command.Linear, Command.Angular }, GetWheelSeparationM());

	const double LoadPerWheelKg = MassKg / FMath::Max(Wheels.Num(), 1);

	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		URoboWheel* Wheel = Wheels[Index];
		Wheel->UpdateSuspension(Chassis, DeltaSeconds);

		// Wheels[0] is the left driven wheel, Wheels[1] the right; the rest are casters.
		const double TargetSpeedMS = Wheel->bIsDriven ? ((Index == 0) ? LastWheelSpeeds.Left : LastWheelSpeeds.Right): 0.0;
		Wheel->UpdateDrive(Chassis, TargetSpeedMS, LoadPerWheelKg, DeltaSeconds);
	}
}

void ARoboVehicle::SubmitDriveCommand(const FDriveCommand& Command)
{
	Arbiter.Submit(Command);
}

EDriveSource ARoboVehicle::GetActiveDriveSource() const
{
	return Arbiter.GetActiveSource();
}

double ARoboVehicle::GetWheelSeparationM() const
{
	if (Wheels.Num() < 2)
	{
		return 0.0;
	}

	// World distance, so a scaled parent or a nudged component can never make
	// the kinematics disagree with the physical robot.
	const double SeparationUU = FVector::Dist(Wheels[0]->GetComponentLocation(), Wheels[1]->GetComponentLocation());
	return SeparationUU * RoboFrames::MetersPerUU;
}

double ARoboVehicle::GetWheelRadiusM() const
{
	return Wheels.Num() > 0 ? Wheels[0]->WheelRadiusM : 0.0;
}

double ARoboVehicle::GetSpeedMS() const
{
	return Chassis->GetPhysicsLinearVelocity().Size() * RoboFrames::MetersPerUU;
}

FVector ARoboVehicle::GetLinearVelocityMS() const
{
	return RoboFrames::LinearVelocityToRos(Chassis->GetPhysicsLinearVelocity());
}

FVector ARoboVehicle::GetAngularVelocityRadS() const
{
	return RoboFrames::AngularVelocityToRos(Chassis->GetPhysicsAngularVelocityInDegrees());
}

FQuat ARoboVehicle::GetRotationRos() const
{
	return RoboFrames::RotationToRos(GetActorQuat());
}