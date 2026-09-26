#include "RoboVehicle.h"

#include "RoboDiffDrive.h"
#include "RoboSim.h"
#include "RoboWheel.h"
#include "RoboFrames.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ARoboVehicle::ARoboVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	
	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	SetRootComponent(Chassis);

	// Engine primitive: a 100 cm cube, scaled to a 60 x 40 x 20 cm body.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh (TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Chassis->SetStaticMesh(CubeMesh.Object);
	}

	Chassis->SetRelativeScale3D(FVector(0.6, 0.4, 0.2));
	Chassis->SetSimulatePhysics(true);
	Chassis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chassis->SetCollisionProfileName(TEXT("PhysicsActor"));
	
	// Differential drive: two driven wheels on a central axle, plus a caster
	// at each end for balance. Casters roll freely, so they do not resist the
	// turn the way four driven corner wheels would.
	const double HalfTrackUU = 20.0;
	const double CasterOffsetUU = 22.0;
	const double MountHeightUU = -10.0;

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

	UE_LOG(LogRoboSim, Log, TEXT("%s ready: %.1f kg"), *GetName(), MassKg);
}

void ARoboVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double NowSeconds = GetWorld()->GetTimeSeconds();

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

	if (bDrawDebugState)
	{
		DrawDebugString(GetWorld(), FVector(0.0, 0.0, 40.0),
			FString::Printf(TEXT("%s  v=%.2f m/s  w=%.2f rad/s"),
				*UEnum::GetDisplayValueAsText(Command.Source).ToString(),
				Command.Linear, Command.Angular),
			this, FColor::White, 0.0f, true);
	}

	const RoboDiffDrive::FWheelSpeeds WheelSpeeds = RoboDiffDrive::ToWheelSpeeds(
		RoboDiffDrive::FBodyVelocity{ Command.Linear, Command.Angular }, GetWheelSeparationM());

	const double LoadPerWheelKg = MassKg / FMath::Max(Wheels.Num(), 1);

	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		URoboWheel* Wheel = Wheels[Index];
		Wheel->UpdateSuspension(Chassis, DeltaSeconds);

		// Wheels[0] is the left driven wheel, Wheels[1] the right; the rest are casters.
		const double TargetSpeedMS = (Index == 0) ? WheelSpeeds.Left : WheelSpeeds.Right;
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

	// Measured from the wheels themselves rather than stored separately, so
	// moving a wheel in the editor cannot disagree with the kinematics.
	const double LeftY = Wheels[0]->GetRelativeLocation().Y;
	const double RightY = Wheels[1]->GetRelativeLocation().Y;
	return FMath::Abs(RightY - LeftY) * RoboFrames::MetersPerUU;
}