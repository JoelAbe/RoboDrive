#include "RoboVehicle.h"

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
	
	// 40 cm between left and right wheels, 40 cm between front and rear,
	// mounted at the bottom face of the 20 cm tall chassis.
	const double HalfTrackUU = 20.0;
	const double HalfBaseUU = 20.0;
	const double MountHeightUU = -10.0;

	const TCHAR* WheelNames[] = { TEXT("WheelFL"), TEXT("WheelFR"), TEXT("WheelRL"), TEXT("WheelRR") };
	const FVector WheelOffsets[] = {
		FVector( HalfBaseUU, -HalfTrackUU, MountHeightUU),   // front left  (-Y is left in Unreal)
		FVector( HalfBaseUU,  HalfTrackUU, MountHeightUU),   // front right
		FVector(-HalfBaseUU, -HalfTrackUU, MountHeightUU),   // rear left
		FVector(-HalfBaseUU,  HalfTrackUU, MountHeightUU)    // rear right
	};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WheelNames); ++Index)
	{
		URoboWheel* Wheel = CreateDefaultSubobject<URoboWheel>(WheelNames[Index]);
		Wheel->SetupAttachment(Chassis);
		Wheel->SetRelativeLocation(WheelOffsets[Index]);
		Wheels.Add(Wheel);
	}
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

	for (URoboWheel* Wheel : Wheels)
	{
		Wheel->UpdateSuspension(Chassis, DeltaSeconds);
	}
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